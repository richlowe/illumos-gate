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
# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright 2019 Joyent, Inc.
#

MODULE		= sol_uverbs
MOD_SRCDIR	= $(UTSBASE)/common/io/ib/clients/of/sol_uverbs

OBJS		=			\
		sol_uverbs.o		\
		sol_uverbs_comp.o	\
		sol_uverbs_event.o	\
		sol_uverbs_hca.o	\
		sol_uverbs_qp.o

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/ibtl misc/sol_ofs

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-label

# really broken
SMOFF += logical_instead_of_bitwise,or_vs_and

# false positive
SMOFF += signed

include $(UTSBASE)/Makefile.kmod.targ
