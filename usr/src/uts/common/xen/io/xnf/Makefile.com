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
# Copyright (c) 2018, Joyent, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= xnf
OBJS		= xnf.o
OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
i86xpv_ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
i86hvm_ROOTMODULE	= $(ROOT_HVM_DRV_DIR)/$(MODULE)
ROOTMODULE		= $($(UTSMACH)_ROOTMODULE)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

i86hvm_CPPFLAGS	= -DHVMPV_XNF_VERS=1
CPPFLAGS	+= $($(UTSMACH)_CPPFLAGS)

#
# Driver depends on MAC & IP
#
LDFLAGS		+= -N misc/mac -N drv/ip
i86hvm_LDFLAGS	= -Ndrv/xpvd -Ndrv/xpv
LDFLAGS 	+= $($(UTSMACH)_LDFLAGS)

#
# use Solaris specific code in xen public header files
#
CFLAGS		+= -D_SOLARIS

# needs work
SMATCH=off

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

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/xen/io/xnf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
