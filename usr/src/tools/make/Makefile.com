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

# Copyright 2015, Richard Lowe.
# Copyright 2020 Joyent, Inc.

MAKE_INCLUDE= $(SRC)/cmd/make/include

CC = $(NATIVECC)
CCC = $(NATIVECCC)
CFLAGS = $(NATIVE_CFLAGS)
CFLAGS += $(CCVERBOSE)
CCFLAGS = $(NATIVE_CCFLAGS) -_gcc4=-std=gnu++0x
CPPFLAGS = -I$(MAKE_INCLUDE) $(MAKE_DEFS)
CCNEEDED = $(NATIVECCNEEDED)
