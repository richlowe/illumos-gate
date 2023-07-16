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

MODULE		= igb
MOD_SRCDIR	= $(UTSBASE)/common/io/igb

OBJS		=		\
		igb_buf.o	\
		igb_debug.o	\
		igb_gld.o	\
		igb_log.o	\
		igb_main.o	\
		igb_osdep.o	\
		igb_rx.o	\
		igb_sensor.o	\
		igb_stat.o	\
		igb_tx.o

# Intel common code
include		$(SRC)/uts/common/io/e1000api/Makefile.com
OBJS		+= $(E1000API_OBJS)

include $(UTSBASE)/Makefile.kmod

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
INC_PATH	+= -I$(UTSBASE)/common/io/igb

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= misc/mac
MAPFILES	+= ddi mac random kernel ksensor

include $(UTSBASE)/Makefile.mapfile
include $(UTSBASE)/common/io/e1000api/Makefile.targ
include $(UTSBASE)/Makefile.kmod.targ
