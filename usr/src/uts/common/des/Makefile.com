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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2014 Garrett D'Amore <garrett@damore.org>
#

MODULE		= des
MOD_SRCDIR	= $(UTSBASE)/common/des
OBJS		= des_crypt.o des_impl.o des_ks.o des_soft.o
ROOTMODULE	= $(ROOT_CRYPTO_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_MISC_DIR)/$(MODULE)
LINK_TARGET	= ../../../kernel/crypto/$(SUBDIR64)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/Makefile.kmod

COM_DIR = $(COMMONBASE)/crypto
INSTALL_TARGET	+= $(ROOTLINK)

DEPENDS_ON	= misc/kcf

CPPFLAGS	+= -I$(COM_DIR)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)

include $(UTSBASE)/Makefile.kmod.targ

$(ROOTLINK):	$(ROOT_MISC_DIR) $(ROOTMODULE)
	-$(RM) $@; ln -s $(LINK_TARGET) $@

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/des/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
