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
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= pcmcia
MOD_SRCDIR	= $(UTSBASE)/common/pcmcia

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		pcmcia.o	\
		cs.o		\
		cis.o		\
		cis_callout.o	\
		cis_handlers.o	\
		cis_params.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

intel_INC_PATH	=  -I$(UTSBASE)/i86pc
INC_PATH        += $($(UTSMACH)_INC_PATH)

DEPENDS_ON	= misc/busra misc/pci_autoconfig

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/pcmcia/cis/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/pcmcia/cs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/pcmcia/nexus/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
