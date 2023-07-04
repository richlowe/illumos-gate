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
# Copyright 2017 Hayashi Naoyuki
#

# XXXARM: This builds a different, qemu-specific mmio based virtio.  We need
# to arrange to set this all up in the virtio/Makefile.com or have
# virtio/Makefile.pci and virtio/Makefile.mmio or something like that.

UTSBASE	= ../../..

#
#	Define the module and object file sets.
#
MODULE		= virtio
OBJS		= virtio_main.o virtio_dma.o virtio_mmio.o
OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_VIRT_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/armv8/virt/Makefile.virt

#
#       Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

INC_PATH +=		-I$(UTSBASE)/common/io/virtio

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(ALL_DEPS)

def:		$(DEF_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)


#
#	Include common targets.
#
include $(UTSBASE)/armv8/virt/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/virtio/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

