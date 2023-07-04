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
# Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
#

MODULE =	s10_brand
MOD_SRCDIR =	$(UTSBASE)/common/brand/solaris10
OBJS =		s10_brand.o s10_brand_asm.o
ROOTMODULE =	$(USR_BRAND_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

#
#	Update compiler variables.
#
INC_PATH +=	-I$(MOD_SRCDIR) -I$(OBJS_DIR)
intel_INC_PATH = -I$(UTSBASE)/i86pc/genassym/$(OBJS_DIR)
INC_PATH +=	$($(UTSMACH)_INC_PATH)

DEPENDS_ON =	exec/elfexec

include $(UTSBASE)/Makefile.kmod.targ
