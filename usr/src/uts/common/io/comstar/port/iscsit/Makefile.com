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
# Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= iscsit
MOD_SRCDIR	= $(UTSBASE)/common/io/comstar/port/iscsit

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		iscsit_common.o	\
		iscsit.o \
		iscsit_tgt.o \
		iscsit_sess.o \
		iscsit_login.o \
		iscsit_text.o \
		iscsit_isns.o \
		iscsit_radiusauth.o \
		iscsit_radiuspacket.o \
		iscsit_auth.o \
		iscsit_authclient.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= drv/stmf misc/idm fs/sockfs misc/md5 misc/ksocket

CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-unused-label

# needs work
SMOFF += cast_assign,strcpy_overflow

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(COMMONBASE)/iscsit/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
