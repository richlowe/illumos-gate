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
# Copyright (c) 2019, Joyent, Inc.

#
# The build process for libsaveargs is sightly different from that used by other
# libraries, because libsaveargs must be built in two flavors - as a standalone
# for use by kmdb and as a normal library.  We use $(CURTYPE) to indicate the
# current flavor being built.
#

include $(SRC)/Makefile.master

LIBRARY=	libsaveargs.a
STANDLIBRARY=	libstandsaveargs.so
VERS=		.1

# By default, we build the shared library.  Construction of the standalone
# is specifically requested by architecture-specific Makefiles.
TYPES=		library
CURTYPE=	library

COMDIR=		$(SRC)/lib/libsaveargs/common

OBJECTS_common_amd64	= saveargs.o
OBJECTS_common_aarch64	= saveargs.o bitext.o

OBJECTS= $(OBJECTS_common_$(MACH64)) $(OBJECTS_common_common)
$(BUILD32)OBJECTS +=  $(OBJECTS_common_$(MACH))

include $(SRC)/lib/Makefile.lib

CSTD = $(CSTD_GNU99)

#
# Used to verify that the standalone doesn't have any unexpected external
# dependencies.
#
LINKTEST_OBJ = objs/linktest_stand.o

CLOBBERFILES_standalone = $(LINKTEST_OBJ)
CLOBBERFILES += $(CLOBBERFILES_$(CURTYPE))

#
# The standalone environment currently does not support the stack
# protector.
#
$(STANDLIBRARY) := STACKPROTECT = none

LIBS_standalone	= $(STANDLIBRARY)
LIBS_library = $(DYNLIB)
LIBS = $(LIBS_$(CURTYPE))

MAPFILES =	$(COMDIR)/mapfile-vers

LDLIBS_i386 =	-ldisasm
LDLIBS +=	-lc $(LDLIBS_$(MACH))

LDFLAGS_standalone = $(ZNOVERSION) $(BREDUCE) -dy -r
LDFLAGS = $(LDFLAGS_$(CURTYPE))

ASFLAGS_standalone = -DDIS_STANDALONE
ASFLAGS_library =
ASFLAGS += $(ASFLAGS_$(CURTYPE)) -D_ASM


# We want the thread-specific errno in the library, but we don't want it in
# the standalone.  $(DTS_ERRNO) is designed to add -D_TS_ERRNO to $(CPPFLAGS),
# in order to enable this feature.  Conveniently, -D_REENTRANT does the same
# thing.  As such, we null out $(DTS_ERRNO) to ensure that the standalone
# doesn't get it.
DTS_ERRNO=

CPPFLAGS_standalone = -DDIS_STANDALONE
CPPFLAGS_library = -D_REENTRANT
CPPFLAGS +=	-I$(COMDIR) $(CPPFLAGS_$(CURTYPE))

CFLAGS_standalone = $(STAND_FLAGS_32)
CFLAGS_common =
CFLAGS += $(CFLAGS_$(CURTYPE)) $(CFLAGS_common)

CFLAGS64_standalone = $(STAND_FLAGS_64)
CFLAGS64 += $(CCVERBOSE) $(CFLAGS64_$(CURTYPE)) $(CFLAGS64_common)

DYNFLAGS +=     $(ZINTERPOSE)

.KEEP_STATE:
