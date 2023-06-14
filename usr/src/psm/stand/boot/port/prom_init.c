/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2017 Hayashi Naoyuki
 */

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/systm.h>
#include <sys/dklabel.h>
#include <sys/vtoc.h>
#include <sys/efi_partition.h>
#include "boot_plat.h"
#include "prom_dev.h"
#include "ramdisk.h"
#ifndef PATH_MAX
#define	PATH_MAX	1024
#endif

#include <sys/dktp/fdisk.h>

extern unsigned long unix_startblk;
extern size_t unix_numblks;

#define	PROMIF_CLNTNAMELEN	16
char	promif_clntname[PROMIF_CLNTNAMELEN];

#define	PROM_MAXDEVS 8

struct prom_ctrlblk {
	int opened;
	int fd;
	const struct prom_dev *dev;
};

static struct prom_ctrlblk prom_cb[PROM_MAXDEVS];
static const struct prom_dev *prom_devs[PROM_MAXDEVS];

int
prom_register(const struct prom_dev *dev)
{
	for (int i = 0; i < sizeof (prom_devs) / sizeof (prom_devs[0]); i++) {
		if (prom_devs[i] == NULL) {
			prom_devs[i] = dev;
			return (0);
		}
	}
	return (-1);
}

static ihandle_t stdin_handle = -1;
static ihandle_t stdout_handle = -1;

void
prom_init(char *pgmname, void *cookie)
{
	strncpy(promif_clntname, pgmname, PROMIF_CLNTNAMELEN - 1);
	promif_clntname[PROMIF_CLNTNAMELEN - 1] = '\0';

	stdin_handle = prom_open("stdin");
	stdout_handle = prom_open("stdout");
}

ihandle_t
prom_stdin_ihandle(void)
{
	return (stdin_handle);
}

ihandle_t
prom_stdout_ihandle(void)
{
	return (stdout_handle);
}

static void
hex(char **buf, int len, uint32_t val)
{
	static const char *hexstr = "0123456789abcdef";
	char *p = *buf;
	int i;

	for (i = len - 1; i >= 0; i--) {
		p[i] = hexstr[val & 0xf];
		val >>= 4;
	}
	*buf = p + len;
}

static void
prom_print_uuid(const char *tag, const struct uuid *u)
{
	char buf[37];
	char *p = buf;

	hex(&p, 8, u->time_low);
	*p++ = '-';
	hex(&p, 4, u->time_mid);
	*p++ = '-';
	hex(&p, 4, u->time_hi_and_version);
	*p++ = '-';
	hex(&p, 2, u->clock_seq_hi_and_reserved);
	hex(&p, 2, u->clock_seq_low);
	*p++ = '-';
	hex(&p, 2, u->node_addr[0]);
	hex(&p, 2, u->node_addr[1]);
	hex(&p, 2, u->node_addr[2]);
	hex(&p, 2, u->node_addr[3]);
	hex(&p, 2, u->node_addr[4]);
	hex(&p, 2, u->node_addr[5]);
	*p++ = '\0';

	prom_printf("%s: %s\n", tag, buf);
}

