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


#
#	Define the module and object file sets.
#
MODULE		= sockfs

# XXXMK: Should be sorted but wsdiff
OBJS		=			\
		socksubr.o		\
		sockvfsops.o		\
		sockparams.o		\
		socksyscalls.o		\
		socktpi.o		\
		sockstr.o		\
		sockcommon_vnops.o	\
		sockcommon_subr.o	\
		sockcommon_sops.o	\
		sockcommon.o		\
		sock_notsupp.o		\
		socknotify.o		\
		sodirect.o		\
		sockfilter.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

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
#	Overrides.
#
LDFLAGS         += -Ndrv/ip

#
#	Derived file "nl7ctokgen.h" defines.
#
SRCDIR		 = $(UTSBASE)/common/fs/sockfs
TOKGEN		 = $(SRCDIR)/nl7ctokgen
DERIVED_FILES	 = nl7ctokgen.h
CFLAGS		+= -I.

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)
		$(RM) $(DERIVED_FILES)

clobber:	$(CLOBBER_DEPS)
		$(RM) $(DERIVED_FILES)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/fs/sockfs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

#
#	Derived file gen.
#
$(SRCDIR)/nl7chttp.c: nl7ctokgen.h

nl7ctokgen.h: $(TOKGEN) $(SRCDIR)/nl7ctokreq.txt $(SRCDIR)/nl7ctokres.txt
	/bin/ksh $(TOKGEN) $(SRCDIR)/nl7ctokreq.txt $(SRCDIR)/nl7ctokres.txt >$@
