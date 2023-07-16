#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= nxge
MOD_SRCDIR	= $(UTSBASE)/common/io/nxge

OBJS		=			\
		nxge_classify.o		\
		nxge_espc.o		\
		nxge_fflp.o		\
		nxge_fflp_hash.o	\
		nxge_fm.o		\
		nxge_fzc.o		\
		nxge_hio.o		\
		nxge_hio_guest.o	\
		nxge_hv.o		\
		nxge_hw.o		\
		nxge_intr.o		\
		nxge_ipp.o		\
		nxge_kstats.o		\
		nxge_mac.o		\
		nxge_main.o		\
		nxge_ndd.o		\
		nxge_rxdma.o		\
		nxge_send.o		\
		nxge_txc.o		\
		nxge_txdma.o		\
		nxge_virtual.o		\
		nxge_zcp.o

OBJS 		+=		\
		npi.o		\
		npi_espc.o	\
		npi_fflp.o	\
		npi_ipp.o	\
		npi_mac.o	\
		npi_rxdma.o	\
		npi_txc.o	\
		npi_txdma.o	\
		npi_vir.o	\
		npi_zcp.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

#
# Include nxge specific header files
#
INC_PATH	+= -I$(UTSBASE)/common
INC_PATH	+= -I$(UTSBASE)/common/io/nxge/npi
INC_PATH	+= -I$(UTSBASE)/common/sys/nxge

CFLAGS += -DSOLARIS

CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-type-limits

$(OBJS_DIR)/nxge_hw.o := SMOFF += deref_check
$(OBJS_DIR)/npi_txc.o := SMOFF += shift_to_zero

DEPENDS_ON	= misc/mac drv/ip

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/nxge/npi/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
