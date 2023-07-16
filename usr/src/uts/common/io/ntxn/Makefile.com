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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= ntxn
MOD_SRCDIR	= $(UTSBASE)/common/io/ntxn

OBJS		=		\
		niu.o		\
		unm_gem.o	\
		unm_ndd.o	\
		unm_nic_ctx.o	\
		unm_nic_hw.o	\
		unm_nic_init.o	\
		unm_nic_isr.o	\
		unm_nic_main.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(CONF_SRCDIR)

CFLAGS		+=  -DSOLARIS11 -xO4
CFLAGS64	+=  -DSOLARIS11 -xO -I./

DEPENDS_ON	= misc/mac drv/ip

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-type-limits

# needs work
SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ
