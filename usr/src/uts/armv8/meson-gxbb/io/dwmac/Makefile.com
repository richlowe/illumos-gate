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


#
#	Define the module and object file sets.
#
MODULE		= dwmac
OBJS		= dwmac.o
OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_MESON_GXBB_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/armv8/meson-gxbb/Makefile.meson-gxbb

#
#       Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides
#
ALL_BUILDS	= $(ALL_BUILDSONLY64)
DEF_BUILDS	= $(DEF_BUILDSONLY64)

INC_PATH	+= -I$(SRC)/uts/armv8/meson-gxbb/

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CFLAGS += $(CCVERBOSE)

CERRWARN += -_gcc=-Wno-unused-variable

#	Dependency
LDFLAGS += -Nmisc/mac -Nmisc/mii

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
include $(UTSBASE)/armv8/meson-gxbb/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/armv8/meson-gxbb/io/dwmac/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
