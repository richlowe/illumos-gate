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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= ipf

# XXXMK: should be sorted, but wsdiff
OBJS		=			\
		ip_fil_solaris.o	\
		fil.o			\
		solaris.o		\
		ip_state.o		\
		ip_frag.o		\
		ip_nat.o		\
		ip_proxy.o		\
		ip_auth.o		\
		ip_pool.o		\
		ip_htable.o		\
		ip_lookup.o		\
		ip_log.o		\
		misc.o			\
		ip_compat.o		\
		ip_nat6.o		\
		drand48.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/inet/ipf

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFFILE)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

CPPFLAGS += -DIPFILTER_LKM -DIPFILTER_LOG -DIPFILTER_LOOKUP -DUSE_INET6
CPPFLAGS += -DSUNDDI -DSOLARIS2=$(RELEASE_MINOR) -DIRE_ILL_CN
LDFLAGS += -Ndrv/ip -Nmisc/md5 -Nmisc/neti -Nmisc/hook -Nmisc/kcf
LDFLAGS += -Nmisc/mac

INC_PATH += -I$(UTSBASE)/common/inet/ipf

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-empty-body
CCWARNINLINE	=

# needs work
SMATCH=off

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS) $(SISCHECK_DEPS)

clean:		$(CLEAN_DEPS) $(SISCLEAN_DEPS)

clobber:	$(CLOBBER_DEPS) $(SISCLEAN_DEPS)

install:	$(INSTALL_DEPS) $(SISCHECK_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/inet/ipf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
