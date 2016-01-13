#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

PR_MDUSOBJS = os_SunOS_x86.o

include ../Makefile.com
include ../../Makefile.lib.64

LDLIBS +=	-L$(ROOTLIBDIR)/mps/64 $(NSS_LIBS)
DYNFLAGS +=	'-R$$ORIGIN/../mps/64'

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64) 

# We want $ORIGIN to work at link time, and that requires that the
# dependents are actual siblings, so copy them from adjuncts.

MPS_LIBS = nspr4 plc4 nss3 ssl3 plds4 nssutil3
ROOT_MPS_LIBS = $(MPS_LIBS:%=$(ROOTLIBDIR)/mps/64/lib%.so)
CLOBBERFILES += $(ROOT_MPS_LIBS)

$(DYNLIB) : rootmpslibs
rootmpslibs: $(ROOTLIBDIR)/mps/64 $(ROOT_MPS_LIBS)

$(ROOTLIBDIR)/mps/64:
	$(INS.dir)

$(ROOTLIBDIR)/mps/64/%: $(ADJUNCT_PROTO)/usr/lib/mps/64/%
	$(INS.file)
