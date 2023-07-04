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

#
#	Define the module and object file sets.
#
MODULE =	s10_brand
OBJS =		s10_brand.o s10_brand_asm.o
OBJECTS =	$(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE =	$(USR_BRAND_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

S10_BASE =	$(UTSBASE)/common/brand/solaris10

#
#	Define targets
#
ALL_TARGET =		$(BINARY)
INSTALL_TARGET =	$(BINARY) $(ROOTMODULE)


#
#	Update compiler variables.
#
INC_PATH +=	-I$(S10_BASE) -I$(OBJS_DIR)
intel_INC_PATH = -I$(UTSBASE)/i86pc/genassym/$(OBJS_DIR)
INC_PATH +=	$($(UTSMACH)_INC_PATH)

LDFLAGS +=	-Nexec/elfexec

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/brand/solaris10/%.S
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/brand/solaris10/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
