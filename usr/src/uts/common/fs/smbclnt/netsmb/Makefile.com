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
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= nsmb
MOD_SRCDIR	= $(UTSBASE)/common/fs/smbclnt/netsmb

# XXXMK: should be sorted, but wsdiff
OBJS		=		\
		smb_conn.o	\
		smb_dev.o	\
		smb_iod.o	\
		smb_pass.o	\
		smb_rq.o	\
		smb_sign.o	\
		smb_smb.o	\
		smb_subrs.o	\
		smb_time.o	\
		smb_tran.o	\
		smb_trantcp.o	\
		smb_usr.o	\
		smb2_rq.o	\
		smb2_sign.o	\
		smb2_smb.o	\
		subr_mchain.o	\
		nsmb_sign_kcf.o

ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)
OFFSETS_SRC	= $(CONF_SRCDIR)/offsets.in
IOC_CHECK_H	= $(OBJS_DIR)/ioc_check.h

#
#	Include common rules.
#
include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(UTSBASE)/common/fs/smbclnt

DEPENDS_ON      = crypto/md4 crypto/md5 misc/tlimod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#

# The mb_put/md_get functions are intentionally used with and without
# return value checks, so filter those.
SMOFF += all_func_returns

# needs work
SMOFF += signed,deref_check

include $(UTSBASE)/Makefile.kmod.targ

#
# Create ioc_check.h and compare with the saved
# ioc_check.ref to ensure 32/64-bit invariance.
#
$(IOC_CHECK_H): $(OFFSETS_SRC)
	$(OFFSETS_CREATE) <$(OFFSETS_SRC) >$@.tmp
	cmp -s ioc_check.ref $@.tmp && \
	  mv -f $@.tmp $@

$(NOT_AARCH64_BLD)$(OBJECTS) : $(IOC_CHECK_H)
