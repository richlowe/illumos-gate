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

#
# Copyright 2018 Joyent, Inc.
#

LIBRARY =	libvmm.a
VERS =		.1
OBJECTS =	libvmm.o list.o

SRCDIR =	.

include ../../Makefile.lib
include ../../Makefile.rootfs

LIBS		= $(DYNLIB)

# The FreeBSD compat and contrib headers need to be first in the search
# path, hence we can't just append them to CPPFLAGS. So we assign CPPFLAGS
# directly and pull in CPPFLAGS.master at the appropriate place.
CPPFLAGS =	-I$(COMPAT)/bhyve -I$(CONTRIB)/bhyve \
		-I$(COMPAT)/bhyve/amd64 -I$(CONTRIB)/bhyve/amd64 \
		$(CPPFLAGS.master) -I$(SRC)/uts/i86pc

LDLIBS +=	-lc -lvmmapi

.KEEP_STATE:

all: $(LIBS)

pics/%.o: $(SRC)/common/list/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# include library targets
include ../../Makefile.targ
