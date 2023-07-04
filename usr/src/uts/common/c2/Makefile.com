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
# Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
#

MODULE		= c2audit
MOD_SRCDIR	= $(UTSBASE)/common/c2

# XXXMK: Should be sorted but wsdiff
OBJS		=			\
		adr.o			\
		audit.o			\
		audit_event.o		\
		audit_io.o		\
		audit_path.o		\
		audit_start.o		\
		audit_syscalls.o	\
		audit_token.o		\
		audit_mem.o

ROOTMODULE	= $(ROOT_SYS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON = fs/sockfs

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-clobbered
CERRWARN	+= $(CNOWARN_UNINIT)

include $(UTSBASE)/Makefile.kmod.targ
