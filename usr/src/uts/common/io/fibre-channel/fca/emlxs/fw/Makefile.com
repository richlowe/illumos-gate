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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

MODULE		= emlxs_fw
MOD_SRCDIR	= $(UTSBASE)/common/io/fibre-channel/fca/emlxs/fw
ROOTMODULE	= $(ROOT_EMLXS_FW_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

intel_EMLXS_FLAGS	= -DEMLXS_I386
aarch64_EMLXS_FLAGS	= -DEMLXS_AARCH64
EMLXS_FLAGS		+= $($(UTSMACH)_EMLXS_FLAGS)
EMLXS_FLAGS             += -DS11
EMLXS_FLAGS             += -DVERSION=\"11\"
EMLXS_FLAGS             += -DMACH=\"$(MACH)\"
EMLXS_CFLAGS            = $(EMLXS_FLAGS)
EMLXS_LFLAGS            = $(EMLXS_FLAGS)
CFLAGS	                += $(EMLXS_CFLAGS) -DEMLXS_ARCH=\"$(CLASS)\"

INC_PATH	+= -I$(UTSBASE)/common/sys/fibre-channel/fca/emlxs

DEPENDS_ON	= misc/fctl

include $(UTSBASE)/Makefile.kmod.targ
