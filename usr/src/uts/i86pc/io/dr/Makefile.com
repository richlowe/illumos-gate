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
# Copyright (c) 2018, Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= dr
MOD_SRCDIR	= $(UTSBASE)/i86pc/io/dr

OBJS		=		\
		dr.o		\
		dr_cpu.o	\
		dr_err.o	\
		dr_io.o		\
		dr_mem_acpi.o	\
		dr_quiesce.o	\
		dr_util.o

ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

CERRWARN += -_gcc=-Wno-parentheses
CERRWARN += $(CNOWARN_UNINIT)
CERRWARN += -_gcc=-Wno-empty-body

# needs work
SMOFF += index_overflow

DEPENDS_ON = misc/drmach_acpi

CLEANFILES +=	$(DR_GENERR)
CLEANFILES +=	$(DR_IO)/dr_err.c

include $(UTSBASE)/Makefile.kmod.targ

SBD_IOCTL	= $(UTSBASE)/i86pc/sys/sbd_ioctl.h
DR_IO		= $(UTSBASE)/i86pc/io/dr
DR_GENERR	= $(DR_IO)/sbdgenerr

$(DR_GENERR):			$(DR_IO)/sbdgenerr.pl
	$(RM) $@
	$(CAT) $(DR_IO)/sbdgenerr.pl > $@
	$(CHMOD) +x $@

$(DR_IO)/dr_err.c:		$(DR_GENERR) $(SBD_IOCTL)
	$(RM) $@
	$(DR_GENERR) ESBD < $(SBD_IOCTL) > $(DR_IO)/dr_err.c
