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
# Copyright (c) 1999, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright 2019 Joyent, Inc.
# Copyright 2018 Nexenta Systems, Inc.
# Copyright 2019 Peter Tribble.
# Copyright 2022 Oxide Computer Company
#

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
#	Microcode
#
CORE_OBJS += microcode.o

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
