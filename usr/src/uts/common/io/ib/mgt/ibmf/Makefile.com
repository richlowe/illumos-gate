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

MODULE		= ibmf
MOD_SRCDIR	= $(UTSBASE)/common/io/ib/mgt/ibmf
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		ibmf.o \
		ibmf_impl.o \
		ibmf_dr.o \
		ibmf_wqe.o \
		ibmf_ud_dest.o \
		ibmf_mod.o \
		ibmf_send.o \
		ibmf_recv.o \
		ibmf_handlers.o \
		ibmf_trans.o \
		ibmf_timers.o \
		ibmf_msg.o \
		ibmf_utils.o \
		ibmf_rmpp.o \
		ibmf_saa.o \
		ibmf_saa_impl.o \
		ibmf_saa_utils.o \
		ibmf_saa_events.o

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/ibtl

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
