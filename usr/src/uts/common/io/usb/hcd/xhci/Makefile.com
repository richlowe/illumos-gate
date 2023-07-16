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
# Copyright 2016 Joyent, Inc.
#


MODULE		= xhci
MOD_SRCDIR	= $(UTSBASE)/common/io/usb/hcd/xhci

OBJS		=		\
		xhci.o		\
		xhci_command.o	\
		xhci_context.o	\
		xhci_dma.o	\
		xhci_endpoint.o \
		xhci_event.o	\
		xhci_hub.o	\
		xhci_intr.o	\
		xhci_polled.o	\
		xhci_quirks.o	\
		xhci_ring.o	\
		xhci_usba.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= misc/usba

include $(UTSBASE)/Makefile.kmod.targ
