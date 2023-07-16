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

#
# Copyright 2024 Michael van der Westhuizen
# Copyright 2017 Hayashi Naoyuki
#

CORE_OBJS_FDT =			\
	cpuinfo_fdt.o

CORE_OBJS +=			\
	aarch64_mmu.o		\
	aarch64_subr.o		\
	arch_kdi.o		\
	arch_timer.o		\
	bcmp.o			\
	bcopy.o			\
	bitmap_arch.o		\
	boot_console.o		\
	bzero.o			\
	cbe.o			\
	confunix.o		\
	copy.o			\
	cpc_subr.o		\
	cpuid.o			\
	cpupm_mach.o		\
	ddi_aarch64.o		\
	ddi_arch.o		\
	ddi_impl.o		\
	dtrace_subr.o		\
	earlybsa.o		\
	exceptions.o		\
	fakebop.o		\
	gic.o			\
	hardclk.o		\
	hat_aarch64.o		\
	hat_kdi.o		\
	hat_kpm.o		\
	hment.o			\
	hold_page.o		\
	htable.o		\
	ibft.o			\
	intr.o			\
	ip_ocsum.o		\
	kdi_asm.o		\
	kdi_exceptions.o	\
	kdi_util.o		\
	lgrpplat.o		\
	lock_prim.o		\
	locore.o		\
	mach_kdi.o		\
	mach_sysconfig.o	\
	machdep.o		\
	mem_config_arch.o	\
	mem_config_stubs.o	\
	memchr.o		\
	memcmp.o		\
	memcpy.o		\
	memmove.o		\
	memnode.o		\
	memset.o		\
	mlsetup.o		\
	mp_call.o		\
	mp_machdep.o		\
	mp_startup.o		\
	notes.o			\
	ovbcopy.o		\
	polled_io.o		\
	ppage.o			\
	psci.o			\
	sendsig.o		\
	ssp.o			\
	startup.o		\
	strlen.o		\
	sundep.o		\
	swtch.o			\
	trap.o			\
	vm_machdep.o		\
	vpanic.o		\
	x_call.o		\
	$(CORE_OBJS_FDT)

#
#	PROM Routines,
#
#	XXXARM: called earlier in boot than usual and so in 'unix'
#	it'd be nice if they weren't, but I doubt that's possible right now
#
CORE_OBJS +=			\
	fdt.o			\
	fdt_empty_tree.o	\
	fdt_ro.o		\
	fdt_rw.o		\
	fdt_strerror.o		\
	fdt_sw.o		\
	fdt_wip.o		\
	prom_enter.o		\
	prom_env.o		\
	prom_exit.o		\
	prom_getchar.o		\
	prom_node.o		\
	prom_panic.o		\
	prom_printf.o		\
	prom_putchar.o		\
	prom_reboot.o		\
	prom_utils.o		\
	prom_version.o

CORE_OBJS +=		\
	decompress.o	\
	pci_strings.o	\
	prmachdep.o

#
#	Kernel linker
#
KRTLD_OBJS +=			\
	bootfsops.o		\
	bootrd.o		\
	bootrd_cpio.o		\
	doreloc.o		\
	hsfs.o			\
	kobj_convrelstr.o	\
	kobj_isa.o		\
	kobj_reloc.o		\
	ufsops.o
