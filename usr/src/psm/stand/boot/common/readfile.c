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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/exechdr.h>
#include <sys/elf.h>
#include <sys/elf_notes.h>
#include <sys/bootconf.h>
#include <sys/reboot.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/link.h>
#include <sys/auxv.h>
#include <sys/salib.h>
#include <sys/bootvfs.h>
#include <sys/platnames.h>

#include "util.h"

union {
	struct exec X;
	Elf32_Ehdr Elfhdr;
	Elf64_Ehdr Elfhdr64;
} ex;

#define	x ex.X
#define	elfhdr ex.Elfhdr
#define	elfhdr64 ex.Elfhdr64

typedef int	(*func_t)();

#define	FAIL	((func_t)-1)
#define	ALIGN(x, a)	\
	((a) == 0 ? (uintptr_t)(x) : (((uintptr_t)(x) + (a) - 1) & ~((a) - 1)))

#define	__BOOT_NAUXV_IMPL	22

int	use_align = 0;
int	npagesize = 0;
uint_t	icache_flush = 0;
char	*cpulist = NULL;
char	*mmulist = NULL;
char	*module_path;		/* path for kernel modules */

/*
 * This file gets compiled in LP64 (for sun4u) and ILP32 models.
 * For LP64 compilation, the "client" file we load and run may be LP64 or ILP32,
 * and during bringup, the LP64 clients may have ELF32 headers.
 */
/*
 * Bootstrap vector for ELF32 LP64 client
 */
Elf64_Boot *elfbootvecELF64;	/* ELF bootstrap vector for Elf64 LP64 */

#define	OK		((func_t)0)

#define	FAIL_READELF64	((uint64_t)0)
#define	FAIL_ILOAD64	((Elf64_Addr)-1)

#ifdef DEBUG
static int debug = 1;
#else /* DEBUG */
static int debug = 0;
#endif /* DEBUG */

#define	dprintf		if (debug) printf

typedef struct {
	uint_t	a_type;
	union {
		uint64_t a_val;
		uint64_t a_ptr;
	} a_un;
} auxv64_t;

#if defined(__sparcv9)
extern int client_isLP64;
#endif	/* __sparcv9 */

static uint64_t read_elf64(int, int, Elf64_Ehdr *);
static Elf64_Addr iload64(char *, Elf64_Phdr *, Elf64_Phdr *, auxv64_t **);

static caddr_t	segbrk(caddr_t *, size_t, size_t);
static int	openpath(char *, char *, int);
static char	*getmodpath(char *);
extern void	setup_aux(void);

extern void	*kmem_alloc(size_t, int);
extern void	kmem_free(void *, size_t);
extern int	cons_gets(char *, int);

extern void sync_instruction_memory(caddr_t v, size_t len);

extern int	verbosemode;
extern int	boothowto;
extern int	pagesize;
extern char	filename[];

/*
 * repeat reads (forever) until size of request is satisfied
 * (Thus, you don't want to use this cases where short reads are ok)
 */
ssize_t
xread(int fd, char *p, size_t nbytes)
{
	size_t bytesread = 0;
	int errorcount = 0;
	ssize_t i;

	while (bytesread < nbytes) {
		i = read(fd, p, nbytes - bytesread);
		if (i < 0) {
			++errorcount;
			if (verbosemode)
				printf("read error (0x%x times)\n", errorcount);
			continue;
		}
		bytesread += i;
		p += i;
	}
	return (bytesread);
}

/*
 * Read in a Unix executable file and return its entry point.
 * Handle the various a.out formats correctly.
 * "fd" is the standalone file descriptor to read from.
 * Print informative little messages if "print" is on.
 * Returns -1 for errors.
 */
