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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= dtrace
MOD_SRCDIR	= $(UTSBASE)/common/dtrace/dtrace
OBJS		= dtrace.o dtrace_isa.o dtrace_asm.o

include $(UTSBASE)/Makefile.kmod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
$(OBJS_DIR)/dtrace.o := SMOFF += signed_integer_overflow_check,deref_check

CPPFLAGS	+= -I$(SRC)/common/util

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(DSF_DIR)/$(OBJS_DIR)
ASSYM_H		= $(DSF_DIR)/$(OBJS_DIR)/assym.h

include $(UTSBASE)/Makefile.kmod.targ

$(BINARY):	$(ASSYM_H)

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/dtrace/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/dtrace/%.S
	$(COMPILE.s) -o $@ $<
