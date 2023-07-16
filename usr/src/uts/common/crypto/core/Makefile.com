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
# Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2019, Joyent, Inc.

MODULE		= kcf
MOD_SRCDIR	= $(UTSBASE)/common/crypto/core

OBJS		=			\
		cbc.o			\
		ccm.o			\
		ctr.o			\
		ecb.o			\
		fips_random.o		\
		gcm.o			\
		kcf.o			\
		kcf_callprov.o		\
		kcf_cbufcall.o		\
		kcf_cipher.o		\
		kcf_crypto.o		\
		kcf_cryptoadm.o		\
		kcf_ctxops.o		\
		kcf_digest.o		\
		kcf_dual.o		\
		kcf_keys.o		\
		kcf_mac.o		\
		kcf_mech_tabs.o		\
		kcf_miscapi.o		\
		kcf_object.o		\
		kcf_policy.o		\
		kcf_prov_lib.o		\
		kcf_prov_tabs.o		\
		kcf_random.o		\
		kcf_sched.o		\
		kcf_session.o		\
		kcf_sign.o		\
		kcf_spi.o		\
		kcf_verify.o		\
		modes.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

COM_DIR = $(COMMONBASE)/crypto

CFLAGS		+= -I$(COM_DIR)
AS_CPPFLAGS	+= -I../../$(PLATFORM)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label

# needs work
SMOFF += all_func_returns,signed_integer_overflow_check

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/crypto/api/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/crypto/spi/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/modes/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/rng/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
