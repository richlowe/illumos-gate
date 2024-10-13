#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
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

LIBRARY = libfdt.a
OBJECTS = \
	fdt.o fdt_ro.o fdt_strerror.o fdt_wip.o fdt_empty_tree.o \
	fdt_rw.o fdt_sw.o fdt_addresses.o

include ../Makefile.com

CPPFLAGS += -I$(SRC)/contrib/libfdt

include ../Makefile.targ

objs/%.o: $(SRC)/contrib/libfdt/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

