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

# Copyright 2015, Richard Lowe.


include $(SRC)/cmd/Makefile.cmd
include $(SRC)/test/Makefile.com

PROG =	secflags_aslr		\
	secflags_core		\
	secflags_dts		\
	secflags_elfdump	\
	secflags_forbidnullmap	\
	secflags_limits		\
	secflags_noexecstack	\
	secflags_proc		\
	secflags_psecflags	\
	secflags_zonecfg

PROG += addrs-32 addrs-64 stacky

ROOTOPTPKG = $(ROOT)/opt/os-tests
TESTDIR = $(ROOTOPTPKG)/tests/secflags

CMDS = $(PROG:%=$(TESTDIR)/%)
$(CMDS) := FILEMODE = 0555

addrs-32.o: addrs.c
	$(COMPILE.c) -m32 addrs.c -o $@

addrs-64.o: addrs.c
	$(COMPILE.c) -m64 addrs.c -o $@

addrs-32: addrs-32.o
	$(LINK.c) -m32 addrs-32.o -o $@ $(LDLIBS)
	$(POST_PROCESS)

addrs-64: addrs-64.o
	$(LINK.c) -m64 addrs-64.o -o $@ $(LDLIBS)
	$(POST_PROCESS)

stacky := MAPFILE.NES=			# Will foil the test, clearly
stacky: stacky.o
	$(LINK.c) -m32 stacky.o -o $@ $(LDLIBS)
	$(POST_PROCESS)

all: $(PROG)

install: all $(CMDS)

lint: 

clobber: clean
	-$(RM) $(PROG)

clean:

$(CMDS): $(TESTDIR) $(PROG)

$(TESTDIR):
	$(INS.dir)

$(TESTDIR)/%: %
	$(INS.file)
