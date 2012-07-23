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

LIBRARY = libmksdmsi18n.a
VERS = .1
OBJECTS = libmksdmsi18n_init.o

include $(SRC)/lib/Makefile.lib
include ../../Makefile.com

LIBS = $(LIBRARY)
SRCDIR = ../
CPPFLAGS += -DI18N_VERSION=1 -D_FILE_OFFSET_BITS=64

all: $(LIBS)

install: all

lint:

include $(SRC)/lib/Makefile.targ
