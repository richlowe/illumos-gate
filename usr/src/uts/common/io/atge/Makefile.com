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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

MODULE		= atge
MOD_SRCDIR	= $(UTSBASE)/common/io/atge

# XXXMK: should be sorted, but wsdiff
OBJS		=		\
		atge_main.o	\
		atge_l1e.o	\
		atge_mii.o	\
		atge_l1.o	\
		atge_l1c.o

include $(UTSBASE)/Makefile.kmod

CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-switch

DEPENDS_ON	= misc/mac misc/mii

include $(UTSBASE)/Makefile.kmod.targ
