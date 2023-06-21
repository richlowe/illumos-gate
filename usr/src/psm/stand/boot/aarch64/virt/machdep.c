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
 * Copyright 2020 Hayashi Naoyuki
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/salib.h>
#include <sys/bootconf.h>
#include <sys/boot.h>
#include <sys/bootinfo.h>
#include <sys/sysmacros.h>
#include <sys/machparam.h>
#include <sys/memlist.h>
#include <sys/memlist_impl.h>
#include <sys/controlregs.h>
#include <sys/saio.h>
#include <sys/bootsyms.h>
#include <sys/fcntl.h>
#include <sys/platform.h>
#include <sys/platnames.h>
#include <alloca.h>
#include <netinet/inetutil.h>
#include <sys/bootvfs.h>
#include <sys/psci.h>
#include "ramdisk.h"
#include "boot_plat.h"
#include "virtnet.h"
#include "virtblk.h"
#include "virtio.h"
#include <libfdt.h>
#ifndef rounddown
#define	rounddown(x, y)	(((x)/(y))*(y))
#endif

extern char _BootScratch[];
extern char _RamdiskStart[];
extern char _RamdiskStart[];
extern char _RamdiskEnd[];
extern char filename[];
static struct xboot_info xboot_info;
static char zfs_bootfs[256];	/* ZFS_MAXNAMELEN */
static char zfs_boot_pool_guid[256 * 2];
static char zfs_boot_vdev_guid[256 * 2];
char v2args_buf[V2ARGS_BUF_SZ];
char *v2args = v2args_buf;
extern char *bootp_response;

extern	int (*readfile(int fd, int print))();
extern	void kmem_init(void);
extern	void *kmem_alloc(size_t, int);
extern	void kmem_free(void *, size_t);
extern	void get_boot_args(char *buf);
extern	void setup_bootops(void);
extern	struct	bootops bootops;
extern	void exitto(int (*entrypoint)());
extern	int openfile(char *filename);
extern int determine_fstype_and_mountroot(char *);
extern	ssize_t xread(int, char *, size_t);
extern	void _reset(void);
extern	void init_physmem_common(void);

void
setup_aux(void)
{
}

static void
add_iomap(pnode_t node, void *arg)
{
	if (prom_is_compatible(node, (const char *)arg)) {
		int index = 0;
		for (;;) {
			uint64_t base;
			uint64_t size;
			if (prom_get_reg_address(node, index, &base) == 0 &&
			    prom_get_reg_size(node, index, &size) == 0) {
				uint64_t addr = rounddown(base, MMU_PAGESIZE);
				uint64_t len = roundup(base + size, MMU_PAGESIZE) - addr;
				if (!memlist_find(piolistp, addr)) {
					prom_printf("add io %p %p for %s\n", addr, len, arg);
					memlist_add_span(addr, len, &piolistp);
				}
			} else {
				break;
			}
			index++;
		}
	}
}

static void
fixup_virtio(pnode_t node, void *arg)
{
	if (!prom_is_compatible(node, "virtio,mmio"))
		return;

	uint64_t reg;
	if (prom_get_reg_address(node, 0, &reg) != 0)
		return;

	// check virt
	if (VIRTIO_MMIO_MAGIC_VALUE(reg) != 0x74726976)
		return;
	// check QEMU
	if (VIRTIO_MMIO_VENDOR_ID(reg) != 0x554d4551)
		return;
	// check Legacy
	if (VIRTIO_MMIO_VERSION(reg) != 1)
		return;

	const char *name = NULL;
	if (VIRTIO_MMIO_DEVICE_ID(reg) == 1)
		name = "virtio-net";
	else if (VIRTIO_MMIO_DEVICE_ID(reg) == 2)
		name = "virtio-blk";

	if (name == NULL)
		return;

	int len;
	len = prom_getproplen(node, "compatible");

	char *prop = __builtin_alloca(strlen(name) + 1 + len);
	strcpy(prop, name);
	prom_getprop(node, "compatible", prop + strlen(name) + 1);
	prom_setprop(node, "compatible", prop, strlen(name) + 1 + len);
}

static void
fixup_cpu(pnode_t node, void *arg)
{
	int len;
	len = prom_getproplen(node, "device_type");
	if (len <= 0)
		return;
	char *prop = __builtin_alloca(len + 1);
	memset(prop, 0, len + 1);
	prom_getprop(node, "device_type", prop);
	if (strcmp(prop, "cpu") != 0)
		return;

	len = prom_getproplen(node, "enable-method");
	if (len > 0)
		return;

	char psci[] = "psci";
	prom_setprop(node, "enable-method", psci, sizeof(psci));
}

/*
 * Remove -D and the path from the boot args, look for the next "-" char
*/
void
fix_boot_args(char *str)
{
	char *s2;
	char *s3;
	int i;

	s2 = strstr(str, "-D");
	if (s2 != NULL) {
		s3 = s2 + 2;
		for (i = 0; s3[i] != 0 && s3[i] != '-'; i++)
			;
		s3 = s3 + i;
		memmove(s2, s3, strlen(s3) + 1);
	}
}


