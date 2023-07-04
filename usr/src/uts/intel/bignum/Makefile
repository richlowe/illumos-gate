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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2019, Joyent, Inc.
#

UTSBASE = ../..
UTSMACH = intel
include $(SRC)/uts/common/bignum/Makefile.com

intel_OBJS	+= bignum_amd64.o bignum_amd64_asm.o
OBJS		+= $(intel_OBJS)

# See configuration descriptions in Makefile.com
BIGNUM_CFG = -DPSR_MUL

$(OBJS_DIR)/bignumimpl.o := CPPFLAGS += $(BIGNUM_CFG)
$(OBJS_DIR)/bignum_amd64.o := CPPFLAGS += $(BIGNUM_CFG)

$(OBJS_DIR)/%.o: $(BIGNUMDIR)/amd64/%.c
	$(COMPILE.c) -o $@ $(BIGNUM_CFG) $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o: $(BIGNUMDIR)/amd64/%.S
	$(COMPILE.s) -o $@ $(BIGNUM_CFG) $<
