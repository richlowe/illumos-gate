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
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= e1000g
MOD_SRCDIR	= $(UTSBASE)/common/io/e1000g

# XXXMK: should be sorted, but wsdiff
OBJS		=		\
		e1000g_debug.o	\
		e1000g_main.o	\
		e1000g_alloc.o	\
		e1000g_tx.o	\
		e1000g_rx.o	\
		e1000g_stat.o	\
		e1000g_osdep.o	\
		e1000g_workarounds.o

# Intel common code
include		$(SRC)/uts/common/io/e1000api/Makefile.com

OBJS		+= $(E1000API_OBJS)

include $(UTSBASE)/Makefile.kmod

CFLAGS += -D_KERNEL -Di386   -DNEWSTAT -DNOMUT -DRCVWORKAROUND \
	  -DINTEL_IP \
	  -DPAXSON  -DBAY_CITY \
	  -DTANAX_WORKAROUND -I$(UTSBASE)/common/io/e1000g \
	  $(E1000API_CFLAGS)

CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-unused-variable

SMOFF += all_func_returns,indenting,shift_to_zero

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= misc/mac
MAPFILES	= ddi mac

include $(UTSBASE)/Makefile.mapfile
include $(UTSBASE)/common/io/e1000api/Makefile.targ
include $(UTSBASE)/Makefile.kmod.targ
