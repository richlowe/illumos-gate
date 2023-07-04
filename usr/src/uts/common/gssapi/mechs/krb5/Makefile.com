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
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= kmech_krb5

OBJS		= krb5mech.o

# Mech
# XXXMK: Should be sorted, but wsdiff
OBJS		+=			\
		delete_sec_context.o	\
		import_sec_context.o	\
		gssapi_krb5.o		\
		k5seal.o		\
		k5unseal.o		\
		k5sealv3.o		\
		ser_sctx.o		\
		sign.o			\
		util_crypt.o		\
		util_validate.o		\
		util_ordering.o		\
		util_seqnum.o		\
		util_set.o		\
		util_seed.o		\
		wrap_size_limit.o	\
		verify.o

# Seal
OBJS		+=	\
		seal.o	\
		unseal.o

# Mech gen
OBJS		+= util_token.o

# crypto
# XXXMK: Should be sorted, but wsdiff
OBJS		+=			\
		cksumtypes.o		\
		decrypt.o		\
		encrypt.o		\
		encrypt_length.o	\
		etypes.o		\
		nfold.o			\
		verify_checksum.o	\
		prng.o			\
		block_size.o		\
		make_checksum.o		\
		checksum_length.o	\
		hmac.o			\
		default_state.o		\
		mandatory_sumtype.o

# crypto/des
# XXXMK: Should be sorted but wsdiff
OBJS		+=		\
		f_cbc.o		\
		f_cksum.o	\
		f_parity.o	\
		weak_key.o	\
		d3_cbc.o	\
		ef_crypto.o

# crypto/dk
OBJS		+= \
		checksum.o	\
		derive.o	\
		dk_decrypt.o	\
		dk_encrypt.o

# crypto/arcfour
OBJS		+= k5_arcfour.o

# crypto/enc_provider
# XXXMK: Should be sorted, but wsdiff
OBJS		+= \
		des.o			\
		des3.o			\
		arcfour_provider.o	\
		aes_provider.o

# crypto/hash_provider
# XXXMK: Should be sorted, but wsdiff
OBJS		+=			\
		hash_kef_generic.o	\
		hash_kmd5.o		\
		hash_crc32.o		\
		hash_ksha1.o

# crytpo/keyhash_provider
OBJS		+=		\
		descbc.o	\
		k5_kmd5des.o	\
		k_hmac_md5.o

# crypto/crc32
OBJS		+= crc32.o

# crypto/old
OBJS		+= old_decrypt.o old_encrypt.o

# crypto/raw
OBJS		+= raw_decrypt.o raw_encrypt.o

# krb
# XXXMK: Should be sorted, but wsdiff
OBJS		+=		\
		kfree.o		\
		copy_key.o	\
		parse.o		\
		init_ctx.o	\
		ser_adata.o	\
		ser_addr.o	\
		ser_auth.o	\
		ser_cksum.o	\
		ser_key.o	\
		ser_princ.o	\
		serialize.o	\
		unparse.o	\
		ser_actx.o

# OS
# XXXMK: Should be sorted, but wsdiff
OBJS		+=		\
		timeofday.o	\
		toffset.o	\
		init_os_ctx.o	\
		c_ustime.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_KGSS_DIR)/$(MODULE)

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
# Defined kgssapi and md5 as depdencies
#
LDFLAGS += -N misc/kgssapi -N misc/md5

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

# needs work
SMOFF += indenting

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:        $(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

KGSSDFLAGS=-I $(UTSBASE)/common/gssapi/include
KGSSDFLAGS += $(KRB5_DEFS)

INC_PATH += \
	-I$(UTSBASE)/common/gssapi \
	-I$(UTSBASE)/common/gssapi/include \
	-I$(UTSBASE)/common/gssapi/mechs/krb5/include \
	-I$(SRC)/lib/gss_mechs/mech_krb5/include \
	-I$(SRC)/lib/gss_mechs/mech_krb5/krb5/krb

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

KMECHKRB5_BASE=$(UTSBASE)/common/gssapi/mechs/krb5


$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/crc32/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)


$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/des/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/arcfour/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/dk/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/enc_provider/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/hash_provider/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/keyhash_provider/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/raw/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/crypto/old/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/krb5/krb/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/krb5/os/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/ser_sctx.o := CPPFLAGS += -DPROVIDE_KERNEL_IMPORT=1

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/mech/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(KMECHKRB5_BASE)/profile/%.c
	$(COMPILE.c) $(KGSSDFLAGS) -o $@ $<
	$(CTFCONVERT_O)
