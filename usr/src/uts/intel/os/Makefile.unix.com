
#
#	Core (unix) objects
#
CORE_OBJS +=		\
	arch_kdi.o	\
	comm_page_util.o \
	copy.o		\
	copy_subr.o	\
	cpc_subr.o	\
	ddi_arch.o	\
	ddi_i86.o	\
	ddi_i86_asm.o	\
	desctbls.o	\
	desctbls_asm.o	\
	exception.o	\
	float.o		\
	fmsmb.o		\
	fpu.o		\
	i86_subr.o	\
	lock_prim.o	\
	ovbcopy.o	\
	polled_io.o	\
	retpoline.o	\
	sseblk.o	\
	sundep.o	\
	swtch.o		\
	sysi86.o

DBOOT_OBJS +=		\
	retpoline.o


#
#	file system modules
#
CORE_OBJS +=		\
	prmachdep.o

#
#	shared hypervisor functionality
#
CORE_OBJS +=		\
	hma.o		\
	hma_asm.o	\
	hma_fpu.o	\
	smt.o		\

#
#	Decompression code
#
CORE_OBJS += decompress.o

#
#	Microcode utilities
#
CORE_OBJS += ucode_utils.o

#
#	Kernel linker
#
KRTLD_OBJS +=		\
	bootfsops.o	\
	bootrd.o	\
	bootrd_cpio.o	\
	ufsops.o	\
	hsfs.o		\
	doreloc.o	\
	kobj_boot.o	\
	kobj_convrelstr.o \
	kobj_crt.o	\
	kobj_isa.o	\
	kobj_reloc.o
