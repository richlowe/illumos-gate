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

SRCS  =			\
	svccfg_main.c		\
	svccfg_engine.c		\
	svccfg_internal.c	\
	svccfg_libscf.c		\
	svccfg_tmpl.c		\
	svccfg_xml.c		\
	svccfg_help.c

OBJS =				\
	$(SRCS:%.c=%.o)		\
	svccfg_grammar.o	\
	svccfg_lex.o		\
	manifest_find.o		\
	manifest_hash.o		\
	notify_params.o		\

include $(SRC)/cmd/Makefile.cmd

# These are because of bugs in lex(1)/yacc(1) generated code
CERRWARN +=	-_gcc=-Wno-unused-label
CERRWARN +=	-_gcc=-Wno-unused-variable

CERRWARN +=	-_gcc=-Wno-switch
CERRWARN +=	$(CNOWARN_UNINIT)

# not linted
SMATCH=off

LFLAGS = -t
YFLAGS = -d

CLOBBERFILES += svccfg_lex.c svccfg_grammar.c svccfg_grammar.h
