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
# Copyright (c) 2017, Joyent, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= smrt

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		smrt.o			\
		smrt_device.o		\
		smrt_interrupts.o	\
		smrt_commands.o		\
		smrt_logvol.o		\
		smrt_hba.o		\
		smrt_ciss_simple.o	\
		smrt_ciss.o		\
		smrt_physical.o		\
		smrt_sata.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR     = $(UTSBASE)/common/io/scsi/adapters/smrt

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Kernel Module Dependencies
#
LDFLAGS		+= -Nmisc/scsi


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
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/scsi/adapters/smrt/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
