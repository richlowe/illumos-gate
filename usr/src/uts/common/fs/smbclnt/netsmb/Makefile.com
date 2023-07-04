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
# Copyright 2024 RackTop Systems, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= nsmb

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

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/fs/smbclnt/netsmb
OFFSETS_SRC	= $(CONF_SRCDIR)/offsets.in
IOC_CHECK_H	= $(OBJS_DIR)/ioc_check.h

MAPFILE		= $(CONF_SRCDIR)/$(MODULE).mapfile
MAPFILE_EXT	= $(CONF_SRCDIR)/nsmb_ext.mapfile
MAPFILES	+= tlimod ddi

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(ALL_TARGET_$(OBJS_DIR))
INSTALL_TARGET	= $(INSTALL_TARGET_$(OBJS_DIR))

#
#	Overrides.
#
#	We need some unusual overrides here so we'll
#	build ioc_check.h for both 32-bit/64-bit,
#	but only build 64-bit binaries.
#

DEF_BUILDS	= $(DEF_BUILDS64)
ALL_BUILDS	= $(ALL_BUILDS64)
ALL_TARGET_debug64	= $(BINARY) $(SRC_CONFILE)
ALL_TARGET_obj64	= $(BINARY) $(SRC_CONFILE)
INSTALL_TARGET_debug64	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)
INSTALL_TARGET_obj64	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
# Now the normal overrides...
#
INC_PATH	+= -I$(UTSBASE)/common/fs/smbclnt
LDFLAGS         += -Ncrypto/md4 -Ncrypto/md5 -Nmisc/kcf -Nmisc/tlimod
LDFLAGS		+= -M $(MAPFILE) -M $(MAPFILE_EXT)

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

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/Makefile.mapfile
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/fs/smbclnt/netsmb/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

#
# Create ioc_check.h and compare with the saved
# ioc_check.ref to ensure 32/64-bit invariance.
#
$(IOC_CHECK_H): $(OFFSETS_SRC)
	$(OFFSETS_CREATE) <$(OFFSETS_SRC) >$@.tmp
	cmp -s ioc_check.ref $@.tmp && \
	  mv -f $@.tmp $@

# If we have a 32bit as well as a 64bit build in userland, check invariance.
$(BUILD32)$(OBJECTS) : $(IOC_CHECK_H)
