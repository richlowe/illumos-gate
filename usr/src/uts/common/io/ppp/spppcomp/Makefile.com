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

#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright 2019 Joyent, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= spppcomp

# XXXMK: should be sorted, but wsdiff
OBJS		=		\
		spppcomp.o	\
		spppcomp_mod.o	\
		deflate.o	\
		bsd-comp.o	\
		vjcompress.o	\
		zlib.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(USR_STRMOD_DIR)/$(MODULE)

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
#	Internal build definitions
#
CPPFLAGS	+= -DINTERNAL_BUILD -DSOL2 -DMUX_FRAME

#
#	Additional compiler definitions
#
INC_PATH	+= -I$(UTSBASE)/common/io/ppp/common

CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
SMOFF += indenting,index_overflow

#
# Depends on sppp
#
LDFLAGS         += -N drv/sppp

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

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/ppp/spppcomp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
