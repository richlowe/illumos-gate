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
# Copyright (c) 2018, Joyent, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= cpqary3

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		cpqary3.o		\
		cpqary3_noe.o		\
		cpqary3_talk2ctlr.o	\
		cpqary3_isr.o		\
		cpqary3_transport.o	\
		cpqary3_mem.o		\
		cpqary3_scsi.o		\
		cpqary3_util.o		\
		cpqary3_ioctl.o		\
		cpqary3_bd.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR     = $(UTSBASE)/common/io/cpqary3

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

CPPFLAGS	+= -I$(UTSBASE)/common/io/cpqary3

#
#	Kernel Module Dependencies
#
LDFLAGS		+= -Nmisc/scsi

SMATCH=off

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

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/cpqary3/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
