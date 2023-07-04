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
# Copyright 2016, Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= cpudrv
MOD_SRCDIR	= $(UTSBASE)/common/io/cpudrv
OBJS		= cpudrv.o cpudrv_mach.o
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-function

DEPENDS_ON	= misc/acpica

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/io/cpudrv/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
