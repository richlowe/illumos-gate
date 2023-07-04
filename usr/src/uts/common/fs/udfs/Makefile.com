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
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= udfs
MOD_SRCDIR	= $(UTSBASE)/common/fs/udfs
OBJS		=		\
		udf_alloc.o	\
		udf_bmap.o	\
		udf_dir.o	\
		udf_inode.o	\
		udf_subr.o	\
		udf_vfsops.o	\
		udf_vnops.o
ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON = fs/specfs

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-type-limits

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
