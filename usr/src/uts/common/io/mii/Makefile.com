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

MODULE		= mii
MOD_SRCDIR	= $(UTSBASE)/common/io/mii

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		mii.o		\
		mii_cicada.o	\
		mii_natsemi.o	\
		mii_intel.o	\
		mii_qualsemi.o	\
		mii_marvell.o	\
		mii_realtek.o	\
		mii_other.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/mac

CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ
