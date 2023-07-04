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
# Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2013, 2016 by Delphix. All rights reserved.
# Copyright 2020 Joyent, Inc.
# Copyright 2023 OmniOS Community Edition (OmniOSce) Association.
#

include $(SRC)/Makefile.master

LIBRARY= libzpool.a
VERS= .1

# Common definitions of object sets from the kernel module
include $(SRC)/uts/common/fs/zfs/Makefile.zfs

KERNEL_OBJS = kernel.o util.o
# XXXARM: No cross DTrace
$(NOT_AARCH64_BLD)DTRACE_OBJS = zfs.o
$(AARCH64_BLD)DTRACE_OBJS=

OBJECTS=$(LUA_OBJS) $(ZFS_COMMON_OBJS) $(ZFS_SHARED_OBJS) $(KERNEL_OBJS)

# include library definitions
include ../../Makefile.lib

SRCDIR=		../common

# There should be a mapfile here
MAPFILES =

LIBS +=		$(DYNLIB)

INCS += -I../common
INCS += -I../../../uts/common/fs/zfs
INCS += -I../../../uts/common/fs/zfs/lua
INCS += -I../../../common/zfs
INCS += -I../../../common
INCS += -I../../libzutil/common


CLEANFILES += ../common/zfs.h
CLEANFILES += $(EXTPICS)

$(LIBS): ../common/zfs.h

CSTD=	$(CSTD_GNU99)

CFLAGS +=	$(CCGDEBUG) $(CCVERBOSE)
CFLAGS64 +=	$(CCGDEBUG) $(CCVERBOSE)
LDLIBS +=	-lcmdutils -lumem -lavl -lnvpair -lz -lc -lmd \
		-lfakekernel -lzutil
ADJUNCT_LIBS +=	libz.so
CPPFLAGS.first =	-I$(SRC)/lib/libfakekernel/common
# The in-gate lz4 headers must take precedence over any that may appear in an
# adjunct.
CPPFLAGS.first +=	-I$(SRC)/common/lz4
CPPFLAGS +=	$(INCS)	-DDEBUG -D_FAKE_KERNEL

CERRWARN +=	-_gcc=-Wno-parentheses
CERRWARN +=	-_gcc=-Wno-switch
CERRWARN +=	-_gcc=-Wno-type-limits
CERRWARN +=	-_gcc=-Wno-unused-variable
CERRWARN +=	-_gcc=-Wno-empty-body
CERRWARN +=	-_gcc=-Wno-unused-function
CERRWARN +=	-_gcc=-Wno-unused-label
CERRWARN +=	-_gcc=-Wno-implicit-function-declaration

# not linted
SMATCH=off

.KEEP_STATE:

all: $(LIBS)

include ../../Makefile.targ

EXTPICS= $(DTRACE_OBJS:%=pics/%)

pics/%.o: ../../../uts/common/fs/zfs/%.c ../common/zfs.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics/%.o: ../../../uts/common/fs/zfs/lua/%.c ../common/zfs.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics/%.o: ../../../common/zfs/%.c $(NOT_AARCH64_BLD)../common/zfs.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics/%.o: ../../../common/lz4/%.c $(NOT_AARCH64_BLD)../common/zfs.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics/%.o: ../common/%.d $(PICS)
	$(COMPILE.d) -C -s $< -o $@ $(PICS)
	$(POST_PROCESS_O)

pics/%.o: $(SRC)/uts/$(MACH)/zfs/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# XXXARM: We don't have DTrace, so we have to stub this
../common/%.h: ../common/%.d
	if [[ $(MACH) != "aarch64" ]]; then \
		$(DTRACE) -xnolibs -h -s $< -o $@; \
	else \
		touch $@; \
	fi;
