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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2019 Joyent, Inc.
# Copyright 2020 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= procfs
MOD_SRCDIR	= $(UTSBASE)/common/fs/proc

OBJS		=		\
		prcontrol.o	\
		prioctl.o	\
		prsubr.o	\
		prusrio.o	\
		prvfsops.o	\
		prvnops.o

ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
$(OBJS_DIR)/prsubr.o := SMOFF += all_func_returns
$(OBJS_DIR)/prcontrol.o := SMOFF += all_func_returns
$(OBJS_DIR)/prioctl.o := SMOFF += signed

DEPENDS_ON	= fs/namefs

include $(UTSBASE)/Makefile.kmod.targ
