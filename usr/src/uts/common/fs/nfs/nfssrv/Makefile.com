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

MODULE		= nfssrv
MOD_SRCDIR	= $(UTSBASE)/common/fs/nfs/nfssrv

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		nfs_server.o		\
		nfs_srv.o		\
		nfs3_srv.o		\
		nfs_acl_srv.o		\
		nfs_auth.o		\
		nfs_auth_xdr.o		\
		nfs_export.o		\
		nfs_log.o		\
		nfs_log_xdr.o		\
		nfs4_srv.o		\
		nfs4_state.o		\
		nfs4_srv_attr.o		\
		nfs4_srv_ns.o		\
		nfs4_db.o		\
		nfs4_srv_deleg.o	\
		nfs4_deleg_ops.o	\
		nfs4_srv_readdir.o	\
		nfs4_dispatch.o		\
		nfs4x_srv.o		\
		nfs4x_state.o		\
		nfs4x_dispatch.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= strmod/rpcmod fs/nfs misc/rpcsec misc/klmmod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-switch

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
