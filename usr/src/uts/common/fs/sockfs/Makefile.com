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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= sockfs
MOD_SRCDIR	= $(UTSBASE)/common/fs/sockfs

# XXXMK: Should be sorted but wsdiff
OBJS		=			\
		socksubr.o		\
		sockvfsops.o		\
		sockparams.o		\
		socksyscalls.o		\
		socktpi.o		\
		sockstr.o		\
		sockcommon_vnops.o	\
		sockcommon_subr.o	\
		sockcommon_sops.o	\
		sockcommon.o		\
		sock_notsupp.o		\
		socknotify.o		\
		sodirect.o		\
		sockfilter.o

ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= drv/ip

#
#	Derived file "nl7ctokgen.h" defines.
#
SRCDIR		 = $(UTSBASE)/common/fs/sockfs
TOKGEN		 = $(SRCDIR)/nl7ctokgen
DERIVED_FILES	 = nl7ctokgen.h
CFLAGS		+= -I.

CLEANFILES	+= $(DERIVED_FILES)

include $(UTSBASE)/Makefile.kmod.targ

#
#	Derived file gen.
#
$(SRCDIR)/nl7chttp.c: nl7ctokgen.h

nl7ctokgen.h: $(TOKGEN) $(SRCDIR)/nl7ctokreq.txt $(SRCDIR)/nl7ctokres.txt
	/bin/ksh $(TOKGEN) $(SRCDIR)/nl7ctokreq.txt $(SRCDIR)/nl7ctokres.txt >$@
