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
# Copyright 2018, Nexenta Systems, Inc. All Rights Reserved
# Use is subject to license terms.
#

MODULE		= smartpqi
MOD_SRCDIR	= $(UTSBASE)/common/io/scsi/adapters/smartpqi/

# XXXMK: Should be sorted, but wsdiff
OBJS 		=		\
		smartpqi_main.o \
		smartpqi_intr.o \
		smartpqi_hba.o	\
		smartpqi_util.o \
		smartpqi_hw.o	\
		smartpqi_init.o \
		smartpqi_sis.o

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	 = misc/scsi drv/scsi_vhci misc/sata

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(UTSBASE)/common/io/scsi/adapters/smartpqi

include $(UTSBASE)/Makefile.kmod.targ
