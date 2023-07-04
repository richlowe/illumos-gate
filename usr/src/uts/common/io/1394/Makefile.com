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

MODULE		= s1394
MOD_SRCDIR	= $(UTSBASE)/common/io/1394

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		t1394.o			\
		t1394_errmsg.o		\
		s1394.o			\
		s1394_addr.o		\
		s1394_asynch.o		\
		s1394_bus_reset.o	\
		s1394_cmp.o		\
		s1394_csr.o		\
		s1394_dev_disc.o	\
		s1394_fa.o		\
		s1394_fcp.o		\
		s1394_hotplug.o		\
		s1394_isoch.o		\
		s1394_misc.o		\
		h1394.o			\
		nx1394.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-parentheses

# needs work
SMOFF += indenting

include $(UTSBASE)/Makefile.kmod.targ
