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
# Copyright (c) 2016 by Delphix. All rights reserved.
#

MODULE		= dev
MOD_SRCDIR	= $(UTSBASE)/common/fs/dev

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		sdev_subr.o	\
		sdev_vfsops.o	\
		sdev_vnops.o	\
		sdev_ptsops.o	\
		sdev_zvolops.o	\
		sdev_comm.o	\
		sdev_profile.o	\
		sdev_ncache.o	\
		sdev_netops.o	\
		sdev_ipnetops.o	\
		sdev_vtops.o	\
		sdev_plugin.o
ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= fs/devfs misc/dls

CFLAGS		+= $(CCVERBOSE)

INC_PATH	+= -I$(UTSBASE)/common/fs/zfs
INC_PATH	+= -I$(UTSBASE)/common/io/bpf

CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-unused-function

include $(UTSBASE)/Makefile.kmod.targ
