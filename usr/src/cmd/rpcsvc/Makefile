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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#
# cmd/rpcsvc/Makefile

PROTOCOL_DIR= $(ROOT)/usr/include/rpcsvc

include ../Makefile.cmd

SUBDIRS= rpc.bootparamd
ETCFILES= rpc

TXTS= $(ETCFILES:%=net_files/%)

NETSVC=	$(ROOTLIB)/netsvc
NIS = $(NETSVC)/nis

ROOTDIRS= \
		$(NETSVC) \
		$(NIS) \

IETCFILES=		$(ETCFILES:%=$(ROOTETC)/%)

# non-default file attributes
$(ROOTETC)/rpc :=				FILEMODE=	0644

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

# install rules

$(ROOTETC)/%: ./net_files/%
	$(INS.file)

.KEEP_STATE:

all: $(TXTS) $(SUBDIRS)

install: $(TXTS) .WAIT $(ROOTDIRS) .WAIT $(IETCFILES) $(SUBDIRS)

$(ROOTDIRS):
	$(INS.dir)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

check:

clean: $(SUBDIRS)

clobber: $(SUBDIRS)

lint:

FRC:

include ../Makefile.targ
