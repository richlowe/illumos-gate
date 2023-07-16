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

MODULE		= mac
MOD_SRCDIR	= $(UTSBASE)/common/io/mac

OBJS		=			\
		mac.o			\
		mac_bcast.o		\
		mac_client.o		\
		mac_datapath_setup.o	\
		mac_flow.o		\
		mac_hio.o		\
		mac_mod.o		\
		mac_ndd.o		\
		mac_protect.o		\
		mac_provider.o		\
		mac_sched.o		\
		mac_soft_ring.o		\
		mac_stat.o		\
		mac_util.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(UTSBASE)/common/io/bpf

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-variable

# needs work
SMOFF += all_func_returns
$(OBJS_DIR)/mac_util.o := SMOFF += signed

include $(UTSBASE)/Makefile.kmod.targ
