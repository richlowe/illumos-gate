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
# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2019, Joyent, Inc.
#

COMMONBASE = ../../../common
BIGNUMDIR = $(COMMONBASE)/bignum
CRYPTODIR = $(COMMONBASE)/crypto

#
#	Define the module and object file sets.
#
MODULE		= bignum

OBJS		= bignum_mod.o bignumimpl.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
# Linkage dependencies
#
LDFLAGS += -Nmisc/kcf

CPPFLAGS	+= -I$(BIGNUMDIR) -I$(CRYPTODIR)

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

# Bignum configuration (BIGNUM_CFG):
#   PSR_MUL:
#       There is a processor-specific implementation bignum multiply functions
#   HWCAP:
#       There are multiple implementations of bignum functions, and the
#	appropriate one must be chosen at run time, based on testing
#	hardware capabilities.
#
# -DPSR_MUL:
# For AMD64, there is a processor-specific implementation of
# the bignum multiply functions, which takes advantage of the
# 64x64->128 bit multiply instruction.
#
# -UHWCAP:
# There is only one implementation, because the 128 bit multiply using
# general-purpose registers is faster than any MMX or SSE2 implementation.

CFLAGS	+= -xO4

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/bignum/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/bignum/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

# Note that there are machine-specific rules for this module in machine makefiles
