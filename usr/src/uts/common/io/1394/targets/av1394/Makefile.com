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

MODULE		= av1394
MOD_SRCDIR	= $(UTSBASE)/common/io/1394/targets/av1394

OBJS		= \
		av1394.o		\
		av1394_as.o		\
		av1394_async.o		\
		av1394_cfgrom.o		\
		av1394_cmp.o		\
		av1394_fcp.o		\
		av1394_isoch.o		\
		av1394_isoch_chan.o	\
		av1394_isoch_recv.o	\
		av1394_isoch_xmit.o	\
		av1394_list.o		\
		av1394_queue.o

include $(UTSBASE)/Makefile.kmod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-type-limits

# needs work
$(OBJS_DIR)/av1394_isoch.o := SMOFF += signed

DEPENDS_ON	= misc/s1394

include $(UTSBASE)/Makefile.kmod.targ
