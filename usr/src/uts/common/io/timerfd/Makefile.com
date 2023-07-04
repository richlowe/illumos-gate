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
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= timerfd
MOD_SRCDIR	= $(UTSBASE)/common/io/timerfd
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

CERRWARN	+= -_gcc=-Wno-parentheses

# needs work
SMOFF += all_func_returns

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

include $(UTSBASE)/Makefile.kmod.targ