void
init_physmem(void)
{
	init_physmem_common();
}
void
init_iolist(void)
{
	prom_walk(add_iomap, "arm,pl011");
	prom_walk(add_iomap, "arm,pl031");
	prom_walk(add_iomap, "arm,cortex-a15-gic");
	prom_walk(add_iomap, "virtio,mmio");
	prom_walk(fixup_virtio, NULL);
}
void exitto(int (*entrypoint)())
{
	prom_walk(fixup_cpu, NULL);
	for (struct memlist *ml = plinearlistp; ml != NULL; ml = ml->ml_next) {
		uintptr_t pa = ml->ml_address;
		uintptr_t sz = ml->ml_size;
		map_phys(PTE_UXN | PTE_PXN | PTE_AF | PTE_SH_INNER | PTE_AP_KRWUNA | PTE_ATTR_NORMEM, (caddr_t)(SEGKPM_BASE + pa), pa, sz);
	}
	for (struct memlist *ml = piolistp; ml != NULL; ml = ml->ml_next) {
		uintptr_t pa = ml->ml_address;
		uintptr_t sz = ml->ml_size;
		map_phys(PTE_UXN | PTE_PXN | PTE_AF | PTE_SH_INNER | PTE_AP_KRWUNA | PTE_ATTR_DEVICE, (caddr_t)(SEGKPM_BASE + pa), pa, sz);
	}

	uint64_t v;
	char *str;

	v = htonll((uint64_t)_RamdiskStart);
	prom_setprop(prom_chosennode(), "ramdisk_start", (caddr_t)&v, sizeof(v));
	v = htonll((uint64_t)_RamdiskEnd);
	prom_setprop(prom_chosennode(), "ramdisk_end", (caddr_t)&v, sizeof(v));
	v = htonll((uint64_t)pfreelistp);
	prom_setprop(prom_chosennode(), "phys-avail", (caddr_t)&v, sizeof(v));
	v = htonll((uint64_t)pinstalledp);
	prom_setprop(prom_chosennode(), "phys-installed", (caddr_t)&v, sizeof(v));
	v = htonll((uint64_t)pscratchlistp);
	prom_setprop(prom_chosennode(), "boot-scratch", (caddr_t)&v, sizeof(v));
	if (bootp_response) {
		uint_t blen = strlen(bootp_response) / 2;
		char *pktbuf = __builtin_alloca(blen);
		hexascii_to_octet(bootp_response, blen * 2, pktbuf, &blen);
		prom_setprop(prom_chosennode(), "bootp-response", pktbuf, blen);
	} else {
	}
	str = prom_bootargs(); //pass the bootargs from u-boot to illumos
	fix_boot_args(str);
	prom_setprop(prom_chosennode(), "boot-args", (caddr_t)str, strlen(str) + 1);
	prom_setprop(prom_chosennode(), "bootargs", (caddr_t)str, strlen(str) + 1);
	str = filename;
	prom_setprop(prom_chosennode(), "whoami", (caddr_t)str, strlen(str) + 1);
	str = filename;
	prom_setprop(prom_chosennode(), "boot-file", (caddr_t)str, strlen(str) + 1);

	if (prom_getproplen(prom_chosennode(), "impl-arch-name") < 0) {
		str = "armv8";
		prom_setprop(prom_chosennode(), "impl-arch-name", (caddr_t)str, strlen(str) + 1);
	}

	str = get_mfg_name();
	prom_setprop(prom_chosennode(), "mfg-name", (caddr_t)str, strlen(str) + 1);
	str = "115200,8,n,1,-";
	prom_setprop(prom_chosennode(), "ttya-mode", (caddr_t)str, strlen(str) + 1);
	prom_setprop(prom_chosennode(), "ttyb-mode", (caddr_t)str, strlen(str) + 1);

	str = "/pl011@9000000:115200n8";	// qemu
	prom_setprop(prom_chosennode(), "stdout-path", (caddr_t)str, strlen(str) + 1);

	xboot_info.bi_fdt = SEGKPM_BASE + (uint64_t)get_fdtp();
	entrypoint(&xboot_info);
}

extern void get_boot_zpool(char *);
extern void get_boot_zpool_guid(char *);
extern void get_boot_vdev_guid(char *);

static void
set_zfs_bootfs(void)
{
	get_boot_zpool(zfs_bootfs);
	prom_setprop(prom_chosennode(), "zfs-bootfs", (caddr_t)zfs_bootfs, strlen(zfs_bootfs) + 1);
	prom_printf("zfs-bootfs=%s\n", zfs_bootfs);

	get_boot_zpool_guid(zfs_boot_pool_guid);
	prom_setprop(prom_chosennode(), "zfs-bootpool",
	    (caddr_t)zfs_boot_pool_guid, strlen(zfs_boot_pool_guid) + 1);
	prom_printf("zfs-bootpool=%s\n", zfs_boot_pool_guid);

	get_boot_vdev_guid(zfs_boot_vdev_guid);
	prom_setprop(prom_chosennode(), "zfs-bootvdev",
	    (caddr_t)zfs_boot_vdev_guid, strlen(zfs_boot_vdev_guid) + 1);
	prom_printf("zfs-bootvdev=%s\n", zfs_boot_vdev_guid);
}

