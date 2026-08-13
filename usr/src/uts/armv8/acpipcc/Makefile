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
# Copyright 2026 Michael van der Westhuizen
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= acpipcc
OBJECTS		= $(ACPIPCC_OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/armv8/io/acpi/acpipcc

MAPFILE		= $(CONF_SRCDIR)/$(MODULE).mapfile

#
#	Include common rules.
#
include $(UTSBASE)/armv8/Makefile.armv8

#
#       Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
# Depends on the acpica ACPI interpreter and support module
#
LDFLAGS		+= -N misc/acpica
LDFLAGS		+= -M $(MAPFILE)

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
include $(UTSBASE)/armv8/Makefile.targ
