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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2014 Garrett D'Amore <garrett@damore.org>
# Copyright (c) 2019, Joyent, Inc.
#

COM_DIR = $(COMMONBASE)/crypto/aes

#
#	Define the module and object file sets.
#
MODULE		= aes
# XXXMK: Could be in the client makefile, if not for wsdiff
intel_OBJS	= aes_amd64.o aes_intel.o aeskey.o
OBJS		= $($(UTSMACH)_OBJS) aes.o aes_impl.o aes_modes.o
OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_CRYPTO_DIR)/$(MODULE)

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
#	Linkage dependencies
#
LDFLAGS += -Nmisc/kcf

CPPFLAGS	+= -I$(COM_DIR) -I$(COM_DIR)/..
CPPFLAGS	+= -DCRYPTO_PROVIDER_NAME=\"$(MODULE)\"
AS_CPPFLAGS	+= -I../../$(PLATFORM)

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

$(OBJS_DIR)/%.o:               $(UTSBASE)/common/crypto/io/aes/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/aes/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
