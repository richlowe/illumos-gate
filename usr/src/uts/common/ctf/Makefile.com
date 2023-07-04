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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2018 Joyent, Inc.
#


MODULE		= ctf
MOD_SRCDIR	= $(UTSBASE)/common/ctf

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		ctf_create.o \
		ctf_decl.o \
		ctf_error.o \
		ctf_hash.o \
		ctf_labels.o \
		ctf_lookup.o \
		ctf_open.o \
		ctf_types.o \
		ctf_util.o \
		ctf_subr.o \
		ctf_mod.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

CPPFLAGS	+= -I$(SRC)/common/ctf -DCTF_OLD_VERSIONS
LDFLAGS		+= $(BREDUCE) -M$(UTSBASE)/common/ctf/mapfile

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= $(CNOWARN_UNINIT)

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(COMMONBASE)/ctf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
