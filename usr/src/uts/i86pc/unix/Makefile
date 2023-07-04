#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2019 Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
UNIX		= unix
DBOOT		= dboot

# Object lists.  Note that these are augmented by the Makefile.unix.com
# for each less-specific platform (common, intel, ...)
CORE_OBJS +=			\
	acpi_stubs.o		\
	biosdisk.o		\
	bios_call.o		\
	cbe.o			\
	cmi.o			\
	cmi_hw.o		\
	cms.o			\
	comm_page.o		\
	confunix.o		\
	cpu_idle.o		\
	cpuid.o			\
	cpuid_subr.o		\
	cpupm.o			\
	cpupm_mach.o		\
	cpupm_amd.o		\
	cpupm_intel.o		\
	cpupm_throttle.o	\
	cpu_acpi.o		\
	dis_tables.o		\
	ddi_impl.o		\
	dtrace_subr.o		\
	dvma.o			\
	fpu_subr.o		\
	fakebop.o		\
	fastboot.o		\
	fb_swtch.o		\
	graphics.o		\
	hardclk.o		\
	hat_i86.o		\
	hat_kdi.o		\
	hment.o			\
	hold_page.o		\
	hpet_acpi.o		\
	hrtimers.o		\
	htable.o		\
	hypercall.o		\
	hypersubr.o		\
	i86_mmu.o		\
	ibft.o			\
	instr_size.o		\
	intr.o			\
	kboot_mmu.o		\
	kdi_idt.o		\
	kdi_idthdl.o		\
	kdi_asm.o		\
	lgrpplat.o		\
	mach_kdi.o		\
	mach_sysconfig.o	\
	machdep.o		\
	md_clear.o		\
	mem_config.o		\
	mem_config_stubs.o	\
	mem_config_arch.o	\
	memlist_new.o		\
	memnode.o		\
	microcode_amd.o		\
	microcode_intel.o	\
	mlsetup.o		\
	mp_call.o		\
	mp_implfuncs.o		\
	mp_machdep.o		\
	mp_pc.o			\
	mp_startup.o		\
	memscrub.o		\
	mpcore.o		\
	notes.o			\
	pci_bios.o		\
	pci_cfgacc.o		\
	pci_cfgacc_x86.o	\
	pci_cfgspace.o		\
	pci_mech1.o		\
	pci_mech1_amd.o		\
	pci_mech2.o		\
	pci_neptune.o		\
	pci_orion.o		\
	pmem.o			\
	ppage.o			\
	prom_panic.o		\
	prom_reboot.o		\
	pwrnow.o		\
	speedstep.o		\
	ssp.o			\
	startup.o		\
	timestamp.o		\
	todpc_subr.o		\
	tscc_hpet.o		\
	tscc_pit.o		\
	tscc_vmware.o		\
	trap.o			\
	turbo.o			\
	ucode_utils_amd.o	\
	ucode_utils_intel.o	\
	vm_machdep.o		\
	xpv_platform.o		\
	x_call.o

#
#	Add the SMBIOS subsystem object files directly to the list of objects
#	built into unix itself; this is all common code except for smb_dev.c.
#
CORE_OBJS += smb_error.o smb_info.o smb_open.o smb_subr.o smb_dev.o


#
# These get compiled twice:
# - once in the dboot (direct boot) identity mapped code
# - once for use during early startup in unix
#
BOOT_DRIVER_OBJS =		\
	boot_console.o		\
	boot_keyboard.o		\
	boot_keyboard_table.o	\
	boot_vga.o		\
	boot_fb.o		\
	boot_mmu.o		\
	dboot_multiboot2.o	\
	font.o			\
	$(FONT).o

CORE_OBJS += $(BOOT_DRIVER_OBJS)

#
#	locore.o is special. It must be the first file relocated so that it
#	it is relocated just where its name implies.
#
SPECIAL_OBJS +=			\
	locore.o		\
	fast_trap_asm.o		\
	interrupt.o		\
	syscall_asm_amd64.o	\
	kpti_trampolines.o



OBJECTS		= $(SPECIAL_OBJS:%=$(OBJS_DIR)/%) \
		  $(CORE_OBJS:%=$(OBJS_DIR)/%) \
		  $(KRTLD_OBJS:%=$(OBJS_DIR)/%) \
		  $(MACH_NOT_YET_KMODS:%=$(OBJS_DIR)/%)

ROOTMODULE	= $(ROOT_PSM_KERN_DIR)/$(UNIX)

UNIX_BIN	= $(OBJS_DIR)/$(UNIX)

LIBS		= $(GENLIB)

GENUNIX		= genunix
GENUNIX_DIR	= ../../intel/$(GENUNIX)

LIBOPTS		= -L $(GENUNIX_DIR)/$(OBJS_DIR) -l $(GENUNIX)

COMMP_CTF_SRC	= $(OBJS_DIR)/comm_page_ctf.c

CTFEXTRAOBJS	= $(OBJS_DIR)/vers.o $(OBJS_DIR)/comm_page_ctf.o

#
# Objects that get compiled into the identity mapped PT_LOAD section of unix
# to handle the earliest part of booting.
#
DBOOT_OBJS +=			\
	dboot_asm.o		\
	dboot_grub.o		\
	dboot_printf.o		\
	dboot_startkern.o	\
	memcpy.o		\
	memset.o		\
	muldiv.o		\
	sha1.o			\
	string.o		\
	$(BOOT_DRIVER_OBJS)	\
	dboot_elfload.o

DBOOT_OBJS_DIR	= dboot/$(OBJS_DIR)
DBOOT_OBJECTS	= $(DBOOT_OBJS:%=$(DBOOT_OBJS_DIR)/%)
DBOOT_BIN	= $(DBOOT_OBJS_DIR)/$(DBOOT)
DBOOT_O		= $(OBJS_DIR)/$(DBOOT).o
DBOOT_S		= $(DBOOT_O:%.o=%.s)

#
#	Include common rules.
#
include $(UTSBASE)/intel/os/Makefile.unix.com
include $(UTSBASE)/common/os/Makefile.unix.com
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Define targets
#
ALL_TARGET	= $(UNIX_BIN)
INSTALL_TARGET	= $(UNIX_BIN) $(ROOTMODULE)

#
#	This is UNIX_DIR. Use a short path.
#
UNIX_DIR	= .

#
#	Overrides
#
CLEANFILES	+=		\
	$(UNIX_O) $(MODSTUBS_O)	\
	$(OBJS_DIR)/vers.c \
	$(OBJS_DIR)/dtracestubs.s \
	$(DTRACESTUBS_O) $(DTRACESTUBS) \
	$(CTFEXTRAOBJS) \
	$(COMMP_CTF_SRC)

CLEANFILES	+=		\
	$(DBOOT_O) $(DBOOT_S)	\
	$(DBOOT_OBJECTS)	\
	$(OBJS_DIR)/bios_call_src.o \
	$(OBJS_DIR)/bios_call_src \
	$(OBJS_DIR)/bios_call.s \
	$(DBOOT_BIN)

CLEANFILES	+=		\
	$(DBOOT_OBJS_DIR)/$(FONT).c \
	$(OBJS_DIR)/$(FONT).c

CLEANFILES	+=		\
	$(OBJS_DIR)/fb_swtch_src.o \
	$(OBJS_DIR)/fb_swtch_src \
	$(OBJS_DIR)/fb_swtch.s

CLEANFILES	+= $(ZLIB_OBJS:%.o=$(OBJS_DIR)/%.o)

CLOBBERFILES	= $(CLEANFILES) $(UNIX_BIN)

#
#	Font data for generated console fonts
#
FONT		= 8x16
FONT_SRC	= ter-u16b


# instr_size needs a special header
$(OBJS_DIR)/instr_size.o :=	EXTRA_OPTIONS	= -I$(SRC)/common/dis/i386

CFLAGS += -DDIS_MEM

#
# For now, disable these checks; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-char-subscripts
CERRWARN	+= -_gcc=-Wno-unused-function

# false positives
SMOFF += index_overflow

# needs work
SMOFF += all_func_returns

$(OBJS_DIR)/bootrd_cpio.o := SMOFF += allocating_enough_data

# too hairy
$(OBJS_DIR)/inflate.o := SMATCH=off

#
#	Default build targets.
#
.KEEP_STATE:

def: $(DEF_DEPS)

all: $(ALL_DEPS)

clean: $(CLEAN_DEPS)

clobber: $(CLOBBER_DEPS)

install: $(INSTALL_DEPS)

MAPFILE_32 = $(UNIX_MAPFILE)
MAPFILE_64 = $(UNIX_MAPFILE).amd64

MAPFILE_NAME = $(MAPFILE_$(CLASS))

$(OBJS_DIR)/kobj.o		:= CPPFLAGS += -D_DBOOT

