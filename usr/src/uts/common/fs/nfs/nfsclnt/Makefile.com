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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= nfs
MOD_SRCDIR	= $(UTSBASE)/common/fs/nfs/nfsclnt

OBJS		=			\
		nfs3_vfsops.o		\
		nfs3_vnops.o		\
		nfs3_xdr.o		\
		nfs4_acache.o		\
		nfs4_acl.o		\
		nfs4_attr.o		\
		nfs4_callback.o		\
		nfs4_client.o		\
		nfs4_client_debug.o	\
		nfs4_client_secinfo.o	\
		nfs4_client_state.o	\
		nfs4_common.o		\
		nfs4_idmap.o		\
		nfs4_recovery.o		\
		nfs4_rnode.o		\
		nfs4_shadow.o		\
		nfs4_stub_vnops.o	\
		nfs4_subr.o		\
		nfs4_vfsops.o		\
		nfs4_vnops.o		\
		nfs4_xdr.o		\
		nfs4x_xdr.o		\
		nfs_acl_vnops.o		\
		nfs_acl_xdr.o		\
		nfs_client.o		\
		nfs_cmd.o		\
		nfs_common.o		\
		nfs_dump.o		\
		nfs_stats.o		\
		nfs_strerror.o		\
		nfs_subr.o		\
		nfs_sys.o		\
		nfs_vfsops.o		\
		nfs_vnops.o		\
		nfs_xdr.o

ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_SYS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INSTALL_TARGET	+= $(ROOTLINK)

DEPENDS_ON =		\
	fs/specfs	\
	misc/rpcsec	\
	strmod/rpcmod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-empty-body

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ

$(ROOTLINK):	$(ROOT_SYS_DIR) $(ROOTMODULE)
	-$(RM) $@; ln $(ROOTMODULE) $@
