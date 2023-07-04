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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
#

MODULE		= ata
MOD_SRCDIR	= $(UTSBASE)/intel/io/dktp/ata

# GHD
# XXXMK: should be sorted but wsdiff
OBJS		=		\
		ghd.o		\
		ghd_debug.o	\
		ghd_dma.o	\
		ghd_queue.o	\
		ghd_scsa.o	\
		ghd_scsi.o	\
		ghd_timer.o	\
		ghd_waitq.o	\
		ghd_gcmd.o

# ATA
OBJS		+=		\
		ata_blacklist.o \
		ata_common.o	\
		ata_disk.o	\
		ata_dma.o	\
		atapi.o		\
		atapi_fsm.o	\
		ata_debug.o	\
		sil3xxx.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(UTSBASE)/intel/io/dktp/ghd

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)

DEPENDS_ON	= misc/scsi

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/dktp/ghd/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
