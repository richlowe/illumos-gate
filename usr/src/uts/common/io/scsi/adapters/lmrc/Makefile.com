#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2023 Racktop Systems, Inc.
#

#
#	Define the module and object file sets.
#
MODULE		= lmrc

OBJS		=	\
	lmrc_ddi.o	\
	lmrc_ioctl.o	\
	lmrc_phys.o	\
	lmrc_raid.o	\
	lmrc_scsa.o	\
	lmrc.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/scsi/adapters/lmrc/

#
#	Kernel Module Dependencies
#
LDFLAGS += -Nmisc/scsi -Ndrv/scsi_vhci -Nmisc/sata

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFFILE)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(ALL_DEPS)

def:		$(DEF_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ


$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/scsi/adapters/lmrc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
