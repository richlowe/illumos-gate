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
# Copyright 2013 Nexenta Inc.  All rights reserved.
# Copyright 2019 Joyent, Inc.
#

MODULE		= vioif
MOD_SRCDIR	= $(UTSBASE)/common/io/vioif

include $(UTSBASE)/Makefile.kmod

INC_PATH	 += -I$(UTSBASE)/common/io/virtio

DEPENDS_ON	= misc/virtio misc/mac

include $(UTSBASE)/Makefile.kmod.targ
