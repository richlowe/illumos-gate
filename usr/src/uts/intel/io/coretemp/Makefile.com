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
# Copyright 2019, Joyent, Inc.
#

MODULE		= coretemp
MOD_SRCDIR	= $(UTSBASE)/intel/io/coretemp

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

#
# Because we need to use cross calls directly, we must include the
# definitions below. Once CMI rdmsr routines have been fixed, we can
# remove this and move out of the platform specific driver world.
#
INC_PATH	+= -I$(UTSBASE)/i86pc/

include $(UTSBASE)/Makefile.kmod.targ
