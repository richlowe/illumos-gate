#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

include $(SRC)/Makefile.master

SUBDIRS	=	$(MACH64)
$(BUILD32)SUBDIRS += $(MACH)

VERS = .1

include $(SRC)/lib/Makefile.lib

ENCODING  = ISO8859

SRCDIR=	src

HDRS=           sqlite3.h sqlite3ext.h
ROOTHDRDIR=     $(ROOT)/usr/include/sqlite3-sys
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=      $(HDRS:%.h=%.check)

.KEEP_STATE:

all	:= TARGET= all
clean	:= TARGET= clean
clobber	:= TARGET= clobber
install	:= TARGET= install


# This is the default Makefile target.  The objects listed here
# are what get build when you type just "make" with no arguments.
#
all install:	$(SUBDIRS)
clobber:	$(SUBDIRS)

$(ROOTHDRDIR):
	$(INS.dir)

$(ROOTHDRDIR)/%: % $(ROOTHDRDIR)
	$(INS.file)

install_h:	$(ROOTHDRS)

check:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include $(SRC)/lib/Makefile.targ

.PARALLEL: $(SUBDIRS)
