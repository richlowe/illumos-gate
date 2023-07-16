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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= pmcs
MOD_SRCDIR	= $(UTSBASE)/common/io/scsi/adapters/pmcs

OBJS		=		\
		pmcs_attach.o	\
		pmcs_ds.o	\
		pmcs_fwlog.o	\
		pmcs_intr.o	\
		pmcs_nvram.o	\
		pmcs_sata.o	\
		pmcs_scsa.o	\
		pmcs_smhba.o	\
		pmcs_subr.o

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/scsi

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

include $(MOD_SRCDIR)/fw/pmcs8001fw.version

PMCS_DRV_FLGS	= -DMODNAME=\"$(MODULE)\"
CPPFLAGS	+= $(PMCS_DRV_FLGS) \
	-DPMCS_FIRMWARE_VERSION=$(PMCS_FW_VERSION) \
	-DPMCS_FIRMWARE_VERSION_STRING=\"$(PMCS_FW_VERSION_STRING)\"

CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-unused-value
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-parentheses

# needs work
SMATCH=off

include $(UTSBASE)/Makefile.kmod.targ
