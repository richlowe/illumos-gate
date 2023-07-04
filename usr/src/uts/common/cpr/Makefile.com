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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright 2016, Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= cpr
MOD_SRCDIR	= $(UTSBASE)/common/cpr

# XXXMK: this object section is constructed to aid wsdiff, it need not be
# this bad, and could be in intel/Makefile were it not for that.
i86pc_OBJS	= cpr_impl.o cpr_wakecode.o
i386_OBJS	= cpr_intel.o

# XXXMK: Beware the difference between MACH and UTSMACH
# Common
OBJS		=			\
		$($(UTSMACH)_OBJS)	\
		cpr_driver.o		\
		cpr_dump.o		\
		cpr_main.o		\
		cpr_misc.o		\
		cpr_mod.o		\
		cpr_stat.o		\
		cpr_uthread.o		\
		$($(MACH)_OBJS)

ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

#
#	bootdev required as per previous inline commenting referencing symbol
#	i_devname_to_promname(), which may only be necessary on SPARC. Removing
#	this symbol may be sufficient to remove dependency.
#
DEPENDS_ON	= misc/acpica misc/bootdev

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-parentheses

$(OBJS_DIR)/cpr_impl.o :=	CERRWARN	+= -_gcc=-Wno-unused-function

include $(UTSBASE)/Makefile.kmod.targ
