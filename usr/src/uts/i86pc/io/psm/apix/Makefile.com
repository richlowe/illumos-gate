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
# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2019, Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= apix
MOD_SRCDIR	= $(UTSBASE)/i86pc/io/psm/apix

OBJS		=			\
		apic_common.o		\
		apic_regops.o		\
		apic_timer.o		\
		apix.o			\
		apix_intr.o		\
		apix_irm.o		\
		apix_regops.o		\
		apix_utils.o		\
		mp_platform_common.o	\
		psm_common.o

ROOTMODULE	= $(ROOT_PSM_MACH_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/acpica

# needs work
$(OBJS_DIR)/psm_common.o := SMOFF += deref_check

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/io/psm/apix/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/io/psm/pcplusmp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/io/psm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
