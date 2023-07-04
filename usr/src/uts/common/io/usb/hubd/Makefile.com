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

MODULE		= hubd
MOD_SRCDIR	= $(UTSBASE)/common/io/usb/hubd

CONFIGFILES	= config_map.conf
ROOTETCUSB	= $(ROOT)/etc/usb

ROOTCONFIGFILES	= $(CONFIGFILES:%=$(ROOTETCUSB)/%)

$(ROOTCONFIGFILES):=	FILEMODE = $(CFILEMODE)

include $(UTSBASE)/Makefile.kmod

INSTALL_DEPS	+= $(ROOTCONFIGFILES)

DEPENDS_ON	= misc/usba

$(ROOTETCUSB)/%: $(ROOTETCUSB) $(MOD_SRCDIR)/%
	$(INS.file)

include $(UTSBASE)/Makefile.kmod.targ
