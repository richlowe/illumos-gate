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

MODULE		= aes
MOD_SRCDIR	= $(UTSBASE)/common/crypto/io/aes

# XXXMK: Could be in the client makefile, if not for wsdiff
intel_OBJS	= aes_amd64.o aes_intel.o aeskey.o
OBJS		= $($(UTSMACH)_OBJS) aes.o aes_impl.o aes_modes.o
ROOTMODULE	= $(ROOT_CRYPTO_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

COM_DIR = $(COMMONBASE)/crypto/aes

DEPENDS_ON = misc/kcf

CPPFLAGS	+= -I$(COM_DIR) -I$(COM_DIR)/..
CPPFLAGS	+= -DCRYPTO_PROVIDER_NAME=\"$(MODULE)\"
AS_CPPFLAGS	+= -I../../$(PLATFORM)

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/aes/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
