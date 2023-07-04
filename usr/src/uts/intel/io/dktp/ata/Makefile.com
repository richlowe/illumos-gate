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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
#


#
#	Define the module and object file sets.
#
MODULE		= ata

# GHD
# XXXMK: should be sorted but wsdiff
OBJS		=		\
		ghd.o		\
		ghd_debug.o	\
		ghd_dma.o	\
		ghd_queue.o	\
		ghd_scsa.o	\
		ghd_scsi.o	\
		ghd_timer.o	\
		ghd_waitq.o	\
		ghd_gcmd.o

# ATA
OBJS		+=		\
		ata_blacklist.o \
		ata_common.o	\
		ata_disk.o	\
		ata_dma.o	\
		atapi.o		\
		atapi_fsm.o	\
		ata_debug.o	\
		sil3xxx.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/intel/io/dktp/ata

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
#	Overrides.
#
#DEBUG_FLGS	= -DATA_DEBUG -DGHD_DEBUG -DDEBUG
DEBUG_FLGS	=
DEBUG_DEFS	+= $(DEBUG_FLGS)
INC_PATH	+= -I$(UTSBASE)/intel/io/dktp/ghd

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)

#
# Depends on scsi
#
LDFLAGS         += -N misc/scsi

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

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/dktp/ata/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/dktp/ghd/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
