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
# Copyright (c) 2014 Joyent, Inc.  All rights reserved.
#

MODULE		= eventfd
MOD_SRCDIR	= $(UTSBASE)/common/io/eventfd
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

CERRWARN	+= -_gcc=-Wno-parentheses

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

include $(UTSBASE)/Makefile.kmod.targ
