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
# Copyright 2019 Joyent, Inc.
#


MODULE		= qede

# illumos-specific
OBJS		=		\
		qede_cfg.o	\
		qede_dbg.o	\
		qede_fp.o	\
		qede_gld.o	\
		qede_kstat.o	\
		qede_main.o	\
		qede_misc.o	\
		qede_osal.o

# OS-independent
# XXXMK: Should be sorted, but wsdiff
OBJS		+=			\
		ecore_hw.o		\
		ecore_cxt.o		\
		ecore_selftest.o	\
		ecore_init_ops.o	\
		ecore_init_fw_funcs.o	\
		ecore_sp_commands.o	\
		ecore_dcbx.o		\
		ecore_dbg_fw_funcs.o	\
		ecore_mcp.o		\
		ecore_spq.o		\
		ecore_phy.o		\
		ecore_dev.o		\
		ecore_l2.o		\
		ecore_int.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/qede

include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
# Common definitions that are used by QLogic.
#
CPPFLAGS	+= -D__inline=inline
CPPFLAGS	+= -D_inline=inline
CPPFLAGS	+= -DILLUMOS
CPPFLAGS	+= -DECORE_CONFIG_DIRECT_HWFN
CPPFLAGS	+= -DCONFIG_ECORE_L2

#
# Includes that are needed
#
CPPFLAGS	+= -I$(UTSBASE)/common/io/qede
CPPFLAGS	+= -I$(UTSBASE)/common/io/qede/579xx/drivers/ecore
CPPFLAGS	+= -I$(UTSBASE)/common/io/qede/579xx/drivers/ecore/hsi_repository
CPPFLAGS	+= -I$(UTSBASE)/common/io/qede/579xx/hsi/
CPPFLAGS	+= -I$(UTSBASE)/common/io/qede/579xx/hsi/hw
CPPFLAGS	+= -I$(UTSBASE)/common/io/qede/579xx/hsi/mcp

#
# Temporarily gag these warnings for the moment. We'll work with
# upstream to get them clean.
#
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-function
CCWARNINLINE	=

# 3rd party module
SMOFF += all_func_returns,indenting,no_if_block,deref_check,testing_index_after_use

# real bug in qede_multicast()
$(OBJS_DIR)/qede_gld.o := SMOFF += assign_vs_compare

#
# Unfortunately the default use of -fstack-protector-strong breaks the
# qede module. For the time being limit its use of stack-protector to
# the basic form (-fstack-protector).
#
STACKPROTECT=basic

ALL_TARGET	= $(BINARY) $(CONFMOD)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

LDFLAGS		+= -N misc/mac

.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/qede/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/qede/579xx/drivers/ecore/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
