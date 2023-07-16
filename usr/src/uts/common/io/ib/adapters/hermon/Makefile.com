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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= hermon
MOD_SRCDIR	= $(UTSBASE)/common/io/ib/adapters/hermon

OBJS		=		\
		hermon.o	\
		hermon_agents.o \
		hermon_cfg.o	\
		hermon_ci.o	\
		hermon_cmd.o	\
		hermon_cq.o	\
		hermon_event.o	\
		hermon_fcoib.o	\
		hermon_fm.o	\
		hermon_ioctl.o	\
		hermon_misc.o	\
		hermon_mr.o	\
		hermon_qp.o	\
		hermon_qpmod.o	\
		hermon_rsrc.o	\
		hermon_srq.o	\
		hermon_stats.o	\
		hermon_umap.o	\
		hermon_wr.o

#
#	Include common rules.
#
include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	=	\
	drv/ib		\
	misc/ibmf	\
	misc/ibtl

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-value
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
