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
# Copyright 2021 Oxide Computer Company
#

MODULE		= ena
MOD_SRCDIR	= $(UTSBASE)/common/io/ena

OBJS		=		\
		ena.o		\
		ena_admin.o	\
		ena_aenq.o	\
		ena_dma.o	\
		ena_gld.o	\
		ena_hw.o	\
		ena_intr.o	\
		ena_rx.o	\
		ena_stats.o	\
		ena_tx.o	\
		ena_watchdog.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(MOD_SRCDIR)

DEPENDS_ON	= misc/mac
MAPFILES	= ddi mac kernel

include $(UTSBASE)/Makefile.mapfile
include $(UTSBASE)/Makefile.kmod.targ
