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
# Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= srpt
MOD_SRCDIR	= $(UTSBASE)/common/io/comstar/port/srpt

OBJS		=		\
		srpt_ch.o	\
		srpt_cm.o	\
		srpt_ioc.o	\
		srpt_mod.o	\
		srpt_stp.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	=	\
	drv/stmf	\
	misc/ibcm	\
	misc/ibtl

CERRWARN	+= -_gcc=-Wno-unused-label

# needs work
$(OBJS_DIR)/srpt_stp.o := SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ
