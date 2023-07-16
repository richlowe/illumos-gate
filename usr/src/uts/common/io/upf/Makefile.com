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

MODULE		= upf
MOD_SRCDIR	= $(UTSBASE)/common/io/upf
OBJS		= upf_usbgem.o

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(UTSBASE)/common/io/usbgem
CPPFLAGS	+= -DVERSION=\"2.0.1\"
CPPFLAGS	+= -DUSBGEM_CONFIG_GLDv3

DEPENDS_ON	=	\
	drv/ip		\
	misc/mac	\
	misc/usba	\
	misc/usbgem

CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-label

# needs work
$(OBJS_DIR)/upf_usbgem.o := SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ
