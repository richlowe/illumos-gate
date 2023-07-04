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
# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= rdsv3
MOD_SRCDIR	= $(UTSBASE)/common/io/ib/clients/rdsv3

# XXXMK: should be sorted, but wsdiff
OBJS		=			\
		af_rds.o \
		rdsv3_ddi.o \
		bind.o \
		loop.o \
		threads.o \
		connection.o \
		transport.o \
		cong.o \
		sysctl.o \
		message.o \
		rds_recv.o \
		send.o \
		stats.o \
		info.o \
		page.o \
		rdma_transport.o \
		ib_ring.o \
		ib_rdma.o \
		ib_recv.o \
		ib.o \
		ib_send.o \
		ib_sysctl.o \
		ib_stats.o \
		ib_cm.o \
		rdsv3_sc.o \
		rdsv3_debug.o \
		rdsv3_impl.o \
		rdma.o \
		rdsv3_af_thr.o

#
#	Include common rules.
#
include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	=	\
	fs/sockfs	\
	misc/ksocket	\
	drv/ip		\
	misc/ibtl	\
	misc/ibcm	\
	misc/sol_ofs

# CFLAGS		+= -DOFA_SOLARIS

#
# Disable these warnings since some errors suppressed here are in the OFED
# code, but we'd like to keep it as is as much as possible.  Note. maintainers
# should endeavor to investigate and remove these for maximum coverage, please
# do not carry these forward to new Makefiles blindly.
#
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

# needs work
SMATCH=off

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

include $(UTSBASE)/Makefile.kmod.targ
