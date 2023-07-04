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
# Copyright (c) 2019, Joyent, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= bnxe

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		bnxe_cfg.o		\
		bnxe_fcoe.o		\
		bnxe_debug.o		\
		bnxe_gld.o		\
		bnxe_hw.o		\
		bnxe_intr.o		\
		bnxe_kstat.o		\
		bnxe_lock.o		\
		bnxe_main.o		\
		bnxe_mm.o		\
		bnxe_mm_l4.o		\
		bnxe_mm_l5.o		\
		bnxe_rr.o		\
		bnxe_rx.o		\
		bnxe_timer.o		\
		bnxe_tx.o		\
		bnxe_workq.o		\
		bnxe_clc.o		\
		bnxe_illumos.o		\
		ecore_sp_verbs.o	\
		bnxe_context.o		\
		57710_init_values.o	\
		57711_init_values.o	\
		57712_init_values.o	\
		bnxe_fw_funcs.o		\
		bnxe_hw_debug.o		\
		lm_l4fp.o		\
		lm_l4rx.o		\
		lm_l4sp.o		\
		lm_l4tx.o		\
		lm_l5.o			\
		lm_l5sp.o		\
		lm_dcbx.o		\
		lm_devinfo.o		\
		lm_dmae.o		\
		lm_er.o			\
		lm_hw_access.o		\
		lm_hw_attn.o		\
		lm_hw_init_reset.o	\
		lm_main.o		\
		lm_mcp.o		\
		lm_niv.o		\
		lm_nvram.o		\
		lm_phy.o		\
		lm_power.o		\
		lm_recv.o		\
		lm_resc.o		\
		lm_sb.o			\
		lm_send.o		\
		lm_sp.o			\
		lm_dcbx_mp.o		\
		lm_sp_req_mgr.o		\
		lm_stats.o		\
		lm_util.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
SRCDIR		= $(UTSBASE)/common/io/bnxe
CONF_SRCDIR	= $(SRCDIR)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Driver-specific flags
#	XXX inline bits were originally set to inline
#
CPPFLAGS	+= -DLM_RXPKT_NON_CONTIGUOUS \
		   -DELINK_ENHANCEMENTS \
		   -DELINK_57711E_SUPPORT \
		   -DELINK_DEBUG \
		   -D__inline= \
		   -D_inline= \
		   -D__BASENAME__=\"bnxe\" \
		   -D__SunOS \
		   -D__S11 \
		   -DILLUMOS \
		   -DLITTLE_ENDIAN \
		   -DLITTLE_ENDIAN_HOST \
		   -D__LITTLE_ENDIAN \
		   -I$(SRCDIR)/577xx/include \
		   -I$(SRCDIR)/577xx/drivers/common/ecore \
		   -I$(SRCDIR)/577xx/drivers/common/include \
		   -I$(SRCDIR)/577xx/drivers/common/include/l4 \
		   -I$(SRCDIR)/577xx/drivers/common/include/l5 \
		   -I$(SRCDIR)/577xx/drivers/common/lm/device \
		   -I$(SRCDIR)/577xx/drivers/common/lm/fw \
		   -I$(SRCDIR)/577xx/drivers/common/lm/include \
		   -I$(SRCDIR)/577xx/drivers/common/lm/l4 \
		   -I$(SRCDIR)/577xx/drivers/common/lm/l4/include \
		   -I$(SRCDIR)/577xx/drivers/common/lm/l5 \
		   -I$(SRCDIR)/577xx/drivers/common/lm/l5/include \
		   -I$(SRCDIR)/577xx/hsi/hw/include \
		   -I$(SRCDIR)/577xx/hsi/mcp \
		   -I$(SRCDIR)

LDFLAGS		+= -Ndrv/ip -Nmisc/mac
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-value
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-but-set-variable

# a whole mess
SMATCH=off


#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ


$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnxe/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnxe/577xx/common/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnxe/577xx/drivers/common/ecore/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnxe/577xx/drivers/common/lm/device/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnxe/577xx/drivers/common/lm/fw/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnxe/577xx/drivers/common/lm/l4/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnxe/577xx/drivers/common/lm/l5/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/bnxe/577xx/drivers/common/lm/device/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
