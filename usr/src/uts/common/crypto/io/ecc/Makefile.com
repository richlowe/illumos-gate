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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2014 Garrett D'Amore <garrett@damore.org>
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= ecc
MOD_SRCDIR	= $(UTSBASE)/common/crypto/io/ecc

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		ecc.o		\
		ec.o		\
		ec2_163.o	\
		ec2_mont.o	\
		ecdecode.o	\
		ecl_mult.o	\
		ecp_384.o	\
		ecp_jac.o	\
		ec2_193.o	\
		ecl.o		\
		ecp_192.o	\
		ecp_521.o	\
		ecp_jm.o	\
		ec2_233.o	\
		ecl_curve.o	\
		ecp_224.o	\
		ecp_aff.o	\
		ecp_mont.o	\
		ec2_aff.o	\
		ec_naf.o	\
		ecl_gf.o	\
		ecp_256.o	\
		mp_gf2m.o	\
		mpi.o		\
		mplogic.o	\
		mpmontg.o	\
		mpprime.o	\
		oid.o		\
		secitem.o	\
		ec2_test.o	\
		ecp_test.o

ROOTMODULE	= $(ROOT_CRYPTO_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

COM1_DIR = $(COMMONBASE)/mpi
COM2_DIR = $(COMMONBASE)/crypto

DEPENDS_ON = misc/kcf

CPPFLAGS	+= -I$(COM1_DIR) -I$(COM2_DIR)

CFLAGS		+= -DMP_API_COMPATIBLE -DNSS_ECC_MORE_THAN_SUITE_B

CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-empty-body
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-parentheses

# false positive
$(OBJS_DIR)/ecdecode.o := SMOFF += assign_vs_compare
$(OBJS_DIR)/ec.o := SMOFF += assign_vs_compare

SMOFF += indenting,all_func_returns

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(COMMONBASE)/crypto/ecc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/mpi/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