func_t
readfile(int fd, int print)
{
	uint64_t elf64_go2;

	ssize_t i;
	int shared = 0;

	if (verbosemode) {
		dprintf("fd = %x\n", fd);
	}

	i = xread(fd, (char *)&elfhdr, sizeof (Elf64_Ehdr));
	if (x.a_magic == ZMAGIC || x.a_magic == NMAGIC)
		shared = 1;
	if (i != sizeof (Elf64_Ehdr)) {
		printf("Error reading ELF header.\n");
		return (FAIL);
	}
	if (!shared && x.a_magic != OMAGIC) {
		if (*(int *)&elfhdr.e_ident == *(int *)(ELFMAG)) {
			if (verbosemode) {
				int is64 = (elfhdr.e_ident[EI_CLASS] ==
				    ELFCLASS64);

				dprintf("calling readelf, elfheader is:\n");
				dprintf("e_ident\t0x%x, 0x%x, 0x%x, 0x%x\n",
				    *(int *)&elfhdr.e_ident[0],
				    *(int *)&elfhdr.e_ident[4],
				    *(int *)&elfhdr.e_ident[8],
				    *(int *)&elfhdr.e_ident[12]);
				dprintf("e_machine\t0x%x\n", elfhdr.e_machine);

				dprintf("e_entry\t\t0x%llx\n", (is64 ?
				    elfhdr64.e_entry :
				    (u_longlong_t)elfhdr.e_entry));
				dprintf("e_shoff\t\t0x%llx\n", (is64 ?
				    elfhdr64.e_shoff :
				    (u_longlong_t)elfhdr.e_shoff));
				dprintf("e_shnentsize\t%d\n", (is64 ?
				    elfhdr64.e_shentsize : elfhdr.e_shentsize));
				dprintf("e_shnum\t\t%d\n", (is64 ?
				    elfhdr64.e_shnum : elfhdr.e_shnum));
				dprintf("e_shstrndx\t%d\n", (is64 ?
				    elfhdr64.e_shstrndx : elfhdr.e_shstrndx));
			}


			dprintf("ELF file CLASS 0x%x 32 is %x 64 is %x\n",
			    elfhdr.e_ident[EI_CLASS], ELFCLASS32, ELFCLASS64);

			if (elfhdr.e_ident[EI_CLASS] == ELFCLASS64) {
				elf64_go2 = read_elf64(fd, print,
				    (Elf64_Ehdr *)&elfhdr);

				return ((elf64_go2 == FAIL_READELF64) ? FAIL :
				    (func_t)elf64_go2);

			}
		} else {
			printf("File not executable.\n");
			return (FAIL);
		}
	}
	return (FAIL);
}

/*
 * Macros to add attribute/values to the ELF bootstrap vector
 * and the aux vector.
 */
#define	AUX64(p, a, v)	{ (p)->a_type = (a); \
			((p)++)->a_un.a_val = (uint64_t)(v); }

#define	EBV64(p, a, v)	{ (p)->eb_tag = (a); \
			((p)++)->eb_un.eb_val = (Elf64_Xword)(v); }

