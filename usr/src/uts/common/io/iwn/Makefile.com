#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2016 Hans Rosenfeld <rosenfeld@grumpf.hope-2000.org>
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= iwn
MOD_SRCDIR	= $(UTSBASE)/common/io/iwn
OBJS		= if_iwn.o
ROOTFIRMWARE	= $(FWFILES:%=$(ROOT_FIRMWARE_DIR)/$(MODULE)/%)

include $(UTSBASE)/Makefile.kmod


#
#	Define firmware location & files
#
FWDIR	= $(UTSBASE)/common/io/iwn/fw-iw
FWFILES	= iwlwifi-100-5.ucode iwlwifi-1000-3.ucode iwlwifi-105-6.ucode \
	iwlwifi-135-6.ucode iwlwifi-2000-6.ucode iwlwifi-2030-6.ucode \
	iwlwifi-4965-2.ucode iwlwifi-5000-2.ucode iwlwifi-5150-2.ucode \
	iwlwifi-6000-4.ucode iwlwifi-6000g2a-6.ucode iwlwifi-6000g2b-6.ucode \
	iwlwifi-6050-5.ucode

INSTALL_TARGET	+= $(ROOTFIRMWARE)

DEPENDS_ON	= misc/mac misc/net80211 drv/random drv/ip

# needs work
SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ
