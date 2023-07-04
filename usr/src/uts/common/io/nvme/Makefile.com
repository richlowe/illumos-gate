#
# CDDL HEADER START
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
# CDDL HEADER END
#
#
# Copyright 2015 Nexenta Systems, Inc. All rights reserved.
#


#
#	Define the module and object file sets.
#
MODULE		= nvme

# XXXMK: These should be sorted, but wsdiff
OBJS	=		\
	nvme.o		\
	nvme_validate.o \
	nvme_lock.o	\
	nvme_stat.o	\
	nvme_feature.o	\
	nvme_field.o	\
	nvme_firmware.o \
	nvme_format.o	\
	nvme_identify.o \
	nvme_log.o	\
	nvme_version.o	\
	nvme_vuc.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/nvme
#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
# Bits required for common source.
#
CPPFLAGS	+= -I$(SRC)/common/nvme

#
# Driver depends on blkdev
#
LDFLAGS		+= -N drv/blkdev -N misc/sata

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

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/nvme/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/nvme/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
