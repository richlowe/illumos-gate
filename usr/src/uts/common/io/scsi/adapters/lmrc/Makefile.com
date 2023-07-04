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
# Copyright 2023 Racktop Systems, Inc.
#

MODULE		= lmrc
MOD_SRCDIR	= $(UTSBASE)/common/io/scsi/adapters/lmrc/

OBJS		=	\
	lmrc_ddi.o	\
	lmrc_ioctl.o	\
	lmrc_phys.o	\
	lmrc_raid.o	\
	lmrc_scsa.o	\
	lmrc.o

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/scsi drv/scsi_vhci misc/sata

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

include $(UTSBASE)/Makefile.kmod.targ
