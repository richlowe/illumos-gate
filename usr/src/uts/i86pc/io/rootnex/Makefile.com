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
# Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= rootnex
MOD_SRCDIR	= $(UTSBASE)/i86pc/io/rootnex

OBJS		=		\
		rootnex.o

i86pc_OBJS	=		\
		immu.o		\
		immu_dmar.o	\
		immu_dvma.o	\
		immu_intrmap.o	\
		immu_qinv.o	\
		immu_regs.o

OBJS 		+= $($(UTSMACH)_OBJS)

ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

i86pc_ALL_TARGET	= $(SRC_CONFFILE)
i86pc_INSTALL_TARGET	= $(ROOT_CONFFILE)

ALL_TARGET		+= $($(UTSMACH)_ALL_TARGET)
INSTALL_TARGET		+= $($(UTSMACH)_INSTALL_TARGET)

i86pc_DEPENDS_ON	= misc/iommulib misc/acpica
DEPENDS_ON		= $($(UTSMACH)_DEPENDS_ON)

#
# For now, disable these checks; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
$(OBJS_DIR)/immu_qinv.o := SMOFF += index_overflow

include $(UTSBASE)/Makefile.kmod.targ
