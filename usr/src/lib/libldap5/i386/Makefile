#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# lib/libldap5/i386/Makefile
#

PR_MDUSOBJS = os_SunOS_x86.o

include ../Makefile.com

LDLIBS +=	-L$(ROOTLIBDIR)/mps $(NSS_LIBS)
DYNFLAGS +=	'-R$$ORIGIN/mps'

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)

# We want $ORIGIN to work at link time, and that requires that the
# dependents are actual siblings, so copy them from adjuncts.

MPS_LIBS = nspr4 plc4 nss3 ssl3 plds4 nssutil3
ROOT_MPS_LIBS = $(MPS_LIBS:%=$(ROOTLIBDIR)/mps/lib%.so)
CLOBBERFILES += $(ROOT_MPS_LIBS)

$(DYNLIB) : rootmpslibs
rootmpslibs: $(ROOTLIBDIR)/mps $(ROOT_MPS_LIBS)

$(ROOTLIBDIR)/mps:
	$(INS.dir)

$(ROOTLIBDIR)/mps/%: $(ADJUNCT_PROTO)/usr/lib/mps/%
	$(INS.file)
