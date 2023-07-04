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
# Copyright 2019 Joyent, Inc.
#

MODULE		= sda
MOD_SRCDIR	= $(UTSBASE)/common/io/sdcard/impl

OBJS		=		\
		sda_cmd.o	\
		sda_host.o	\
		sda_init.o	\
		sda_mem.o	\
		sda_mod.o	\
		sda_slot.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

MAPFILE		= $(UTSBASE)/common/io/sdcard/impl/mapfile
DEPENDS_ON	= drv/blkdev
LDFLAGS 	+= $(BREDUCE) -M $(MAPFILE)

# needs work
SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ
