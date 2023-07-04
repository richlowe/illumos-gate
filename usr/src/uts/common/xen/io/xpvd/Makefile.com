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

MODULE		= xpvd
MOD_SRCDIR	= $(UTSBASE)/common/xen/io/xpvd

i86xpv_ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
i86hvm_ROOTMODULE	= $(ROOT_HVM_DRV_DIR)/$(MODULE)
ROOTMODULE		= $($(UTSMACH)_ROOTMODULE)

include $(UTSBASE)/Makefile.kmod

i86hvm_ALL_TARGET	= $(SRC_CONFFILE)
i86hvm_INSTALL_TARGET	= $(ROOT_CONFFILE)

ALL_TARGET	+= $($(UTSMACH)_ALL_TARGET)
INSTALL_TARGET	+= $($(UTSMACH)_INSTALL_TARGET)

CERRWARN	+= $(CNOWARN_UNINIT)

i86hvm_CPPFLAGS		= -DHVMPV_XPVD_VERS=1
i86hvm_DEPENDS_ON	= drv/xpv

CPPFLAGS	+= $($(UTSMACH)_CPPFLAGS)
DEPENDS_ON	= $($(UTSMACH)_DEPENDS_ON)

include $(UTSBASE)/Makefile.kmod.targ
