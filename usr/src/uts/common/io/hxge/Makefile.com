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
# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= hxge
MOD_SRCDIR	= $(UTSBASE)/common/io/hxge

OBJS		=		\
		hpi.o		\
		hpi_pfc.o	\
		hpi_rxdma.o	\
		hpi_txdma.o	\
		hpi_vir.o	\
		hpi_vmac.o	\
		hxge_fm.o	\
		hxge_fzc.o	\
		hxge_hw.o	\
		hxge_kstats.o	\
		hxge_main.o	\
		hxge_ndd.o	\
		hxge_pfc.o	\
		hxge_rxdma.o	\
		hxge_send.o	\
		hxge_txdma.o	\
		hxge_virtual.o	\
		hxge_vmac.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(UTSBASE)/common/io/hxge

CPPFLAGS	+= -DSOLARIS

#
# Debug flags
#
# CPPFLAGS += -DHXGE_DEBUG -DHPI_DEBUG
#

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

SMOFF += deref_check,logical_instead_of_bitwise

DEPENDS_ON	= misc/mac drv/ip

include $(UTSBASE)/Makefile.kmod.targ