static uint64_t
read_elf64(int fd, int print, Elf64_Ehdr *elfhdrp)
{
	Elf64_Phdr *phdr;	/* program header */
	Elf64_Nhdr *nhdr;	/* note header */
	int nphdrs, phdrsize;
	caddr_t allphdrs;
	caddr_t	namep, descp;
	Elf64_Addr loadaddr, base;
	size_t offset = 0;
	size_t size;
	int i;
	uintptr_t	off;
	int bss_seen = 0;
	int interp = 0;				/* interpreter required */
	static char dlname[MAXPATHLEN];		/* name of interpeter */
	uintptr_t dynamic;			/* dynamic tags array */
	Elf64_Phdr *thdr;			/* "text" program header */
	Elf64_Phdr *dhdr;			/* "data" program header */
	Elf64_Addr entrypt;			/* entry point of standalone */

	/* Initialize pointers so we won't free bogus ones on elf64error */
	allphdrs = NULL;
	nhdr = NULL;
#if defined(__sparcv9)
	client_isLP64 = 1;
#endif	/* __sparcv9 */

	if (verbosemode)
		printf("Elf64 client\n");

	if (elfhdrp->e_phnum == 0 || elfhdrp->e_phoff == 0)
		goto elf64error;

	entrypt = elfhdrp->e_entry;
	if (verbosemode)
		dprintf("Entry point: 0x%llx\n", (u_longlong_t)entrypt);

	/*
	 * Allocate and read in all the program headers.
	 */
	nphdrs = elfhdrp->e_phnum;
	phdrsize = nphdrs * elfhdrp->e_phentsize;
	allphdrs = (caddr_t)kmem_alloc(phdrsize, 0);
	if (allphdrs == NULL)
		goto elf64error;
	if (verbosemode)
		dprintf("lseek: args = %x %llx %x\n", fd,
		    (u_longlong_t)elfhdrp->e_phoff, 0);
	if (lseek(fd, elfhdrp->e_phoff, 0) == -1)
		goto elf64error;
	if (xread(fd, allphdrs, phdrsize) != phdrsize)
		goto elf64error;

	/*
	 * First look for PT_NOTE headers that tell us what pagesize to
	 * use in allocating program memory.
	 */
	npagesize = 0;
	for (i = 0; i < nphdrs; i++) {
		void *note_buf;

		phdr = (Elf64_Phdr *)(allphdrs + elfhdrp->e_phentsize * i);
		if (phdr->p_type != PT_NOTE)
			continue;
		if (verbosemode) {
			dprintf("allocating 0x%llx bytes for note hdr\n",
			    (u_longlong_t)phdr->p_filesz);
		}
		if ((note_buf = kmem_alloc(phdr->p_filesz, 0)) == NULL)
			goto elf64error;
		if (verbosemode)
			dprintf("seeking to 0x%llx\n",
			    (u_longlong_t)phdr->p_offset);
		if (lseek(fd, phdr->p_offset, 0) == -1)
			goto elf64error;
		if (verbosemode) {
			dprintf("reading 0x%llx bytes into 0x%p\n",
			    (u_longlong_t)phdr->p_filesz, (void *)nhdr);
		}
		nhdr = (Elf64_Nhdr *)note_buf;
		if (xread(fd, (caddr_t)nhdr, phdr->p_filesz) != phdr->p_filesz)
			goto elf64error;
		if (verbosemode) {
			dprintf("p_note namesz %x descsz %x type %x\n",
			    nhdr->n_namesz, nhdr->n_descsz, nhdr->n_type);
		}

		/*
		 * Iterate through all ELF PT_NOTE elements looking for
		 * ELF_NOTE_SOLARIS which, if present, will specify the
		 * executable's preferred pagesize.
		 */
		do {
			namep = (caddr_t)(nhdr + 1);

			if (nhdr->n_namesz == strlen(ELF_NOTE_SOLARIS) + 1 &&
			    strcmp(namep, ELF_NOTE_SOLARIS) == 0 &&
			    nhdr->n_type == ELF_NOTE_PAGESIZE_HINT) {
				descp = namep + roundup(nhdr->n_namesz, 4);
				npagesize = *(int *)descp;
				if (verbosemode)
					dprintf("pagesize is %x\n", npagesize);
			}

			offset += sizeof (Elf64_Nhdr) + roundup(nhdr->n_namesz,
			    4) + roundup(nhdr->n_descsz, 4);

			nhdr = (Elf64_Nhdr *)((char *)note_buf + offset);
		} while (offset < phdr->p_filesz);

		kmem_free(note_buf, phdr->p_filesz);
		nhdr = NULL;
	}

	/*
	 * Next look for PT_LOAD headers to read in.
	 */
	if (print)
		printf("Size: ");
	for (i = 0; i < nphdrs; i++) {
		phdr = (Elf64_Phdr *)(allphdrs + elfhdrp->e_phentsize * i);
		if (verbosemode) {
			dprintf("Doing header 0x%x\n", i);
			dprintf("phdr\n");
			dprintf("\tp_offset = %llx, p_vaddr = %llx\n",
			    (u_longlong_t)phdr->p_offset,
			    (u_longlong_t)phdr->p_vaddr);
			dprintf("\tp_memsz = %llx, p_filesz = %llx\n",
			    (u_longlong_t)phdr->p_memsz,
			    (u_longlong_t)phdr->p_filesz);
			dprintf("\tp_type = %x, p_flags = %x\n",
			    phdr->p_type, phdr->p_flags);
		}
		if (phdr->p_type == PT_LOAD) {
			if (verbosemode)
				dprintf("seeking to 0x%llx\n",
				    (u_longlong_t)phdr->p_offset);
			if (lseek(fd, phdr->p_offset, 0) == -1)
				goto elf64error;

			if (phdr->p_flags == (PF_R | PF_W) &&
			    phdr->p_vaddr == 0) {
				/*
				 * It's a PT_LOAD segment that is RW but
				 * not executable and has a vaddr
				 * of zero.  This is relocation info that
				 * doesn't need to stick around after
				 * krtld is done with it.  We allocate boot
				 * memory for this segment, since we don't want
				 * it mapped in permanently as part of
				 * the kernel image.
				 */
				if ((loadaddr = (Elf64_Addr)(uintptr_t)
				    kmem_alloc(phdr->p_memsz, 0)) == 0)
					goto elf64error;

				/*
				 * Save this to pass on
				 * to the interpreter.
				 */
				phdr->p_vaddr = loadaddr;
			} else {
				if (print)
					printf("0x%llx+",
					    (u_longlong_t)phdr->p_filesz);
				/*
				 * If we found a new pagesize above, use it
				 * to adjust the memory allocation.
				 */
				loadaddr = phdr->p_vaddr;
				if (use_align && npagesize != 0) {
					off = loadaddr & (npagesize - 1);
					size = roundup(phdr->p_memsz + off,
					    npagesize);
					base = loadaddr - off;
				} else {
					npagesize = 0;
					size = phdr->p_memsz;
					base = loadaddr;
				}
				/*
				 *  Check if it's text or data.
				 */
				if (phdr->p_flags & PF_W)
					dhdr = phdr;
				else if (phdr->p_flags & PF_X)
					thdr = phdr;

				if (verbosemode)
					dprintf(
					    "allocating memory: %llx %lx %x\n",
					    (u_longlong_t)base,
					    size, npagesize);

				/*
				 * If memory size is zero just ignore this
				 * header.
				 */
				if (size == 0)
					continue;

				/*
				 * We're all set up to read.
				 * Now let's allocate some memory.
				 */
				if (get_progmemory((caddr_t)(uintptr_t)base,
				    size, npagesize))
					goto elf64error;
			}

			if (verbosemode) {
				dprintf("reading 0x%llx bytes into 0x%llx\n",
				    (u_longlong_t)phdr->p_filesz,
				    (u_longlong_t)loadaddr);
			}
			if (xread(fd, (caddr_t)(uintptr_t)
			    loadaddr, phdr->p_filesz) != phdr->p_filesz)
				goto elf64error;

			/* zero out BSS */
			if (phdr->p_memsz > phdr->p_filesz) {
				loadaddr += phdr->p_filesz;
				if (verbosemode) {
					dprintf("bss from 0x%llx size 0x%llx\n",
					    (u_longlong_t)loadaddr,
					    (u_longlong_t)(phdr->p_memsz -
					    phdr->p_filesz));
				}

				bzero((caddr_t)(uintptr_t)loadaddr,
				    phdr->p_memsz - phdr->p_filesz);
				bss_seen++;
				if (print)
					printf("0x%llx Bytes\n",
					    (u_longlong_t)(phdr->p_memsz -
					    phdr->p_filesz));
			}

			/* force instructions to be visible to icache */
			if (phdr->p_flags & PF_X)
				sync_instruction_memory((caddr_t)(uintptr_t)
				    phdr->p_vaddr, phdr->p_memsz);

		} else if (phdr->p_type == PT_INTERP) {
			/*
			 * Dynamically-linked executable.
			 */
			interp = 1;
			if (lseek(fd, phdr->p_offset, 0) == -1) {
				goto elf64error;
			}
			/*
			 * Get the name of the interpreter.
			 */
			if (xread(fd, dlname, phdr->p_filesz) !=
			    phdr->p_filesz ||
			    dlname[phdr->p_filesz - 1] != '\0')
				goto elf64error;
		} else if (phdr->p_type == PT_DYNAMIC) {
			dynamic = phdr->p_vaddr;
		}
	}

	if (!bss_seen && print)
		printf("0 Bytes\n");

	/*
	 * Load the interpreter
	 * if there is one.
	 */
	if (interp) {
		Elf64_Boot bootv[EB_MAX];		/* Bootstrap vector */
		auxv64_t auxv[__BOOT_NAUXV_IMPL];	/* Aux vector */
		Elf64_Boot *bv = bootv;
		auxv64_t *av = auxv;
		size_t vsize;

		/*
		 * Load it.
		 */
		if ((entrypt = iload64(dlname, thdr, dhdr, &av)) ==
		    FAIL_ILOAD64)
			goto elf64error;
		/*
		 * Build bootstrap and aux vectors.
		 */
		setup_aux();
		EBV64(bv, EB_AUXV, 0); /* fill in later */
		EBV64(bv, EB_PAGESIZE, pagesize);
		EBV64(bv, EB_DYNAMIC, dynamic);
		EBV64(bv, EB_NULL, 0);

		AUX64(av, AT_BASE, entrypt);
		AUX64(av, AT_ENTRY, elfhdrp->e_entry);
		AUX64(av, AT_PAGESZ, pagesize);
		AUX64(av, AT_PHDR, (uintptr_t)allphdrs);
		AUX64(av, AT_PHNUM, elfhdrp->e_phnum);
		AUX64(av, AT_PHENT, elfhdrp->e_phentsize);
		if (npagesize)
			AUX64(av, AT_SUN_LPAGESZ, npagesize);

		AUX64(av, AT_SUN_IFLUSH, icache_flush);
		if (cpulist != NULL)
			AUX64(av, AT_SUN_CPU, (uintptr_t)cpulist);
		AUX64(av, AT_NULL, 0);
		/*
		 * Realloc vectors and copy them.
		 */
		vsize = (caddr_t)bv - (caddr_t)bootv;
		if ((elfbootvecELF64 =
		    (Elf64_Boot *)kmem_alloc(vsize, 0)) == NULL)
			goto elf64error;
		bcopy((char *)bootv, (char *)elfbootvecELF64, vsize);

		size = (caddr_t)av - (caddr_t)auxv;
		if (size > sizeof (auxv)) {
			printf("readelf: overrun of available aux vectors\n");
			kmem_free(elfbootvecELF64, vsize);
			goto elf64error;
		}

		if ((elfbootvecELF64->eb_un.eb_ptr =
		    (Elf64_Addr)kmem_alloc(size, 0)) == 0) {
			kmem_free(elfbootvecELF64, vsize);
			goto elf64error;
		}

		bcopy((char *)auxv, (char *)(elfbootvecELF64->eb_un.eb_ptr),
		    size);
	} else {
		kmem_free(allphdrs, phdrsize);
	}
	return ((uint64_t)entrypt);

elf64error:
	if (allphdrs != NULL)
		kmem_free(allphdrs, phdrsize);
	if (nhdr != NULL)
		kmem_free(nhdr, phdr->p_filesz);
	printf("Elf64 read error.\n");
	return (FAIL_READELF64);
}