static void
set_rootfs(char *bpath, char *fstype)
{
	char *str;
	if (strcmp(fstype, "nfs") == 0) {
		str = "nfsdyn";
		prom_setprop(prom_chosennode(), "fstype", (caddr_t)str, strlen(str) + 1);
	} else {
		str = fstype;
		prom_setprop(prom_chosennode(), "fstype", (caddr_t)str, strlen(str) + 1);
		/*
		 * We could set bootpath to the correct value here, but then
		 * when we go to mount root we trust the config in the label
		 * on that device, which will reflect the device where the
		 * disk was built.  Because that config is correct we'll then
		 * not do any more to search for the pool, and fail to mount
		 * it always.
		 *
		 * Instead, we do nothing, and let the system search for the
		 * pool based on using having set the GUIDs of both pool and
		 * vdev.  This is how things work on x86, where there is no
		 * notion of device paths.
		 */
	}
}

void
load_ramdisk(void *virt, const char *name)
{
	static char	tmpname[MAXPATHLEN];

	if (determine_fstype_and_mountroot(prom_bootpath()) == VFS_SUCCESS) {
		set_rootfs(prom_bootpath(), get_default_fs()->fsw_name);
		if (strcmp(get_default_fs()->fsw_name, "zfs") == 0)
			set_zfs_bootfs();

		strcpy(tmpname, name);
		int fd = openfile(tmpname);
		if (fd >= 0) {
			struct stat st;
			if (fstat(fd, &st) == 0) {
				xread(fd, (char *)virt, st.st_size);
			}
		} else {
			prom_printf("open failed %s\n", tmpname);
			prom_reset();
		}
		closeall(1);
		unmountroot();
	} else {
		prom_printf("mountroot failed\n");
		prom_reset();
	}
}

#define	MAXNMLEN	80		/* # of chars in an impl-arch name */

/*
 * Return the manufacturer name for this platform.
 *
 * This is exported (solely) as the rootnode name property in
 * the kernel's devinfo tree via the 'mfg-name' boot property.
 * So it's only used by boot, not the boot blocks.
 */
char *
get_mfg_name(void)
{
	pnode_t n;
	int len;

	static char mfgname[MAXNMLEN];

	if ((n = prom_rootnode()) != OBP_NONODE &&
	    (len = prom_getproplen(n, "mfg-name")) > 0 && len < MAXNMLEN) {
		(void) prom_getprop(n, "mfg-name", mfgname);
		mfgname[len] = '\0'; /* broken clones don't terminate name */
		return (mfgname);
	}

	return ("Unknown");
}

static char *
get_node_name(pnode_t nodeid)
{
	static char b[256];
	char *name = &b[0];
	strcpy(name, "");
	int namelen = prom_getproplen(nodeid, "name");
	if (namelen > 1) {
		strcpy(name, "/");
		name += strlen(name);
		prom_getprop(nodeid, "name", name);
		int reglen = prom_getproplen(nodeid, "reg");
		if (reglen > 0) {
			name += strlen(name);
			strcpy(name, "@");
			name += strlen(name);
			uint64_t base;
			prom_get_reg(nodeid, 0, &base);
			sprintf(name, "%lx", base);
		}
	}
	return b;
}

static void
get_bootpath_cb(pnode_t node, void *arg)
{
	if (!prom_is_compatible(node, "virtio-net"))
		return;

	char *path = (char *)arg;
	strcpy(path, "");
	for (;;) {
		char *name = get_node_name(node);
		size_t namelen = strlen(name);
		memmove(path + namelen, path, strlen(path) + 1);
		memcpy(path, name, namelen);
		node = prom_parentnode(node);
		if (node == prom_rootnode())
			break;
	}
}

char *
get_default_bootpath(void)
{
	static char def_bootpath[80];

	prom_walk(get_bootpath_cb, def_bootpath);
	return def_bootpath;
}

void _reset(void)
{
	prom_printf("%s:%d\n",__func__,__LINE__);
	psci_system_reset();
	for (;;) {}
}

void init_machdev(void)
{
	char str[] = "QEMU,virt-4.1";
	prom_setprop(prom_chosennode(), "impl-arch-name", (caddr_t)str, strlen(str) + 1);
	prom_setprop(prom_rootnode(), "mfg-name", (caddr_t)str, strlen(str) + 1);
	int namelen = prom_getproplen(prom_rootnode(), "compatible");
	namelen += strlen(str) + 1;
	char *compatible = __builtin_alloca(namelen);
	strcpy(compatible, str);
	prom_getprop(prom_rootnode(), "compatible", compatible + strlen(str) + 1);
	prom_setprop(prom_rootnode(), "compatible", compatible, namelen);

	init_virtnet();
	init_virtblk();
}
