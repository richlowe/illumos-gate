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
#

MODULE		= autofs
MOD_SRCDIR	= $(UTSBASE)/common/fs/autofs

# XXXMK: should be sorted but wsdiff
OBJS		=		\
		auto_vfsops.o	\
		auto_vnops.o	\
		auto_subr.o	\
		auto_xdr.o	\
		auto_sys.o

ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_SYS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INSTALL_TARGET	+= $(ROOTLINK)

DEPENDS_ON = strmod/rpcmod misc/rpcsec fs/mntfs

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= $(CNOWARN_UNINIT)

include $(UTSBASE)/Makefile.kmod.targ

$(ROOTLINK):	$(ROOT_SYS_DIR) $(ROOTMODULE)
	-$(RM) $@; ln $(ROOTMODULE) $@