/*
 * Load the interpreter.  It expects a
 * relocatable .o capable of bootstrapping
 * itself.
 */
static Elf64_Addr
iload64(char *rtld, Elf64_Phdr *thdr, Elf64_Phdr *dhdr, auxv64_t **avp)
{
	Elf64_Ehdr *ehdr = NULL;
	Elf64_Addr dl_entry = (Elf64_Addr)0;
	Elf64_Addr etext, edata;
	uint_t i;
	int fd;
	int size;
	caddr_t shdrs = NULL;

	etext = thdr->p_vaddr + thdr->p_memsz;
	edata = dhdr->p_vaddr + dhdr->p_memsz;

	/*
	 * Get the module path.
	 */
	module_path = getmodpath(filename);

	if ((fd = openpath(module_path, rtld, O_RDONLY)) < 0) {
		printf("boot: cannot find %s\n", rtld);
		goto errorx;
	}
	dprintf("Opened %s OK\n", rtld);
	AUX64(*avp, AT_SUN_LDNAME, (uintptr_t)rtld);
	/*
	 * Allocate and read the ELF header.
	 */
	if ((ehdr = (Elf64_Ehdr *)kmem_alloc(sizeof (Elf64_Ehdr), 0)) == NULL) {
		printf("boot: alloc error reading ELF header (%s).\n", rtld);
		goto error;
	}

	if (xread(fd, (char *)ehdr, sizeof (*ehdr)) != sizeof (*ehdr)) {
		printf("boot: error reading ELF header (%s).\n", rtld);
		goto error;
	}

	size = ehdr->e_shentsize * ehdr->e_shnum;
	if ((shdrs = (caddr_t)kmem_alloc(size, 0)) == NULL) {
		printf("boot: alloc error reading ELF header (%s).\n", rtld);
		goto error;
	}
	/*
	 * Read the section headers.
	 */
	if (lseek(fd, ehdr->e_shoff, 0) == -1 ||
	    xread(fd, shdrs, size) != size) {
		printf("boot: error reading section headers\n");
		goto error;
	}

	AUX64(*avp, AT_SUN_LDELF, ehdr);
	AUX64(*avp, AT_SUN_LDSHDR, shdrs);

	/*
	 * Load sections into the appropriate dynamic segment.
	 */
	for (i = 1; i < ehdr->e_shnum; i++) {
		Elf64_Shdr *sp;
		Elf64_Addr *spp, load;

		sp = (Elf64_Shdr *)(shdrs + (i*ehdr->e_shentsize));
		/*
		 * If it's not allocated and not required
		 * to do relocation, skip it.
		 */
		if (!(sp->sh_flags & SHF_ALLOC) &&
		    sp->sh_type != SHT_SYMTAB &&
		    sp->sh_type != SHT_STRTAB &&
		    sp->sh_type != SHT_RELA)
			continue;
		/*
		 * If the section is read-only,
		 * it goes in as text.
		 */
		spp = (sp->sh_flags & SHF_WRITE)? &edata: &etext;

		/*
		 * Make some room for it.
		 */
		load = (Elf64_Addr)segbrk((caddr_t *)spp, sp->sh_size,
		    sp->sh_addralign);

		if (load == 0) {
			printf("boot: allocating memory for section %d "
			    "failed\n", i);
			goto error;
		}

		/*
		 * Compute the entry point of the linker.
		 */
		if (dl_entry == 0 &&
		    !(sp->sh_flags & SHF_WRITE) &&
		    (sp->sh_flags & SHF_EXECINSTR)) {
			dl_entry = load + ehdr->e_entry;
			if (verbosemode)
				dprintf("boot: loading linker @ 0x%llx\n",
				    (u_longlong_t)dl_entry);
		}

		/*
		 * If it's bss, just zero it out.
		 */
		if (sp->sh_type == SHT_NOBITS) {
			bzero((caddr_t)(uintptr_t)load, sp->sh_size);
		} else {
			/*
			 * Read the section contents.
			 */
			if (lseek(fd, sp->sh_offset, 0) == -1 ||
			    xread(fd, (caddr_t)(uintptr_t)load, sp->sh_size) !=
			    sp->sh_size) {
				printf("boot: error reading section %d\n", i);
				goto error;
			}
		}
		/*
		 * Assign the section's virtual addr.
		 */

		sp->sh_addr = load;

		if (verbosemode)
			dprintf("boot: section %d, type %d, loaded @ 0x%llx, "
			    "size 0x%llx\n", i, sp->sh_type, (u_longlong_t)load,
			    (u_longlong_t)sp->sh_size);

		/* force instructions to be visible to icache */
		if (sp->sh_flags & SHF_EXECINSTR)
			sync_instruction_memory((caddr_t)(uintptr_t)sp->sh_addr,
			    sp->sh_size);
	}
	/*
	 * Update sizes of segments.
	 */
	thdr->p_memsz = etext - thdr->p_vaddr;
	dhdr->p_memsz = edata - dhdr->p_vaddr;

	/* load and relocate symbol tables in SAS */
	(void) close(fd);
	return (dl_entry);

error:
	(void) close(fd);
errorx:
	if (ehdr)
		kmem_free((caddr_t)ehdr, sizeof (Elf64_Ehdr));
	if (shdrs)
		kmem_free(shdrs, size);
	printf("boot: error loading interpreter (%s)\n", rtld);
	return (FAIL_ILOAD64);
}

