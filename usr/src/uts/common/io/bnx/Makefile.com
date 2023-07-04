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

MODULE		= bnx
MOD_SRCDIR		= $(UTSBASE)/common/io/bnx

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		bnxmod.o	\
		bnxcfg.o	\
	 	bnxdbg.o	\
	 	bnxgldv3.o	\
	 	bnxhwi.o	\
	 	bnxint.o	\
	 	bnxrcv.o	\
	 	bnxsnd.o	\
	 	bnxtmr.o	\
	 	bnx_kstat.o	\
	 	bnx_mm.o	\
	 	bnx_hw_cpu.o	\
	 	bnx_hw_misc.o	\
	 	bnx_hw_nvram.o	\
	 	bnx_hw_phy.o	\
	 	bnx_hw_reset.o	\
	 	bnx_lm_main.o	\
	 	bnx_lm_recv.o	\
	 	bnx_lm_send.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

#
#	Driver-specific flags
#
CPPFLAGS += \
	-D_USE_FRIENDLY_NAME \
	-DEXCLUDE_RSS_SUPPORT \
	-DEXCLUDE_KQE_SUPPORT \
	-DL2_ONLY \
	-DSOLARIS \
	-D_ANSI_C_ \
	-DLM_MAX_MC_TABLE_SIZE=256 \
	-DBRCMVERSION="\"7.10.4\"" \
	-DLITTLE_ENDIAN \
	-DLITTLE_ENDIAN_HOST \
	-D__LITTLE_ENDIAN

INC_PATH += \
	-I$(MOD_SRCDIR) \
	-I$(MOD_SRCDIR)/include \
	-I$(MOD_SRCDIR)/570x/common/include \
	-I$(MOD_SRCDIR)/570x/driver/common/lmdev


DEPENDS_ON	= drv/ip misc/mac

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnx/570x/driver/common/lmdev/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
