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
# Copyright 2014 Garrett D'Amore <garrett@damore.org>
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= scsa2usb
MOD_SRCDIR	= $(UTSBASE)/common/io/usb/scsa2usb
OBJS		= scsa2usb.o usb_ms_bulkonly.o usb_ms_cbi.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= misc/usba misc/scsi

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
$(OBJS_DIR)/scsa2usb.o := SMOFF += deref_check

include $(UTSBASE)/Makefile.kmod.targ
