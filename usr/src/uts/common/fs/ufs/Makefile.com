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

MODULE		= ufs
MOD_SRCDIR	= $(UTSBASE)/common/fs/ufs

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		ufs_alloc.o	\
		ufs_bmap.o	\
		ufs_dir.o	\
		ufs_xattr.o	\
		ufs_inode.o	\
		ufs_subr.o	\
		ufs_tables.o	\
		ufs_vfsops.o	\
		ufs_vnops.o	\
		quota.o		\
		quotacalls.o	\
		quota_ufs.o	\
		ufs_filio.o	\
		ufs_lockfs.o	\
		ufs_thread.o	\
		ufs_trans.o	\
		ufs_acl.o	\
		ufs_panic.o	\
		ufs_directio.o	\
		ufs_log.o	\
		ufs_extvnops.o	\
		ufs_snap.o	\
		lufs.o		\
		lufs_thread.o	\
		lufs_log.o	\
		lufs_map.o	\
		lufs_top.o	\
		lufs_debug.o

ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= fs/specfs misc/fssnap_if

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-unused-label

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
