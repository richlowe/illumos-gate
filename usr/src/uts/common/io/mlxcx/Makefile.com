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
# Copyright 2018 Joyent, Inc.
#


MODULE		= mlxcx
MOD_SRCDIR	= $(UTSBASE)/common/io/mlxcx

OBJS		=		\
		mlxcx.o		\
		mlxcx_cmd.o	\
		mlxcx_dma.o	\
		mlxcx_gld.o	\
		mlxcx_intr.o	\
		mlxcx_ring.o	\
		mlxcx_sensor.o

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(UTSBASE)/common/io/mlxcx

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= misc/mac

include $(UTSBASE)/Makefile.kmod.targ