int
prom_open(char *path)
{
	struct mboot bootblk;
	struct ipart *iparts;
	int index, fd, i;
	ssize_t s;
	boolean_t stdinout = strcmp(path, "stdout") == 0 ||
	    strcmp(path, "stdin") == 0;

	if (!stdinout)
		prom_printf("prom_open(%s)\n", path);

	for (index = 0;
	    index < sizeof (prom_cb) / sizeof (prom_cb[0]); index++) {
		if (prom_cb[index].opened == 0)
			break;
	}
	if (index == sizeof (prom_cb) / sizeof (prom_cb[0])) {
		prom_printf("Too many open files\n");
		return (0);
	}

	for (i = 0; i < sizeof (prom_devs) / sizeof (prom_devs[0]); i++) {
		if (prom_devs[i] && prom_devs[i]->match(path))
			break;
	}
	if (i == sizeof (prom_devs) / sizeof (prom_devs[0])) {
		if (!stdinout)
			prom_printf("No matching prom device\n");
		return (0);
	}

	prom_cb[index].dev = prom_devs[i];
	prom_cb[index].fd = 0;
	if (prom_cb[index].dev->open)
		prom_cb[index].fd = prom_cb[index].dev->open(path);

	if (prom_cb[index].fd < 0)
		return (0);

	prom_cb[index].opened = 1;
	fd = index + 1;

	unix_startblk = 0;
	unix_numblks = 0;

	if (stdinout)
		return (fd);

	if (is_netdev(path))
		return (fd);

	if (strcmp(prom_bootpath(), path) != 0)
		return (fd);

	if (prom_read(fd, (caddr_t)&bootblk, 0x200, 0, 0) != 0x200) {
		prom_printf("failed to read bootblk\n");
		return (fd);
	}

	if (bootblk.signature != MBB_MAGIC) {
		prom_printf("bootblk signature != magic: %x != %x\n",
		    bootblk.signature, MBB_MAGIC);
		return (fd);
	}

	iparts = (struct ipart *)(uintptr_t)&bootblk.parts[0];
	for (i = 0; i < FD_NUMPART; i++) {
		switch (iparts[i].systid) {
		case SUNIXOS2: {
			uint64_t slice_start = iparts[i].relsect;
			struct dk_label dkl;

			s = prom_read(fd, (caddr_t)&dkl, 0x200,
			    slice_start + DK_LABEL_LOC, 0);
			if (s != 0x200) {
				prom_printf("short label read: %zd\n", s);
				break;
			}
			for (int j = 0; j < NDKMAP; j++) {
				if (dkl.dkl_vtoc.v_part[j].p_tag != V_ROOT)
					continue;

				unix_startblk = slice_start +
				    dkl.dkl_vtoc.v_part[j].p_start;
				unix_numblks = dkl.dkl_vtoc.v_part[j].p_size;

				prom_printf(
				    "Booting from slice %d (ROOT) (%lx+%x)\n",
				    j, unix_startblk, unix_numblks);
				break;
			}
			if (unix_startblk != 0)
				break;

			for (int j = 0; j < NDKMAP; j++) {
				if (dkl.dkl_vtoc.v_part[j].p_size == 0 ||
				    dkl.dkl_vtoc.v_part[j].p_tag == V_ROOT ||
				    dkl.dkl_vtoc.v_part[j].p_tag == V_BOOT) {
					continue;
				}

				unix_startblk = slice_start +
				    dkl.dkl_vtoc.v_part[j].p_start;
				unix_numblks = dkl.dkl_vtoc.v_part[j].p_size;

				prom_printf(
				    "Booting from slice %d (tag %x) (%lx+%x)\n",
				    j, dkl.dkl_vtoc.v_part[j].p_tag,
				    unix_startblk, unix_numblks);
				break;
			}
			break;
		}
		case EFI_PMBR: {
			efi_gpt_t gpt;
			const size_t block = 0x200;
			const size_t entries = block / sizeof (efi_gpe_t);
			efi_gpe_t gpeb[entries];
			static const struct uuid efi_root = EFI_ROOT;
			static const struct uuid efi_usr = EFI_USR;

			s = prom_read(fd, (caddr_t)&gpt, sizeof (gpt),
			    iparts[i].relsect, 0);
			if (s != sizeof (gpt)) {
				prom_printf("short GPT read: %zd\n", s);
				break;
			}
			if (gpt.efi_gpt_Signature != EFI_SIGNATURE) {
				prom_printf("Bad signature %lx (want %lx)\n",
				    gpt.efi_gpt_Signature, EFI_SIGNATURE);
				break;
			}
			if (gpt.efi_gpt_SizeOfPartitionEntry !=
			    sizeof (efi_gpe_t)) {
				prom_printf("Bad partition table entry size\n");
				break;
			}

			for (uint_t i = 0;
			    i < gpt.efi_gpt_NumberOfPartitionEntries; i++) {
				const efi_gpe_t *gpe = &gpeb[i % entries];
				const struct uuid *uuid;

				/* Read in more table entries if necessary */
				if ((i % entries) == 0) {
					memset(gpeb, 0, sizeof (gpeb));
					s = prom_read(fd, (caddr_t)gpeb,
					    sizeof (gpeb),
					    gpt.efi_gpt_PartitionEntryLBA +
					    (i / entries), 0);
					if (s != sizeof (gpeb)) {
						prom_printf("short partition "
						    "table read\n");
						break;
					}
				}

				uuid = (struct uuid *)
				    &gpe->efi_gpe_PartitionTypeGUID;

				if (memcmp((void *)uuid,
				    &efi_root, sizeof (efi_root)) != 0 &&
				    memcmp((void *)uuid,
				    &efi_usr, sizeof (efi_usr)) != 0) {
					continue;
				}

				unix_startblk = gpe->efi_gpe_StartingLBA;
				unix_numblks = gpe->efi_gpe_EndingLBA -
				    gpe->efi_gpe_StartingLBA + 1;
				boot_partition = i;

				prom_printf("Booting from EFI partition %d\n",
				    i);
				prom_print_uuid("Partiton GUID", uuid);
				break;
			}
		}
		}
	}

	return (fd);
}

