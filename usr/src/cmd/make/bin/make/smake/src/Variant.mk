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
# Copyright 2005 Sun Microsystems, Inc. All rights reserved.
# Use is subject to license terms.
#
# @(#)Variant.mk 1.36 06/12/12
#

TOP		= ../../../../..
include $(TOP)/rules/master.mk
include $(TOP)/Make/lib/Lib.mk

# RELEASE_VER should be "Generic" for FCS. Otherwise it should be overridden
# to display build number - "Build XX", patch number - "Patch XXXXXX-XX", etc.

PKG_TOP		= ../../../..
PROG		= make
PACKAGE		= SUNWspro
PRODVER		= 11
PRODVER_V	= SunOS 5.11
RELEASE_VER	= Generic
DATE:sh		= date '+%B %Y'

VERSTRING	= RELEASE VERSION $(PRODVER_V) $(RELEASE_VER) $(DATE)

MORE_SRC	= \
		ar.cc \
		depvar.cc \
		doname.cc \
		dosys.cc \
		files.cc \
		globals.cc \
		implicit.cc \
		macro.cc \
		main.cc \
		misc.cc \
		nse_printdep.cc \
		read.cc \
		read2.cc \
		rep.cc \
		state.cc

CPPFLAGS	+= -I$(PKG_TOP)/include

LDLIB		+= \
		-lnsl \
		-lsocket \
		-lw

HDRS_DIR	= $(PKG_TOP)/include/mk
HDRS_LIST	= $(HDRS_DIR)/copyright.h $(HDRS_DIR)/defs.h

.INIT: $(HDRS_LIST)

SRCS = $(PROG).cc $(MORE_SRC)
OBJS = $(SRCS:%.cc=%.o)

LIB +=		-lintl -lm

all install:: $(PROG)

$(PROG): $(OBJS)
	$(CCC) $(CCFLAGS) $(LDFLAGS) -o $@ \
		$(OBJS) $(LIB) $(I18LIB) $(LDLIB)
	/bin/sh $(TOP)/exe/sanity-check.sh $(TOP) $(PROG)

clean:
	$(RM) $(OBJS) $(PROG)


include $(TOP)/rules/computed-options.mk

#
# This LIB macro must be declared after the include's above
#
LIB		= \
		$(LIBMKSH) \
		$(LIBMKSDMSI18N) \
		$(LIBBSD) \
		$(LIBVROOT)

%.o: ../../common/%.cc
	$(COMPILE.cc) $(OUTPUT_OPTION) $<

depvar.o nse.o nse_printdep.o := CPPFLAGS += -DSUNOS4_AND_AFTER

LDFLAGS +=	-xildoff -norunpath

install:: install-make-hdrs install-make-bin

IHDR		= make.rules.file
VIHDR		= svr4.make.rules.file
HDR		= ${PKG_TOP}/bin/make/common/make.rules.file
VHDR		= ${PKG_TOP}/bin/make/common/svr4.make.rules.file
HDRSDIR		= $(DESTDIR)/usr/share/lib/make
HDRFILE		= make.rules
VHDRFILE	= svr4.make.rules
SMAKEDIR	= $(DESTDIR)/usr/ccs/bin

install-make-bin: make
	$(INSTALL) -d $(SMAKEDIR)
	$(INSTALL) make $(SMAKEDIR) 
	mcs -d $(SMAKEDIR)/make
	mcs -a '@(#)$(VERSTRING)' $(SMAKEDIR)/make

install-make-hdrs: $(HDR) $(VHDR)
	$(INSTALL) -d $(HDRSDIR)
	$(INSTALL) -m 0444 $(HDR) $(HDRSDIR)
	mv -f $(HDRSDIR)/$(IHDR) $(HDRSDIR)/$(HDRFILE)
	$(INSTALL) -m 0444 $(VHDR) $(HDRSDIR)
	mv -f $(HDRSDIR)/$(VIHDR) $(HDRSDIR)/$(VHDRFILE)


#
# i18n stuff
#
MAKE_MSG	= SUNW_SPRO_MAKE.msg
#I18N_DIRS	= ../../common ../../../../lib/bsd/src ../../../../lib/mksh/src ../../../../lib/vroot/src
I18N_DIRS	= ../../common
TEXTDOMAIN	= SUNW_SPRO_MAKE
APPPATH		= $(PKG_TOP)/bin/make/smake/$(VARIANT)
LIB_DESTDIR	= $(DESTDIR)/usr/lib
CAT_DESTDIR	= $(LIB_DESTDIR)/locale/C/LC_MESSAGES

$(CAT_DESTDIR):
	$(INSTALL) -d $@

msg_catalogs: $(CAT_DESTDIR) .WAIT MAKE_MSG
	cp $(APPPATH)/$(TEXTDOMAIN).msg $(CAT_DESTDIR)

MAKE_MSG:
	$(GENMSG) -l $(SRC)/genmsg.project -o $(TEXTDOMAIN).msg `find $(I18N_DIRS) \( -name '*.cc' -o -name '*.c' -o -name '*.y' -o -name '*.h' \) -print | grep -v /SCCS/`
	rm -f *.cc.new

i18n_install: msg_catalogs 

FRC:
