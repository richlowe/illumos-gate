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
# Use is subject to license
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright 2016, Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

#
#	Define the module and object file sets.
#
MODULE		= cpr

# XXXMK: this object section is constructed to aid wsdiff, it need not be
# this bad, and could be in intel/Makefile were it not for that.
i86pc_OBJS	= cpr_impl.o cpr_wakecode.o
i386_OBJS	= cpr_intel.o

# XXXMK: Beware the difference between MACH and UTSMACH
# Common
OBJS		=			\
		$($(UTSMACH)_OBJS)	\
		cpr_driver.o		\
		cpr_dump.o		\
		cpr_main.o		\
		cpr_misc.o		\
		cpr_mod.o		\
		cpr_stat.o		\
		cpr_uthread.o		\
		$($(MACH)_OBJS)

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	bootdev required as per previous inline commenting referencing symbol
#	i_devname_to_promname(), which may only be necessary on SPARC. Removing
#	this symbol may be sufficient to remove dependency.
#
LDFLAGS		+= -N misc/acpica -N misc/bootdev

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

CFLAGS += $(CCVERBOSE)

CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses
$(OBJS_DIR)/cpr_impl.o :=	CERRWARN	+= -_gcc=-Wno-unused-function

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

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/cpr/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
