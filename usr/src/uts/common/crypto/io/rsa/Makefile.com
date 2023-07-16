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
#

MODULE		= rsa
MOD_SRCDIR	= $(UTSBASE)/common/crypto/io/rsa
OBJS		= rsa.o rsa_impl.o pkcs1.o
ROOTMODULE	= $(ROOT_CRYPTO_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

COM1_DIR = $(COMMONBASE)/bignum
COM2_DIR = $(COMMONBASE)/crypto

DEPENDS_ON =		\
	crypto/md5	\
	crypto/sha2	\
	misc/bignum	\
	misc/kcf

CPPFLAGS	+= -I$(COM1_DIR) -I$(COM2_DIR)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/padding/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/rsa/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
