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

MODULE		= net80211
MOD_SRCDIR	= $(UTSBASE)/common/io/net80211

OBJS		=			\
		net80211.o		\
		net80211_amrr.o		\
		net80211_crypto.o	\
		net80211_crypto_ccmp.o	\
		net80211_crypto_none.o	\
		net80211_crypto_tkip.o	\
		net80211_crypto_wep.o	\
		net80211_ht.o		\
		net80211_input.o	\
		net80211_ioctl.o	\
		net80211_node.o		\
		net80211_output.o	\
		net80211_proto.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	=	\
	drv/ip		\
	mac/mac_wifi	\
	misc/mac

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