int
prom_close(int fd)
{
	if (fd <= 0)
		return (-1);
	int index = fd - 1;
	if (prom_cb[index].opened == 0)
		return (-1);

	if (prom_cb[index].dev->close) {
		prom_cb[index].dev->close(prom_cb[index].fd);
	}
	prom_cb[index].opened = 0;

	return (0);
}

ssize_t
prom_read(ihandle_t fd, caddr_t buf, size_t len, uint_t startblk, char devtype)
{
	if (fd <= 0)
		return (-1);
	int index = fd - 1;
	if (prom_cb[index].opened == 0 || prom_cb[index].dev->read == 0)
		return (-1);
	return (prom_cb[index].dev->read(prom_cb[index].fd, buf, len,
	    startblk));
}

ssize_t
prom_write(ihandle_t fd, caddr_t buf, size_t len, uint_t startblk, char devtype)
{
	if (fd <= 0)
		return (-1);
	int index = fd - 1;
	if (prom_cb[index].opened == 0 || prom_cb[index].dev->write == 0)
		return (-1);
	return (prom_cb[index].dev->write(prom_cb[index].fd, buf, len,
	    startblk));
}

int
prom_seek(int fd, unsigned long long offset)
{
	return (offset);
}

int
prom_getmacaddr(ihandle_t fd, caddr_t ea)
{
	if (fd <= 0)
		return (-1);
	int index = fd - 1;
	if (prom_cb[index].opened == 0 || prom_cb[index].dev->getmacaddr == 0)
		return (-1);
	return (prom_cb[index].dev->getmacaddr(prom_cb[index].fd, ea));
}

boolean_t
prom_is_netdev(char *devpath)
{
	pnode_t node = prom_finddevice(devpath);
	if (node > 0) {
		int len = prom_getproplen(node, "model");
		if (len > 0) {
			char *buf = __builtin_alloca(len);
			prom_getprop(node, "model", buf);
			if (strcmp(buf, "Ethernet controller") == 0) {
				return (B_TRUE);
			}
		}
	}

	int i;
	for (i = 0; i < sizeof (prom_devs) / sizeof (prom_devs[0]); i++) {
		if (prom_devs[i] && prom_devs[i]->match(devpath))
			break;
	}
	if (i == sizeof (prom_devs) / sizeof (prom_devs[0]))
		return (B_FALSE);
	if (prom_devs[i]->getmacaddr)
		return (B_TRUE);
	return (B_FALSE);
}
