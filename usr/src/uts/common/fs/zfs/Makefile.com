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
# Copyright 2013 Saso Kiselkov. All rights reserved.
# Copyright (c) 2016 by Delphix. All rights reserved.
# Copyright 2019 Joyent, Inc.
#

MODULE		= zfs
MOD_SRCDIR	= $(UTSBASE)/common/fs/zfs

# Object lists definitions are shared with libzpool in userland
include		$(UTSBASE)/common/fs/zfs/Makefile.zfs

intel_OBJS	= spa_boot.o
aarch64_OBJS	= spa_boot.o
OBJS		= $($(UTSMACH)_OBJS) $(ZFS_OBJS) $(LUA_OBJS)

ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_FS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOTLINK) $(ROOT_CONFFILE)

DEPENDS_ON	=	\
	fs/specfs	\
	crypto/swrand	\
	misc/idmap	\
	misc/sha2	\
	misc/skein	\
	misc/edonr

intel_INC_PATH		+= -I$(UTSBASE)/i86pc
aarch64_INC_PATH	+= -I$(UTSBASE)/armv8

INC_PATH	+= -I$(UTSBASE)/common/fs/zfs
INC_PATH	+= -I$(UTSBASE)/common/fs/zfs/lua
INC_PATH	+= -I$(SRC)/common
INC_PATH	+= -I$(COMMONBASE)/zfs
INC_PATH	+= $($(UTSMACH)_INC_PATH)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-label

# needs work
SMOFF += all_func_returns,indenting

$(OBJS_DIR)/llex.o	:= SMOFF += index_overflow
$(OBJS_DIR)/metaslab.o	:= SMOFF += no_if_block
$(OBJS_DIR)/zfs_vnops.o	:= SMOFF += signed
$(OBJS_DIR)/zvol.o	:= SMOFF += deref_check,signed

# false positive
$(OBJS_DIR)/zfs_ctldir.o := SMOFF += strcpy_overflow

include $(UTSBASE)/Makefile.kmod.targ

$(ROOTLINK):	$(ROOT_FS_DIR) $(ROOTMODULE)
	-$(RM) $@; ln $(ROOTMODULE) $@

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/zfs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/fs/zfs/lua/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/zfs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/lz4.o := CPPFLAGS += -I$(COMMONBASE)/lz4
$(OBJS_DIR)/%.o:		$(COMMONBASE)/lz4/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