$(UNIX_BIN):	$(UNIX_O) $(MODSTUBS_O) $(MAPFILE_NAME) \
		$(GENLIB) $(DTRACESTUBS) $(DBOOT_O)
	$(LD) -dy -b -o $@ -e dboot_image -znointerp -M $(MAPFILE_NAME) \
	    $(UNIX_O) $(DBOOT_O) $(MODSTUBS_O) $(LIBOPTS) \
	    $(DTRACESTUBS)
	$(MBH_PATCH) $(UNIX_BIN)
	$(CTFMERGE_UNIQUIFY_AGAINST_GENUNIX)
	$(POST_PROCESS)

$(UNIX_O):	$(OBJECTS) $(CTFEXTRAOBJS)
	$(LD) -r -o $@ $(OBJECTS) $(OBJS_DIR)/vers.o

$(DBOOT_BIN):	$(DBOOT_OBJS_DIR) $(DBOOT_OBJECTS) dboot/Mapfile.dboot
	$(LD) -dn -e _start -M dboot/Mapfile.dboot \
		-o $(DBOOT_BIN) $(DBOOT_OBJECTS)

$(DBOOT_O):	$(DBOOT_BIN)
	@echo "	.data"					> $(DBOOT_S)
	@echo "	.globl	dboot_image"			>> $(DBOOT_S)
	@echo "dboot_image:"				>> $(DBOOT_S)
	$(ELFEXTRACT) $(DBOOT_BIN)			>> $(DBOOT_S)
	$(COMPILE.s) -o $(DBOOT_O) $(DBOOT_S)

$(DBOOT_OBJS_DIR):
	-@mkdir -p $@ 2> /dev/null

$(COMMP_CTF_SRC):	$(UTSBASE)/i86pc/ml/comm_page.S
	$(COMPILE.cpp) -D_GENCTF -o $@ $(UTSBASE)/i86pc/ml/comm_page.S

$(OBJS_DIR)/comm_page_ctf.o:	$(COMMP_CTF_SRC)
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

#
#	Special rules for generating assym.h for inclusion in assembly files.
#
$(DSF_DIR)/$(OBJS_DIR)/assym.h $(DSF_DIR)/$(OBJS_DIR)/kdi_assym.h:	FRC
	@cd $(DSF_DIR); $(MAKE) all.targ


