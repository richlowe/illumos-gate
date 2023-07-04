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
# Copyright 2019 Joyent, Inc.
#


MODULE          = overlay
MOD_SRCDIR	= $(UTSBASE)/common/io/overlay

OBJS		=			\
		overlay.o		\
		overlay_fm.o		\
		overlay_mux.o		\
		overlay_plugin.o	\
		overlay_prop.o		\
		overlay_target.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET      += $(SRC_CONFFILE)
INSTALL_TARGET  += $(ROOT_CONFFILE)

MAPFILE		= $(UTSBASE)/common/io/overlay/overlay.mapfile

DEPENDS_ON	= misc/mac drv/dld misc/dls misc/ksocket

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
