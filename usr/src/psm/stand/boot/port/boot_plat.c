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
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2024 Michael van der Westhuizen
 * Copyright 2017 Hayashi Naoyuki
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/obpdefs.h>
#include <sys/reboot.h>
#include <sys/promif.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/platnames.h>
#include <sys/salib.h>
#include <sys/elf.h>
#include <sys/link.h>
#include <sys/auxv.h>
#include <sys/boot_policy.h>
#include <sys/boot_redirect.h>
#include <sys/bootconf.h>
#include <sys/boot.h>
#include <util/getoptstr.h>
#include "boot_plat.h"
#include "ramdisk.h"
#include "console.h"

#define	SUCCESS		0
#define	FAILURE		-1

#ifdef DEBUG
int debug = 1;
#else
int debug = 0;
#endif

#define	dprintf		if (debug) printf

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
extern void prom_node_late_init(void);

int pagesize = PAGESIZE;
char filename[MAXPATHLEN];
int boothowto = 0;
int verbosemode = 0;
struct memlist *pfreelistp;
struct memlist *pscratchlistp;
struct memlist *pinstalledp;
struct memlist *piolistp;
struct memlist *ptmplistp;
struct memlist *plinearlistp;

char *impl_arch_name;
extern	int (*readfile(int fd, int print))();
extern	void exitto(int (*entrypoint)());
char *bootp_response;
char *kernname;
uint64_t boot_args[6];
int	cache_state = 1;
int boot_partition = -1;

extern char _RamdiskStart[];
extern char _RamdiskEnd[];

static void
post_mountroot(char *bootfile)
{
	int (*go)();
	int fd;

	fd = openfile(bootfile);
	if (fd == FAILURE)
		return;

	go = readfile(fd, boothowto & RB_VERBOSE);
	if (go != (int(*)()) -1) {
		exitto(go);
	}
}

void
set_default_filename(char *filename)
{
	kernname = filename;
}

static int
boot_open(char *pathname, void *arg)
{
	return (open(pathname, O_RDONLY));
}

int
openfile(char *filename)
{
	static char *fullpath;
	static int once;
	int fd;

	if (once == 0) {
		++once;
		fullpath = (char *)kmem_alloc(MAXPATHLEN, 0);
	}

	if (*filename == '/') {
		(void) strcpy(fullpath, filename);
		fd = boot_open(fullpath, NULL);
		return (fd);
	}

	fd = open_platform_file(filename, boot_open, NULL, fullpath);
	if (fd == -1)
		return (-1);

	/*
	 * Copy back the name we actually found
	 */
	(void) strcpy(filename, fullpath);
	return (fd);
}

static void
init_bootargs()
{
	phandle_t chosen = prom_chosennode();
	if (chosen == OBP_NONODE) {
		prom_add_subnode(prom_rootnode(), "chosen");
	}
	chosen = prom_chosennode();

	char *str;
	static char buf[OBP_MAXPATHLEN];
	int len = prom_getproplen(chosen, "bootargs");
	if (0 < len && len < sizeof (buf)) {
		prom_getprop(chosen, "bootargs", buf);
		struct gos_params params;
		params.gos_opts = "D:";
		params.gos_strp = buf;
		getoptstr_init(&params);
		int c;
		while ((c = getoptstr(&params)) != -1) {
			switch (c) {
			case 'D':
				buf[params.gos_optargp - buf +
				    params.gos_optarglen] = 0;
				str = &buf[params.gos_optargp - buf];
				prom_setprop(chosen, "__bootpath",
				    (caddr_t)str, strlen(str) + 1);
				break;
			default:
				break;
			}
		}
	}
	if (prom_getproplen(chosen, "__bootpath") <= 0) {
		str = get_default_bootpath();
		prom_setprop(chosen, "__bootpath",
		    (caddr_t)str, strlen(str) + 1);
	}
}

int
main()
{
	static char	bpath[OBP_MAXPATHLEN];
	static char	bargs[OBP_MAXPATHLEN];
	char *def_boot_archive = "boot_archive";

	init_console();

	prom_init("boot", NULL);

	fiximp();

	init_memlists();
	prom_node_init();
	init_memory();
	prom_node_late_init();

	init_machdev();
	init_ramdisk();

	init_bootargs();
	strncpy(bargs, prom_bootargs(), sizeof (bargs) - 1);
	strncpy(bpath, prom_bootpath(), sizeof (bpath) - 1);
	prom_printf("bootargs=%s\n", bargs);
	prom_printf("bootpath=%s\n", bpath);

	caddr_t virt = create_ramdisk(RD_ROOTFS,
	    (uintptr_t)_RamdiskEnd - (uintptr_t)_RamdiskStart, NULL);
	load_ramdisk(virt, def_boot_archive);

	if (determine_fstype_and_mountroot(RD_ROOTFS) != VFS_SUCCESS) {
		return (-1);
	}

	dprintf("\nboot: V%d /boot interface.\n", BO_VERSION);

	set_default_filename("kernel/aarch64/unix");

	strcpy(filename, kernname);
	post_mountroot(filename);

	return (0);
}