#
# NB: It is important these be sorted in specificity order, there are places
# where patterns overlap (though thankfully they are rare).
#
# Remember, sort by specificity (machine, platform, arch, common), then by the
# 2nd component.
#
$(OBJS_DIR)/%.o:		$(OBJS_DIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/boot/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/conf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/os/cpupm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/dboot/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/ml/%.S
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/os/%.c
	$(COMPILE.c) -_gcc=-fno-stack-protector -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/promif/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/vm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:               $(UTSBASE)/intel/fs/proc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/kdi/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/kdi/%.S
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/ml/%.S
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/conf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:               $(UTSBASE)/common/disp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/pciex/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/xen/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/decompress.o := CPPFLAGS += -I$(SRC)/contrib/zlib
$(OBJS_DIR)/zmod.o := CPPFLAGS += -I$(SRC)/contrib/zlib
$(OBJS_DIR)/zmod_subr.o := CPPFLAGS += -I$(SRC)/contrib/zlib
$(OBJS_DIR)/%.o:		$(UTSBASE)/common/zmod/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/zlib_obj.o:		$(ZLIB_OBJS:%=$(OBJS_DIR)/%)
	$(LD) -r $(BREDUCE) -M$(UTSBASE)/common/zmod/mapfile -o $@ \
	    $(ZLIB_OBJS:%=$(OBJS_DIR)/%)
	$(CTFMERGE) $(CTFMRGFLAGS) -o $@ $(ZLIB_OBJS:%=$(OBJS_DIR)/%)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/vm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/contrib/zlib/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/atomic/amd64/%.s
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(COMMONBASE)/atomic/amd64/%.S
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(COMMONBASE)/dis/i386/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/fs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/font/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/$(FONT).c:	$(FONT_DIR)/$(FONT_SRC).bdf
	$(VTFONTCVT) -f source -o $@ $(FONT_DIR)/$(FONT_SRC).bdf

$(OBJS_DIR)/%.o:		$(COMMONBASE)/ilstr/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/smbios/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/ucode/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/util/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

#
# krtld compiled into unix
#
#
# krtld must refer to its own bzero/bcopy until the kernel is fully linked
#
$(OBJS_DIR)/bootrd.o		:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/doreloc.o		:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj.o		:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_boot.o		:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_bootflags.o	:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_convrelstr.o	:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_isa.o		:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_kdi.o		:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_lm.o		:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_reloc.o	:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_stubs.o	:= CPPFLAGS += -DKOBJ_OVERRIDES
$(OBJS_DIR)/kobj_subr.o		:= CPPFLAGS += -DKOBJ_OVERRIDES

KRTLD_INC_PATH	= -I$(UTSBASE)/common/krtld -I$(UTSBASE)/intel/sys
KRTLD_INC_PATH	+= -I$(UTSBASE)/intel/amd64/krtld

KRTLD_CPPFLAGS	= -D_KRTLD -DELF_TARGET_AMD64 -DMODDIR_SUFFIX=\"amd64\"

$(OBJS_DIR)/%.o:	$(UTSBASE)/common/krtld/%.c
	$(COMPILE.c) $(KRTLD_INC_PATH) $(KRTLD_CPPFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:	$(UTSBASE)/intel/amd64/krtld/%.S
	$(COMPILE.s) $(KRTLD_INC_PATH) $(KRTLD_CPPFLAGS) -o $@ $<

$(OBJS_DIR)/%.o:	$(UTSBASE)/intel/amd64/krtld/%.c
	$(COMPILE.c) $(KRTLD_INC_PATH) $(KRTLD_CPPFLAGS) -o $@ $<
	$(CTFCONVERT_O)

#
# dboot in unix
#
#
# dboot stuff is always 32 bit, linked to run with phys_addr == virt_addr
#
DBOOT_OBJS_DIR	= dboot/$(OBJS_DIR)
DBOOT_MACH_32	= -D_BOOT_TARGET_i386
DBOOT_MACH_64	= -D_BOOT_TARGET_amd64
DBOOT_DEFS	= -D_BOOT $(DBOOT_MACH_$(CLASS))
DBOOT_DEFS	+= -D_MACHDEP -U_KERNEL -D_I32LPx
DBOOT_FLAGS	= $(CFLAGS_XARCH_32) $(CCVERBOSE) $(CSTD) $(CERRWARN)
DBOOT_FLAGS	+= $(CCNOAUTOINLINE)
DBOOT_ASFLAGS	= $(ASFLAGS_XARCH_32) -D_ASM

DBOOT_CC_INCL	= -I$(SRC)/common -I$(SRC)/common/util $(INCLUDE_PATH)
DBOOT_AS_INCL	= $(INCLUDE_PATH)

$(DBOOT_OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/boot/%.c
	$(i386_CC) $(DBOOT_FLAGS) -O $(DBOOT_DEFS) $(DBOOT_CC_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/dboot/%.c
	$(i386_CC) $(DBOOT_FLAGS) -O $(DBOOT_DEFS) $(DBOOT_CC_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/%.o:		$(SRC)/common/font/%.c
	$(i386_CC) $(DBOOT_FLAGS) -O $(DBOOT_DEFS) $(DBOOT_CC_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/$(FONT).c:	$(FONT_DIR)/$(FONT_SRC).bdf
	$(VTFONTCVT) -f source -o $@ $(FONT_DIR)/$(FONT_SRC).bdf

$(DBOOT_OBJS_DIR)/%.o:		$(DBOOT_OBJS_DIR)/%.c
	$(i386_CC) $(DBOOT_FLAGS) -O $(DBOOT_DEFS) $(DBOOT_CC_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/sha1/%.c
	$(i386_CC) $(DBOOT_FLAGS) -O $(DBOOT_DEFS) $(DBOOT_CC_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/%.o:		$(DBOOT_OBJS_DIR)/%.c
	$(i386_CC) $(DBOOT_FLAGS) -O $(DBOOT_DEFS) $(DBOOT_CC_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/%.o:		$(COMMONBASE)/util/%.c
	$(i386_CC) $(DBOOT_FLAGS) -O $(DBOOT_DEFS) $(DBOOT_CC_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/%.o:		$(COMMONBASE)/util/i386/%.S
	$(AS) $(DBOOT_DEFS) $(DBOOT_ASFLAGS) $(DBOOT_AS_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/dboot/%.S
	$(AS) $(DBOOT_DEFS) $(DBOOT_ASFLAGS) $(DBOOT_AS_INCL) -c -o $@ $<

$(DBOOT_OBJS_DIR)/%.o:		$(UTSBASE)/intel/ml/%.S
	$(AS) $(ASFLAGS_XARCH_32) -D_ASM $(DBOOT_DEFS) $(DBOOT_AS_INCL) \
	    $(DBOOT_ASFLAGS) -c -o $@ $<

#
# Stuff to build bios_call.o for the kernel.
#
MAPFILE_BIOS	= $(UTSBASE)/i86pc/conf/Mapfile.bios
$(OBJS_DIR)/bios_call.o:    $(UTSBASE)/i86pc/ml/bios_call_src.S
	$(COMPILE.s) -o $(OBJS_DIR)/bios_call_src.o \
		$(UTSBASE)/i86pc/ml/bios_call_src.S
	$(LD) -dn -M $(MAPFILE_BIOS)  \
		-o $(OBJS_DIR)/bios_call_src $(OBJS_DIR)/bios_call_src.o
	@echo "  .data"				> $(OBJS_DIR)/bios_call.s
	@echo "  .globl bios_image"		>> $(OBJS_DIR)/bios_call.s
	@echo "bios_image:"			>> $(OBJS_DIR)/bios_call.s
	$(ELFEXTRACT) $(OBJS_DIR)/bios_call_src	>> $(OBJS_DIR)/bios_call.s
	@echo "  .align 4"			>> $(OBJS_DIR)/bios_call.s
	@echo "  .globl bios_size"		>> $(OBJS_DIR)/bios_call.s
	@echo "bios_size:"			>> $(OBJS_DIR)/bios_call.s
	@echo "  .long . - bios_image"		>> $(OBJS_DIR)/bios_call.s
	$(COMPILE.s) -o $@ $(OBJS_DIR)/bios_call.s

#
# Stuff to build fb_swtch.o for the kernel.
#
MAPFILE_FBSWTCH	= $(UTSBASE)/i86pc/conf/Mapfile.fb_swtch
$(OBJS_DIR)/fb_swtch.o:    $(UTSBASE)/i86pc/ml/fb_swtch_src.S
	$(COMPILE.s) -o $(OBJS_DIR)/fb_swtch_src.o \
		$(UTSBASE)/i86pc/ml/fb_swtch_src.S
	$(LD) -dn -M $(MAPFILE_FBSWTCH)  \
		-o $(OBJS_DIR)/fb_swtch_src $(OBJS_DIR)/fb_swtch_src.o
	@echo "  .data"				> $(OBJS_DIR)/fb_swtch.s
	@echo "  .globl fb_swtch_image"		>> $(OBJS_DIR)/fb_swtch.s
	@echo "fb_swtch_image:"			>> $(OBJS_DIR)/fb_swtch.s
	$(ELFEXTRACT) $(OBJS_DIR)/fb_swtch_src	>> $(OBJS_DIR)/fb_swtch.s
	@echo "  .align 4"			>> $(OBJS_DIR)/fb_swtch.s
	@echo "  .globl fb_swtch_size"		>> $(OBJS_DIR)/fb_swtch.s
	@echo "fb_swtch_size:"			>> $(OBJS_DIR)/fb_swtch.s
	@echo "  .long . - fb_swtch_image"	>> $(OBJS_DIR)/fb_swtch.s
	$(COMPILE.s) -o $@ $(OBJS_DIR)/fb_swtch.s

#
# dtrace stubs
#

$(OBJS_DIR)/dtracestubs.s:	$(UNIX_O) $(LIBS)
	$(NM) -u $(UNIX_O) $(LIBS) | \
	$(EGREP) '(__dtrace_probe_|smap_(disable|enable))' | $(SORT) | \
	    $(UNIQ) | $(AWK) '{ \
	    printf("\t.globl %s\n\t.type %s,@function\n%s:\n", \
	    $$1, $$1, $$1); }' > $(OBJS_DIR)/dtracestubs.s

$(DTRACESTUBS):	$(DTRACESTUBS_O)
	$(BUILD.SO) $(DTRACESTUBS_O)

#
#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ


#
# Since the assym headers are generated, there must be explicit dependencies
# on them for all files which include them.
#
# See the use of this variable in uts/Makefile.targ, where this
# dependency is established.
#
ASSYM_DEPS      +=		\
	copy.o			\
	desctbls_asm.o		\
	ddi_i86_asm.o		\
	exception.o		\
	fast_trap_asm.o		\
	float.o			\
	i86_subr.o		\
	interrupt.o		\
	lock_prim.o		\
	locore.o		\
	mpcore.o		\
	sseblk.o		\
	swtch.o			\
	syscall_asm_amd64.o	\
	kpti_trampolines.o	\
	cpr_wakecode.o

$(KDI_ASSYM_DEPS:%=$(OBJS_DIR)/%):	$(DSF_DIR)/$(OBJS_DIR)/kdi_assym.h
