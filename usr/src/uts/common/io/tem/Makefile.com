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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

MODULE		= tem
MOD_SRCDIR	= $(UTSBASE)/common/io/tem

aarch64_OBJS	=	\
		font.o	\
		$(FONT).o

OBJS		=		\
		tem.o		\
		tem_safe.o	\
		$($(UTSMACH)_OBJS)

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

aarch64_FONT		= 8x16
aarch64_FONT_SRC	= ter-u16b

FONT		= $($(UTSMACH)_FONT)
FONT_SRC	= $($(UTSMACH)_FONT_SRC)

DEPENDS_ON	= dacf/consconfig_dacf

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(SRC)/common/font/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(OBJS_DIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/$(FONT).c:	$(FONT_DIR)/$(FONT_SRC).bdf
	$(VTFONTCVT) -f source -o $@ $(FONT_DIR)/$(FONT_SRC).bdf
