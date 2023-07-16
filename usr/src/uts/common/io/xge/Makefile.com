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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2019 Joyent, Inc.
#

MODULE		= xge
MOD_SRCDIR	= $(UTSBASE)/common/io/xge

# HAL
OBJS	=			\
	xge-queue.o		\
	xgehal-channel.o	\
	xgehal-config.o		\
	xgehal-device.o		\
	xgehal-driver.o		\
	xgehal-fifo.o		\
	xgehal-mgmt.o		\
	xgehal-mgmtaux.o	\
	xgehal-mm.o		\
	xgehal-ring.o		\
	xgehal-stats.o

OBJS +=		\
	xge.o	\
	xgell.o

include $(UTSBASE)/Makefile.kmod

#
#	GENERAL PURPOUSE HAL FLAGS: Tuning HAL for Solaris specific modes
#
HAL_CFLAGS	 = -DXGE_HAL_USE_MGMT_AUX

#
#	TRACE SECTION: Possible values for MODULE, TRACE and ERR masks:
#
# XGE_COMPONENT_HAL_CONFIG		0x1
# XGE_COMPONENT_HAL_FIFO		0x2
# XGE_COMPONENT_HAL_RING		0x4
# XGE_COMPONENT_HAL_CHANNEL		0x8
# XGE_COMPONENT_HAL_DEVICE		0x10
# XGE_COMPONENT_HAL_MM			0x20
# XGE_COMPONENT_HAL_QUEUE		0x40
# XGE_COMPONENT_HAL_STATS		0x100
# XGE_COMPONENT_OSDEP			0x1000
# XGE_COMPONENT_LL			0x2000
# XGE_COMPONENT_TOE			0x4000
# XGE_COMPONENT_RDMA			0x8000
# XGE_COMPONENT_ALL			0xffffffff
#TRACE_CFLAGS = -DXGE_DEBUG_MODULE_MASK=0xffffffff \
#		-DXGE_DEBUG_TRACE_MASK=0xffffffff \
#		-DXGE_DEBUG_ERR_MASK=0xffffffff
TRACE_CFLAGS	= -DXGE_DEBUG_MODULE_MASK=0x00003010 \
		-DXGE_DEBUG_TRACE_MASK=0x00000000  \
		-DXGE_DEBUG_ERR_MASK=0x00003010

XGE_CFLAGS	= $(HAL_CFLAGS) $(TRACE_CFLAGS) \
		-DSOLARIS

INC_PATH	+= -I$(UTSBASE)/common/io/xge/hal/include \
	-I$(UTSBASE)/common/io/xge/hal/xgehal \
	-I$(UTSBASE)/common/io/xge/drv

CFLAGS		+= $(XGE_CFLAGS) -xO4
CFLAGS64	+= $(XGE_CFLAGS) -xO4

DEPENDS_ON	= misc/mac drv/ip

CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-empty-body
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
SMOFF += indenting,all_func_returns,no_if_block,allocating_enough_data

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/xge/drv/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/xge/hal/xgehal/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
