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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= rsm
MOD_SRCDIR	= $(UTSBASE)/common/io/rsm
OBJS		= rsm.o rsmka_pathmanager.o rsmka_util.o
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH += -I $(UTSBASE)/common/sys/rsm

DEPENDS_ON	= misc/rsmops

#
# Flag to enable for DR testing via drtest driver
#
#CFLAGS	+=  -DRSM_DRTEST
#LDFLAGS	+= -Ndrv/rsm_drtest

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
