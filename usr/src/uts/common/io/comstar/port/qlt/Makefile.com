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
#

MODULE		= qlt
MOD_SRCDIR	= $(UTSBASE)/common/io/comstar/port/qlt
OBJS		=	\
	2400.o		\
	2500.o		\
	2700.o		\
	8100.o		\
	8300fc.o	\
	qlt.o		\
	qlt_dma.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= drv/fct drv/stmf
CFLAGS		+= -DUNIQUE_FW_NAME

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
#
CERRWARN	+= $(CNOWARN_UNINIT)

include $(UTSBASE)/Makefile.kmod.targ
