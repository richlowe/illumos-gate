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
# Copyright (c) 1991, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2011, 2014 by Delphix. All rights reserved.
# Copyright (c) 2013 by Saso Kiselkov. All rights reserved.
# Copyright 2018 Nexenta Systems, Inc.
# Copyright 2022 Garrett D'Amore
# Copyright 2020 Joyent, Inc.
# Copyright 2016 OmniTI Computer Consulting, Inc.  All rights reserved.
# Copyright 2016 Hans Rosenfeld <rosenfeld@grumpf.hope-2000.org>
# Copyright 2022 RackTop Systems, Inc.
# Copyright 2023 Oxide Computer Company
#

i386_CORE_OBJS += \
		atomic.o	\
		avintr.o	\
		pic.o

aarch64_CORE_OBJS +=			\
			atomic.o	\
			avintr.o

CORE_OBJS +=				\
		beep.o			\
		bitset.o		\
		bp_map.o		\
		brand.o			\
		cpucaps.o		\
		cmt.o			\
		cmt_policy.o		\
		cpu.o			\
		cpu_uarray.o		\
		cpu_event.o		\
		cpu_intr.o		\
		cpu_pm.o		\
		cpupart.o		\
		cap_util.o		\
		disp.o			\
		group.o			\
		kstat_fr.o		\
		iscsiboot_prop.o	\
		lgrp.o			\
		lgrp_topo.o		\
		mmapobj.o		\
		mutex.o			\
		page_lock.o		\
		page_retire.o		\
		panic.o			\
		param.o			\
		pg.o			\
		pghw.o			\
		putnext.o		\
		rctl_proc.o		\
		rwlock.o		\
		seg_kmem.o		\
		softint.o		\
		string.o		\
		strtol.o		\
		strtoul.o		\
		strtoll.o		\
		strtoull.o		\
		ilstr.o			\
		thread_intr.o		\
		vm_page.o		\
		vm_pagelist.o		\
		zlib_obj.o		\
		clock_tick.o		\
		$($(MACH)_CORE_OBJS)

ZLIB_OBJS =			\
		zutil.o		\
		zmod.o		\
		zmod_subr.o	\
		adler32.o	\
		crc32.o		\
		deflate.o	\
		inffast.o	\
		inflate.o	\
		inftrees.o	\
		trees.o

KRTLD_OBJS +=				\
		kobj_bootflags.o	\
		getoptstr.o		\
		kobj.o			\
		kobj_kdi.o		\
		kobj_lm.o		\
		kobj_subr.o
