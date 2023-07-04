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

#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2015 Igor Kozhukhov <ikozhukhov@gmail.com>
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#


#
#	Define the module and object file sets.
#
MODULE		= gfx_private

# XXXMK: should be sorted, but wsdiff
OBJS		=		\
		gfx_private.o	\
		gfxp_pci.o	\
		gfxp_segmap.o	\
		gfxp_devmap.o	\
		gfxp_vgatext.o	\
		gfxp_vm.o	\
		vgasubr.o	\
		gfxp_fb.o	\
		gfxp_bitmap.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

#
#	dependency
#
LDFLAGS	+=      -Nmisc/pci_autoconfig

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
# For now, disable these checks; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

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

# NB: This really is i86pc, not $(UTSMACH).  We build this on i86xpv as well
$(OBJS_DIR)/%.o:		$(UTSBASE)/i86pc/io/gfx_private/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/vga/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
