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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= ibtl
MOD_SRCDIR	= $(UTSBASE)/common/io/ib/ibtl

OBJS		=		\
		ibtl_chan.o	\
		ibtl_cm.o	\
		ibtl_cq.o	\
		ibtl_handlers.o \
		ibtl_hca.o	\
		ibtl_ibnex.o	\
		ibtl_impl.o	\
		ibtl_mcg.o	\
		ibtl_mem.o	\
		ibtl_part.o	\
		ibtl_qp.o	\
		ibtl_srq.o	\
		ibtl_util.o	\
		ibtl_wr.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CFLAGS		+= $(CCVERBOSE)
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-value

# needs work
$(OBJS_DIR)/ibtl_impl.o := SMOFF += precedence

include $(UTSBASE)/Makefile.kmod.targ
