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

#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= smbfs
MOD_SRCDIR	= $(UTSBASE)/common/fs/smbclnt/smbfs

OBJS		=		\
		smbfs_acl.o	\
		smbfs_client.o	\
		smbfs_node.o	\
		smbfs_ntacl.o	\
		smbfs_rwlock.o	\
		smbfs_smb.o	\
		smbfs_smb1.o	\
		smbfs_smb2.o	\
		smbfs_subr.o	\
		smbfs_subr2.o	\
		smbfs_vfsops.o	\
		smbfs_vnops.o	\
		smbfs_xattr.o

ROOTMODULE	= $(USR_FS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(UTSBASE)/common/fs/smbclnt
INC_PATH	+= -I$(COMMONBASE)/smbclnt

DEPENDS_ON	= drv/nsmb

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)

# The mb_put/md_get functions are intentionally used with and without
# return value checks, so filter those out.
#
# also needs further work.
SMOFF += all_func_returns,signed,deref_check

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(COMMONBASE)/smbclnt/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
