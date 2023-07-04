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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= mpt_sas
MOD_SRCDIR	= $(UTSBASE)/common/io/scsi/adapters/mpt_sas

OBJS		=		\
		mptsas.o	\
		mptsas_impl.o	\
		mptsas_init.o	\
		mptsas_raid.o	\
		mptsas_smhba.o

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/scsi drv/scsi_vhci misc/sata

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
$(OBJS_DIR)/mptsas_raid.o := SMOFF += index_overflow
$(OBJS_DIR)/mptsas.o := SMOFF += deref_check

include $(UTSBASE)/Makefile.kmod.targ
