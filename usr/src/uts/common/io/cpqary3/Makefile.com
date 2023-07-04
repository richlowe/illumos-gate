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

MODULE		= cpqary3
MOD_SRCDIR     = $(UTSBASE)/common/io/cpqary3

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		cpqary3.o		\
		cpqary3_noe.o		\
		cpqary3_talk2ctlr.o	\
		cpqary3_isr.o		\
		cpqary3_transport.o	\
		cpqary3_mem.o		\
		cpqary3_scsi.o		\
		cpqary3_util.o		\
		cpqary3_ioctl.o		\
		cpqary3_bd.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(UTSBASE)/common/io/cpqary3

DEPENDS_ON	= misc/scsi

SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
