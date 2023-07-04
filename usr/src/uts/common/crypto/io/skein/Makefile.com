#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://opensource.org/licenses/CDDL-1.0.
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
# Copyright 2013 Saso Kiselkov.  All rights reserved.
#

MODULE		= skein
MOD_SRCDIR	= $(UTSBASE)/common/crypto/io/skein
OBJS		= skein.o skein_block.o skein_iv.o skein_mod.o
ROOTMODULE	= $(ROOT_CRYPTO_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

COMDIR	= $(COMMONBASE)/crypto

INSTALL_TARGET	+= $(ROOTLINK)

DEPENDS_ON = misc/kcf

CFLAGS += -I$(COMDIR)

include $(UTSBASE)/Makefile.kmod.targ

$(ROOTLINK):	$(ROOT_MISC_DIR) $(ROOTMODULE)
	-$(RM) $@; ln $(ROOTMODULE) $@

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/skein/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
