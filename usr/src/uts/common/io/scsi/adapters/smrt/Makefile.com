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
# Copyright (c) 2017, Joyent, Inc.
#

MODULE		= smrt
MOD_SRCDIR     = $(UTSBASE)/common/io/scsi/adapters/smrt

OBJS		=			\
		smrt.o			\
		smrt_ciss.o		\
		smrt_ciss_simple.o	\
		smrt_commands.o		\
		smrt_device.o		\
		smrt_hba.o		\
		smrt_interrupts.o	\
		smrt_logvol.o		\
		smrt_physical.o		\
		smrt_sata.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= misc/scsi

include $(UTSBASE)/Makefile.kmod.targ
