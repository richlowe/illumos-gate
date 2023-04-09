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
# Copyright 2015 RackTop Systems.
# Copyright 2020 Joyent, Inc.
#

PROG = svc.configd
OBJS =			\
	backend.o	\
	configd.o	\
	client.o	\
	file_object.o	\
	maindoor.o	\
	object.o	\
	rc_node.o	\
	snapshot.o

include $(SRC)/cmd/Makefile.cmd
include $(SRC)/cmd/Makefile.ctf
include $(SRC)/tools/Makefile.tools

LIBUUTIL	= $(SRC)/lib/libuutil
LIBSCF		= $(SRC)/lib/libscf
LIBSQLITE	= $(SRC)/lib/libsqlite

CPPFLAGS += -I$(SRC)/cmd/svc/configd \
	-I$(SRC)/cmd/svc/common -I$(SRC)/common/svc \
	-I$(ROOT)/usr/include/sqlite-sys -D_REENTRANT \
	-I$(LIBUUTIL)/common -I$(LIBSCF)/inc -I$(LIBSQLITE) \
	-I$(SRC)/tools/svc/libsqlite \

CPPFLAGS += -DNATIVE_BUILD
CFLAGS	+= $(CCVERBOSE)
CERRWARN += -_gcc=-Wno-parentheses
CERRWARN += -_gcc=-Wno-unused-label
CERRWARN += -_gcc=-Wno-unused-variable
CERRWARN += -_gcc=-Wno-unused-function
CERRWARN += $(CNOWARN_UNINIT)

# strange false positive
SMOFF += free

LDFLAGS += $(BDIRECT)
LDLIBS	+= -L$(NATIVE_ADJUNCT)/usr/lib -R$(NATIVE_ADJUNCT)/usr/lib \
	-lumem -L$(ROOTONBLDLIBMACH) \
	-R '$$ORIGIN/../../lib/$(NATIVE_MACH)' -lsqlite-sys -luutil

ADJUNCT_LIBS += libumem.so libc.so

DIRMODE = 0755
FILEMODE = 0555

.KEEP_STATE:
.PARALLEL: $(OBJS)

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

%.o: $(SRC)/cmd/svc/configd/%.c
	$(COMPILE.c) $<
	$(POST_PROCESS_O)

install: all $(ROOTONBLDMACHPROG)

clean:

clobber: clean

include $(SRC)/cmd/Makefile.targ
