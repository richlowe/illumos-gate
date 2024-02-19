/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2024 Michael van der Westhuizen
 * Copyright 2017 Hayashi Naoyuki
 */

#ifndef _SYS_BOOTCONF_H
#define	_SYS_BOOTCONF_H

/*
 * Boot time configuration information objects
 */

#include <sys/types.h>
#include <sys/bootstat.h>
#include <sys/dirent.h>			/* for struct dirent */
#include <sys/memlist.h>
#include <sys/obpdefs.h>
#include <net/if.h>			/* for IFNAMSIZ */
#include <sys/varargs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	BP_MAX_STRLEN	32

/*
 * masks to hand to bsys_alloc memory allocator
 * XXX	These names shouldn't really be srmmu derived.
 */
#define	BO_NO_ALIGN	0x00002000

/* return values for the newer bootops */
#define	BOOT_SUCCESS	0
#define	BOOT_FAILURE	(-1)

/*
 *  We pass a ptr to the space that boot has been using
 *  for its memory lists.
 */
struct bsys_mem {
	uint_t		extent;		/* number of bytes in the space */
};

/*
 * Warning: Changing BO_VERSION blows compatibility between booters
 *          and older kernels.  If you want to change the struct bootops,
 *          please consider adding new stuff to the end and using the
 *          "bootops-extensions" mechanism described below.
 */
#define	BO_VERSION	10		/* bootops interface revision # */

typedef struct bootops {
	/*
	 * the ubiquitous version number
	 */
	uint_t	bsys_version;
	uintptr_t	bsys_ptbl;

	/*
	 * have boot allocate size bytes at virthint
	 */
	caddr_t	(*bsys_alloc)(struct bootops *, caddr_t virthint, size_t size,
		int align);
	paddr_t (*bsys_palloc)(struct bootops *, size_t size, int align);
	/*
	 * free size bytes allocated at virt - put the
	 * address range back onto the avail lists.
	 */
	void	(*bsys_free)(struct bootops *, caddr_t virt, size_t size);

	/*
	 * to find the size of the buffer to allocate
	 */
	ssize_t	(*bsys_getproplen)(struct bootops *, const char *);

	/*
	 * get the value associated with this name
	 */
	int	(*bsys_getprop)(struct bootops *, const char *, void *);

	/*
	 * get the name of the next property in succession
	 * from the standalone
	 */
	char	*(*bsys_nextprop)(struct bootops *, char *prevprop);

	/*
	 * print formatted output
	 */
	void	(*bsys_printf)(void *, const char *, ...) __PRINTFLIKE(2);

	/* end of bootops which exist if (bootops-extensions >= 1) */
} bootops_t;

#define	BOP_GETVERSION(bop)		((bop)->bsys_version)
#define	BOP_ALLOC(bop, virthint, size, align)	\
				((bop)->bsys_alloc)(bop, virthint, size, align)
#define	BOP_PALLOC(bop, size, align)	((bop)->bsys_palloc)(bop, size, align)
#define	BOP_FREE(bop, virt, size)	((bop)->bsys_free)(bop, virt, size)
#define	BOP_GETPROPLEN(bop, name)	((bop)->bsys_getproplen)(bop, name)
#define	BOP_GETPROP(bop, name, buf)	((bop)->bsys_getprop)(bop, name, buf)
#define	BOP_NEXTPROP(bop, prev)		((bop)->bsys_nextprop)(bop, prev)
#define	BOP_DOINT(bop, intnum, rp)	((bop)->bsys_doint)(bop, intnum, rp)
#define	BOP_EALLOC(bop, virthint, size, align, flags)\
		((bop)->bsys_ealloc)(bop, virthint, size, align, flags)

#define	BOP_PUTSARG(bop, msg, arg)	((bop)->bsys_printf)(bop, msg, arg)

#if defined(_KERNEL) && !defined(_BOOT)

/*
 * Boot configuration information
 */

#define	BO_MAXFSNAME	16
#define	BO_MAXOBJNAME	256

struct bootobj {
	char	bo_fstype[BO_MAXFSNAME];	/* vfs type name (e.g. nfs) */
	char	bo_name[BO_MAXOBJNAME];		/* name of object */
	int	bo_flags;			/* flags, see below */
	int	bo_size;			/* number of blocks */
	struct vnode *bo_vp;			/* vnode of object */
	char	bo_devname[BO_MAXOBJNAME];
	char	bo_ifname[BO_MAXOBJNAME];
	int	bo_ppa;
};

/*
 * flags
 */
#define	BO_VALID	0x01	/* all information in object is valid */
#define	BO_BUSY		0x02	/* object is busy */

extern struct bootobj rootfs;
extern struct bootobj swapfile;

extern char obp_bootpath[BO_MAXOBJNAME];
extern char svm_bootpath[BO_MAXOBJNAME];

extern void *gfx_devinfo_list;

extern dev_t getrootdev(void);
extern void getfsname(char *, char *, size_t);
extern int loadrootmodules(void);

extern int strplumb(void);
extern int strplumb_load(void);
extern char *strplumb_get_netdev_path(void);

extern void consconfig(void);
extern void release_bootstrap(void);

extern void param_check(void);
extern int octet_to_hexascii(const void *, uint_t, char *, uint_t *);

extern int dhcpinit(void);

extern struct bootops *bootops;
extern int netboot;
extern int swaploaded;
extern int modrootloaded;
extern char kern_bootargs[];
extern char kern_bootfile[];
extern char *kobj_module_path;
extern char *default_path;
extern char *dhcack;
extern int dhcacklen;
extern char dhcifname[IFNAMSIZ];
extern char *netdev_path;

extern void bop_no_more_mem(void);

extern void bop_printf(void *, const char *, ...) __PRINTFLIKE(2);
extern void vbop_printf(void *ops, const char *fmt, va_list) __VPRINTFLIKE(2);
extern void bop_panic(const char *, ...) __PRINTFLIKE(1);

extern void read_bootenvrc(void);

extern int bootprop_getval(const char *, u_longlong_t *);
struct xboot_info;
void bop_init(struct xboot_info *xbp);

extern ssize_t do_bsys_getproplen(bootops_t *, const char *);
extern int do_bsys_getprop(bootops_t *, const char *, void *);
extern int do_bsys_getproptype(bootops_t *, const char *);

#endif /* _KERNEL && !_BOOT */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_BOOTCONF_H */
