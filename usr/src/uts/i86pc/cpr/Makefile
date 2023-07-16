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
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright 2016, Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

UTSBASE = ../..
UTSMACH = i86pc

include $(SRC)/uts/common/cpr/Makefile.com

OBJS += 		\
	cpr_impl.o 	\
	cpr_intel.o	\
	cpr_wakecode.o

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/ml/%.S
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/os/%.c
	$(COMPILE.c) -_gcc=-fno-stack-protector -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
