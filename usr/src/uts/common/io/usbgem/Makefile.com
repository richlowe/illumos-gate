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
# Copyright 2022 Garrett D'Amore
#

MODULE		= usbgem
MOD_SRCDIR	= $(UTSBASE)/common/io/usbgem
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	=	\
	drv/ip		\
	misc/mac	\
	misc/usba

#
#	The USBGEM has support for various different features. We use
#	these pre-processor macros to define the set we care about.
#
CPPFLAGS	+= \
		-DMODULE \
		-DVERSION=\"1.6\"

CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-unused-function

# needs work
SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ
