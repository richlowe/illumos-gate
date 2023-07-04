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
# Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.


#
#	Define the module and object file sets.
#
MODULE		= igb

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		igb_buf.o	\
		igb_debug.o	\
		igb_gld.o	\
		igb_log.o	\
		igb_main.o	\
		igb_rx.o	\
		igb_stat.o	\
		igb_tx.o	\
		igb_osdep.o	\
		igb_sensor.o

# Intel common code
include		$(SRC)/uts/common/io/e1000api/Makefile.com
OBJS		+= $(E1000API_OBJS)

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/igb

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
SMOFF += all_func_returns,indenting

CFLAGS		+= $(E1000API_CFLAGS)
CFLAGS		+= -I$(UTSBASE)/common/io/igb

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
# Driver depends on MAC
#
LDFLAGS		+= -N misc/mac
MAPFILES	+= ddi mac random kernel ksensor

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
include $(UTSBASE)/common/io/e1000api/Makefile.targ
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/igb/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
