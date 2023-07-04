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

MODULE		= idmap
MOD_SRCDIR	= $(UTSBASE)/common/idmap

# XXXMK: Should be sorted but wsdiff
OBJS		=		\
		idmap_mod.o	\
		idmap_kapi.o	\
		idmap_xdr.o	\
		idmap_cache.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON = sys/doorfs strmod/rpcmod

#
# Function variables unused for rpcgen-generated code
# Constant conditions for do { } while (0) macros
#
CERRWARN	+= -_gcc=-Wno-unused-variable

include $(UTSBASE)/Makefile.kmod.targ
