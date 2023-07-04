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
# Copyright (c) 2016 by Delphix. All rights reserved.
#


#
#	Define the module and object file sets.
#
MODULE		= dev

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		sdev_subr.o	\
		sdev_vfsops.o	\
		sdev_vnops.o	\
		sdev_ptsops.o	\
		sdev_zvolops.o	\
		sdev_comm.o	\
		sdev_profile.o	\
		sdev_ncache.o	\
		sdev_netops.o	\
		sdev_ipnetops.o	\
		sdev_vtops.o	\
		sdev_plugin.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides
#
LDFLAGS		+= -Nfs/devfs -Nmisc/dls
INC_PATH	+= -I$(UTSBASE)/common/fs/zfs
INC_PATH	+= -I$(UTSBASE)/common/io/bpf


CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-unused-function

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/fs/dev/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
