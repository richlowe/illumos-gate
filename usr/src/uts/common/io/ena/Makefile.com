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
# Copyright 2021 Oxide Computer Company
#


MODULE		= ena

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		ena.o		\
		ena_admin.o	\
		ena_dma.o	\
		ena_aenq.o	\
		ena_gld.o	\
		ena_hw.o	\
		ena_intr.o	\
		ena_stats.o	\
		ena_tx.o	\
		ena_rx.o	\
		ena_watchdog.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/ena

include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

CPPFLAGS	+= -I$(UTSBASE)/common/io/ena

ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINBAR) $(ROOTMODULE) $(ROOT_CONFFILE)

LDFLAGS		+= -N misc/mac

MAPFILES	+= ddi mac kernel

.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

include $(UTSBASE)/Makefile.mapfile
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:	$(UTSBASE)/common/io/ena/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
