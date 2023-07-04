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


#
#	Define the module and object file sets.
#
MODULE		= hci1394

OBJS		=			\
		hci1394.o		\
		hci1394_async.o		\
		hci1394_attach.o	\
		hci1394_buf.o		\
		hci1394_csr.o		\
		hci1394_detach.o	\
		hci1394_extern.o	\
		hci1394_ioctl.o		\
		hci1394_isoch.o		\
		hci1394_isr.o		\
		hci1394_ixl_comp.o	\
		hci1394_ixl_isr.o	\
		hci1394_ixl_misc.o	\
		hci1394_ixl_update.o	\
		hci1394_misc.o		\
		hci1394_ohci.o		\
		hci1394_q.o		\
		hci1394_s1394if.o	\
		hci1394_tlabel.o	\
		hci1394_tlist.o		\
		hci1394_vendor.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/1394/adapters
LDFLAGS		+= -Nmisc/s1394

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Overrides
#
#ALL_BUILDS	= $(ALL_BUILDSONLY64)
#DEF_BUILDS	= $(DEF_BUILDSONLY64)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

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

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/1394/adapters/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
