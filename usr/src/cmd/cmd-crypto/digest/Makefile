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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

PROG = digest

ROOTLINK64= $(ROOTBIN64)/mac
ROOTLINK= $(ROOTBIN)/mac
ROOTDIGLINK = $(PROG:%=$(ROOTBIN64)/%)

DCFILE= $(PROG).dc

OBJS = digest.o

SRCS = $(OBJS:%.o=../%.c)

include ../../Makefile.cmd
include ../../Makefile.cmd.64

CFLAGS += $(CCVERBOSE)
CPPFLAGS +=-D_FILE_OFFSET_BITS=64

CERRWARN += $(CNOWARN_UNINIT)

LDLIBS += -lkmf -lpkcs11 -lcryptoutil

.KEEP_STATE:

all:	$(PROG)

install: all $(ROOTBIN) $(ROOTLINK) $(ROOTLINK64) $(ROOTDIGLINK)

$(ROOTDIGLINK):
	$(RM) $@; $(SYMLINK) ../$(@F) $@

$(ROOTLINK): $(ROOTPROG)
	$(RM) $@; $(LN) $(ROOTPROG) $@

%.o:	%.c
	$(COMPILE.c) $<

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS) $(DYNFLAGS)
	$(POST_PROCESS)

$(ROOTLINK64): $(ROOTPROG64)
	$(RM) $@
	$(LN) $(ROOTPROG64) $@

clean:
	$(RM) $(PROG) $(OBJS)

$(DCFILE):
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) -t $(PROG).c
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

include ../../Makefile.targ
