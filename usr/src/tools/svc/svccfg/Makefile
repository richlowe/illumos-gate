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
# Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright 2020 Joyent, Inc.
#

PROG =	svccfg

SRCS  =				\
	svccfg_engine.c		\
	svccfg_help.c		\
	svccfg_internal.c	\
	svccfg_libscf.c		\
	svccfg_main.c		\
	svccfg_tmpl.c		\
	svccfg_xml.c

OBJS =				\
	$(SRCS:%.c=%.o)		\
	manifest_find.o		\
	manifest_hash.o		\
	notify_params.o		\
	svccfg_grammar.o	\
	svccfg_lex.o

include $(SRC)/cmd/Makefile.cmd
include $(SRC)/cmd/Makefile.ctf
include	$(SRC)/tools/Makefile.tools

CPPFLAGS +=	-I. -I$(SRC)/cmd/svc/svccfg -I$(SRC)/cmd/svc/common	\
		-I$(NATIVE_ADJUNCT)/include/libxml2			\
		-DNATIVE_BUILD						\
		-I$(LIBSCF)/inc						\
		-I$(LIBTECLA)						\
		-I$(LIBUUTIL)/common

CERRWARN +=	-_gcc=-Wno-unused-label
CERRWARN +=	-_gcc=-Wno-switch
CERRWARN +=	-_gcc=-Wno-unused-variable
CERRWARN +=	-_gcc=-Wno-parentheses
CERRWARN +=	$(CNOWARN_UNINIT)

# not linted
SMATCH=off

LFLAGS = -t
YFLAGS = -d

CLOBBERFILES += svccfg_lex.c svccfg_grammar.c svccfg_grammar.h

LIBTECLA	= $(SRC)/lib/libtecla		# just for the header
LIBUUTIL	= $(SRC)/lib/libuutil

SVCCFG_EXTRA_LIBS += -lxml2 -lscf -ll -luutil -lumem -lmd \
	-lnvpair
ADJUNCT_LIBS += libxml2.so libl.so libumem.so libmd.so libnvpair.so libc.so

CPPFLAGS +=

LDLIBS += \
	-L$(SRC)/tools/svc/libscf -R '$$ORIGIN/../../lib/$(NATIVE_MACH)' \
	-L$(NATIVE_ADJUNCT)/lib -R$(NATIVE_ADJUNCT)/lib -L$(ROOTONBLDLIBMACH) \
	$(SVCCFG_EXTRA_LIBS)

.KEEP_STATE:
.PARALLEL: $(OBJS)

all debug: $(PROG)

$(PROG): $(OBJS) $(MAPFILES)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTONBLDMACHPROG)

svccfg_lex.c: $(SRC)/cmd/svc/svccfg/svccfg.l svccfg_grammar.h
	$(LEX) $(LFLAGS) $(SRC)/cmd/svc/svccfg/svccfg.l > $@

svccfg_help.o: svccfg_grammar.h

svccfg_grammar.h svccfg_grammar.c: $(SRC)/cmd/svc/svccfg/svccfg.y
	$(YACC) $(YFLAGS) $(SRC)/cmd/svc/svccfg/svccfg.y
	@$(MV) y.tab.h svccfg_grammar.h
	@$(MV) y.tab.c svccfg_grammar.c

clean:
	$(RM) $(OBJS)

%.o: $(SRC)/cmd/svc/svccfg/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

%.o: $(SRC)/cmd/svc/common/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POST_PROCESS_O)

include ../../Makefile.targ

FRC:
