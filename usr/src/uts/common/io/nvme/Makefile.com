#
# CDDL HEADER START
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#
# CDDL HEADER END
#
#
# Copyright 2015 Nexenta Systems, Inc. All rights reserved.
#

MODULE		= nvme

OBJS	=		\
	nvme.o		\
	nvme_feature.o	\
	nvme_field.o	\
	nvme_firmware.o \
	nvme_format.o	\
	nvme_identify.o \
	nvme_lock.o	\
	nvme_log.o	\
	nvme_validate.o \
	nvme_version.o	\
	nvme_vuc.o

MOD_SRCDIR	= $(UTSBASE)/common/io/nvme

include $(UTSBASE)/Makefile.kmod

#
# Bits required for common source.
#
CPPFLAGS	+= -I$(SRC)/common/nvme

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	= drv/blkdev misc/sata

$(OBJS_DIR)/%.o:		$(COMMONBASE)/nvme/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

include $(UTSBASE)/Makefile.kmod.targ
