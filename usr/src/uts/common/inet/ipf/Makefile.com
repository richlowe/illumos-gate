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

MODULE		= ipf
MOD_SRCDIR	= $(UTSBASE)/common/inet/ipf

OBJS		=			\
		drand48.o		\
		fil.o			\
		ip_auth.o		\
		ip_compat.o		\
		ip_fil_solaris.o	\
		ip_frag.o		\
		ip_htable.o		\
		ip_log.o		\
		ip_lookup.o		\
		ip_nat.o		\
		ip_nat6.o		\
		ip_pool.o		\
		ip_proxy.o		\
		ip_state.o		\
		misc.o			\
		solaris.o

ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

CPPFLAGS += -DIPFILTER_LKM -DIPFILTER_LOG -DIPFILTER_LOOKUP -DUSE_INET6
CPPFLAGS += -DSUNDDI -DSOLARIS2=$(RELEASE_MINOR) -DIRE_ILL_CN

DEPENDS_ON =		\
	drv/ip		\
	misc/hook	\
	misc/kcf	\
	misc/mac	\
	misc/md5	\
	misc/neti

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

include $(UTSBASE)/Makefile.sischeck
include $(UTSBASE)/Makefile.kmod.targ
