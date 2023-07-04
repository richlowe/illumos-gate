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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#


#
#       Define the module and object file sets.
#
MODULE		= cpu.generic

OBJS		=		\
		gcpu_main.o	\
		gcpu_mca.o	\
		gcpu_poll_subr.o

i86pc_OBJS	= gcpu_poll_ntv.o
i86xpv_OBJS	= gcpu_mca_xpv.o gcpu_poll_xpv.o

OBJS		+= $($(UTSMACH)_OBJS)

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE      = $(ROOT_PSM_CPU_DIR)/$(MODULE)

#
#       Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

i86xpv_CERRWARN = -_gcc=-Wno-unused-variable
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= $($(UTSMACH)_CERRWARN)

#
#       Define targets
#
ALL_TARGET      = $(BINARY)
INSTALL_TARGET  = $(BINARY) $(ROOTMODULE)

#
#       Default build targets.
#
.KEEP_STATE:

def:            $(DEF_DEPS)

all:            $(ALL_DEPS)

clean:          $(CLEAN_DEPS)

clobber:        $(CLOBBER_DEPS)

install:        $(INSTALL_DEPS)

#
#       Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

# NB: This really is i86pc, not $(UTSMACH).  The i86xpv shares these sources
$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/cpu/generic_cpu/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