/*
 * Extend the segment's "break" value by bytes.
 */
static caddr_t
segbrk(caddr_t *spp, size_t bytes, size_t align)
{
	caddr_t va, pva;
	size_t size = 0;
	unsigned int alloc_pagesize = pagesize;
	unsigned int alloc_align = 0;

	if (npagesize) {
		alloc_align = npagesize;
		alloc_pagesize = npagesize;
	}

	va = (caddr_t)ALIGN(*spp, align);
	pva = (caddr_t)roundup((uintptr_t)*spp, alloc_pagesize);
	/*
	 * Need more pages?
	 */
	if (va + bytes > pva) {
		size = roundup((bytes - (pva - va)), alloc_pagesize);

		if (get_progmemory(pva, size, alloc_align)) {
			printf("boot: segbrk allocation failed, "
			    "0x%lx bytes @ %p\n", bytes, (void *)pva);
			return (NULL);
		}
	}
	*spp = va + bytes;

	return (va);
}

/*
 * Open the file using a search path and
 * return the file descriptor (or -1 on failure).
 */
static int
openpath(char *path, char *fname, int flags)
{
	register char *p, *q;
	char buf[MAXPATHLEN];
	int fd;

	/*
	 * If the file name is absolute,
	 * don't use the module search path.
	 */
	if (fname[0] == '/')
		return (open(fname, flags));

	q = NULL;
	for (p = path;  /* forever */;  p = q) {

		while (*p == ' ' || *p == '\t' || *p == ':')
			p++;
		if (*p == '\0')
			break;
		q = p;
		while (*q && *q != ' ' && *q != '\t' && *q != ':')
			q++;
		(void) strncpy(buf, p, q - p);
		if (q[-1] != '/') {
			buf[q - p] = '/';
			(void) strcpy(&buf[q - p + 1], fname);
		} else {
			/*
			 * This checks for paths that end in '/'
			 */
			(void) strcpy(&buf[q - p], fname);
		}

		if ((fd = open(buf, flags)) > 0)
			return (fd);
	}
	return (-1);
}

