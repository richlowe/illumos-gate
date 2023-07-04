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
# Copyright 2016 Garrett D'Amore <garrett@damore.org>
# Copyright (c) 2018, Joyent, Inc.
#


MODULE		= sfxge
MOD_SRCDIR	= $(UTSBASE)/common/io/sfxge

# illumos specific source
# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		sfxge_err.o \
		sfxge_ev.o \
		sfxge_hash.o \
		sfxge_intr.o \
		sfxge_mac.o \
		sfxge_gld_v3.o \
		sfxge_mon.o \
		sfxge_phy.o \
		sfxge_sram.o \
		sfxge_bar.o \
		sfxge_pci.o \
		sfxge_nvram.o \
		sfxge_rx.o \
		sfxge_tcp.o \
		sfxge_tx.o \
		sfxge_mcdi.o \
		sfxge_vpd.o \
		sfxge.o \
		sfxge_dma.o

# OS-independent sources
# XXXMK: Should be sorted, but wsdiff
OBJS		+=		\
		efx_bootcfg.o \
		efx_crc32.o \
		efx_ev.o \
		efx_filter.o \
		efx_hash.o \
		efx_intr.o \
		efx_mac.o \
		efx_mcdi.o \
		efx_mon.o \
		efx_nic.o \
		efx_nvram.o \
		efx_phy.o \
		efx_port.o \
		efx_rx.o \
		efx_sram.o \
		efx_tx.o \
		efx_vpd.o \
		efx_wol.o \
		mcdi_mon.o \
		siena_mac.o \
		siena_mcdi.o \
		siena_nic.o \
		siena_nvram.o \
		siena_phy.o \
		siena_sram.o \
		siena_vpd.o \
		ef10_ev.o \
		ef10_filter.o \
		ef10_intr.o \
		ef10_mac.o \
		ef10_mcdi.o \
		ef10_nic.o \
		ef10_nvram.o \
		ef10_phy.o \
		ef10_rx.o ef10_tx.o \
		ef10_vpd.o \
		hunt_nic.o \
		hunt_phy.o

include $(UTSBASE)/Makefile.kmod

INC_PATH += -I$(UTSBASE)/common/io/sfxge -I$(UTSBASE)/common/io/sfxge/common

#
# TODO:
# These are specific to this driver.  We will unidef these out later.
# Some of them need further cleanup as well (e.g. we shouldn't bother with
# supporting NDD directly.)
#
CPPFLAGS += -U_USE_MTU_UPDATE

DEPENDS_ON	= misc/mac

# needs work
$(OBJS_DIR)/sfxge_ev.o := SMOFF += index_overflow
SMOFF += all_func_returns

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/sfxge/common/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
