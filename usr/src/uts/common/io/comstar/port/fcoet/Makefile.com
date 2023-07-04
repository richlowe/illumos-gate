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
# Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= fcoet
MOD_SRCDIR	= $(UTSBASE)/common/io/comstar/port/fcoet
OBJS		= fcoet.o fcoet_eth.o fcoet_fc.o

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= drv/stmf drv/fct drv/fcoe

# needs work
SMOFF += all_func_returns,shift_to_zero

include $(UTSBASE)/Makefile.kmod.targ