/*
 * Get the module search path.
 */
static char *
getmodpath(char *fname)
{
	register char *p = strrchr(fname, '/');
	static char mod_path[MOD_MAXPATH];
	size_t len;
	extern char *impl_arch_name;
#if defined(__sparcv9)
	char    *isastr = "/sparcv9";
	size_t	isalen = strlen(isastr);
#endif	/* __sparcv9 */

	if (p == NULL) {
		/* strchr could not find a "/" */
		printf("%s is not a legal kernel pathname", fname);
		return (NULL);
	}
	while (p > fname && *(p - 1) == '/')
		p--;		/* remove trailing "/"s */
	if (p == fname)
		p++;		/* "/" is the modpath in this case */

	len = p - fname;
	(void) strncpy(mod_path, fname, len);
	mod_path[len] = 0;

#if defined(__sparcv9)
	len = strlen(mod_path);
	if ((len > isalen) && (strcmp(&mod_path[len - isalen], isastr) == 0)) {
		mod_path[len - isalen] = '\0';
		if ((client_isLP64 == 0) && verbosemode)
			printf("Assuming LP64 %s client.\n", isastr);
		client_isLP64 = 1;
	}
#endif	/* __sparcv9 */
	mod_path_uname_m(mod_path, impl_arch_name);
	(void) strcat(mod_path, " ");
	(void) strcat(mod_path, MOD_DEFPATH);

	if (boothowto & RB_ASKNAME) {
		char buf[MOD_MAXPATH];

		printf("Enter default directory for modules [%s]: ", mod_path);
		(void) cons_gets(buf, sizeof (buf));
		if (buf[0] != '\0')
			(void) strcpy(mod_path, buf);
	}
	if (verbosemode)
		printf("modpath: %s\n", mod_path);
	return (mod_path);
}
