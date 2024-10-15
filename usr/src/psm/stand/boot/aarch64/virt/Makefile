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
# Copyright 2020 Hayashi Naoyuki
#

TOPDIR =	../../../../..
OBJS += virtnet.o virtblk.o
BOARD=	virt

include ../Makefile.com
ROOT_PSM_DIR=$(ROOT_PLAT_DIR)/QEMU,virt

.KEEP_STATE:

all: $(NFSBOOT_BIN)
$(NFS_SRT0) $(NFS_OBJS):  $(ASSYM_H)

install: all $(ROOT_PSM_NFSBOOT)

include $(BOOTSRCDIR)/Makefile.rules
include ../Makefile.targ

$(ROOT_PSM_NFSBOOT): $(NFSBOOT_BIN) $(ROOT_PSM_DIR)
	$(INS.file) $(NFSBOOT_BIN)
