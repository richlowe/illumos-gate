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
# Copyright 2017 Hayashi Naoyuki
#

UTSBASE=../..
include $(UTSBASE)/../Makefile.master
include ../Makefile.armv8

FILEMODE = 644

HDRS= \
	clock.h \
	irq.h \
	mach_intr.h \
	machbrand.h \
	machclock.h \
	machcpuvar.h \
	machparam.h \
	machsystm.h \
	memlist_plat.h \
	memnode.h \
	platmod.h \
	sbd_ioctl.h \
	smp_impldefs.h \
	smt.h \
	ddi_subrdefs.h \
	vm_machparam.h \
	x_call.h \
	xc_levels.h

ROOTHDRS= $(HDRS:%=$(USR_PSM_ISYS_DIR)/%)

CHECKHDRS=	\
	$(HDRS:%.h=%.check)

install_h: $(ROOTHDRS)

$(USR_PSM_ISYS_DIR)/%: % $(USR_PSM_ISYS_DIR)
	$(INS.file)

$(USR_PSM_ISYS_DIR):
	$(INS.dir)

check:	$(CHECKHDRS)
