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

MODULE		= bge
MOD_SRCDIR	= $(UTSBASE)/common/io/bge

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		bge_main2.o	\
		bge_chip2.o	\
		bge_kstats.o	\
		bge_log.o	\
		bge_ndd.o	\
		bge_atomic.o	\
		bge_mii.o	\
		bge_send.o	\
		bge_recv2.o	\
		bge_mii_5906.o

include	$(UTSBASE)/Makefile.kmod

INSTALL_TARGET	+= $(ROOT_CONFFILE)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-function

# 3rd party source
SMOFF	+= all_func_returns

#
# Driver depends on MAC
#
DEPENDS_ON	= misc/mac

include $(UTSBASE)/Makefile.kmod.targ
