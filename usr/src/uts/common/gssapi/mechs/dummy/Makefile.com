#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
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
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
#

MODULE		= kmech_dummy
MOD_SRCDIR	= $(UTSBASE)/common/gssapi/mechs/dummy
OBJS		= dmech.o
ROOTMODULE	= $(ROOT_KGSS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

CERRWARN	+= -_gcc=-Wno-parentheses

DEPENDS_ON	= misc/kgssapi

INC_PATH +=	-I $(UTSBASE)/common/gssapi/include

# kmech_dummy is not installed, even in the proto area
INSTALL_TARGET = $(BINARY)

include $(UTSBASE)/Makefile.kmod.targ
