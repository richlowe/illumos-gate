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
# Copyright (c) 1992, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright 2017 Hayashi Naoyuki
#

BASEPLAT =	aarch64

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code, ie. profiling support).

P_COMOBJS=	debugdata.o \
		analyze.o	elf.o		external.o	globals.o \
		malloc.o	paths.o		setup.o		util.o \
		dlfcns.o	config_elf.o	locale.o	tsort.o \
		remove.o	move.o		tls.o		cap.o

S_COMOBJS=	debug.o		audit.o		object.o

G_MACHOBJS=	doreloc.o

P_MACHOBJS=	aarch64_elf.o	_setup.o

CP_MACHOBJS=

S_MACHOBJS=

P_ASOBJS=	boot.o		boot_elf.o

S_ASOBJS=

CRTSRCS=	../../../../lib/crt/aarch64
CRTI=		pics/crti.o
CRTN=		pics/crtn.o
CRTS=		$(CRTI) $(CRTN)

include		../../../../Makefile.master

# This is safe because linking is done only with the primary compiler.
MAPFILE-ORDER =

include		$(SRC)/cmd/sgs/rtld/Makefile.com
include		$(SRC)/lib/Makefile.lib.64

MAPFILE-ORDER =			../common/mapfile-order-gcc

# Add any machine specific flags.
SONAME=		/lib/ld.so.1
CPPFLAGS +=	-D_ELF64

SGSMSGTARG +=	$(SGSMSGAARCH64) $(SGSMSG32) $(SGSMSG64)

.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB) $(ROOTCOMPATLINKS)

lint:		$(LINTOUT32)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

include		$(SRC)/cmd/sgs/rtld/Makefile.targ
