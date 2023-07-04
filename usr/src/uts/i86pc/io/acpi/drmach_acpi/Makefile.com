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
# Copyright (c) 2010, Intel Corporation.
# All rights reserved.
#
# Copyright 2016 Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= drmach_acpi
MOD_SRCDIR	= $(UTSBASE)/i86pc/io/acpi/drmach_acpi
OBJS		= drmach_acpi.o dr_util.o drmach_err.o
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

$(OBJS_DIR)/drmach_acpi.o :=	CERRWARN	+= -_gcc=-Wno-unused-function

DEPENDS_ON = misc/acpica misc/acpidev

CLEANFILES +=	$(DRMACH_GENERR)
CLEANFILES +=	$(DRMACH_IO)/drmach_err.c

include $(UTSBASE)/Makefile.kmod.targ

SBD_IOCTL	= $(UTSBASE)/i86pc/sys/sbd_ioctl.h
DR_IO		= $(UTSBASE)/i86pc/io/dr
DRMACH_IO	= $(UTSBASE)/i86pc/io/acpi/drmach_acpi
DRMACH_GENERR	= $(DRMACH_IO)/sbdgenerr

$(DRMACH_GENERR):	$(DR_IO)/sbdgenerr.pl
	$(RM) $@
	$(CAT) $(DR_IO)/sbdgenerr.pl > $@
	$(CHMOD) +x $@

$(DRMACH_IO)/drmach_err.c:	$(DRMACH_GENERR) $(SBD_IOCTL)
	$(RM) $@
	$(DRMACH_GENERR) EX86 < $(SBD_IOCTL) > $(DRMACH_IO)/drmach_err.c

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/io/dr/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
