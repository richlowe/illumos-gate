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
# Copyright 2019 Joyent, Inc.
#

MODULE		= simnet
MOD_SRCDIR	= $(UTSBASE)/common/io/$(MODULE)
OBJS		= simnet.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	=	\
	drv/dld		\
	misc/mac	\
	misc/dls	\
	drv/random

CFLAGS		+= $(CCVERBOSE)
CERRWARN	+= -_gcc=-Wno-switch

# needs work
$(OBJS_DIR)/simnet.o := SMOFF += index_overflow

include $(UTSBASE)/Makefile.kmod.targ
