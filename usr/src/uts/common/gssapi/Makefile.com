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
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= kgssapi
MOD_SRCDIR	= $(UTSBASE)/common/gssapi

OBJS		=			\
		gen_oids.o		\
		gss_display_name.o	\
		gss_import_name.o	\
		gss_release_buffer.o	\
		gss_release_name.o	\
		gss_release_oid_set.o	\
		gssd_clnt_stubs.o	\
		gssd_handle.o		\
		gssd_prot.o		\
		gssdmod.o

# Derived
OBJS		+= \
		gssd_xdr.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INSTALL_TARGET	+= $(ROOT_KGSS_DIR)

DEPENDS_ON =		\
	misc/rpcsec	\
	misc/tlimod	\
	strmod/rpcmod

INC_PATH += -I$(UTSBASE)/common/gssapi/include

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
$(OBJS_DIR)/gssd_clnt_stubs.o := SMOFF += indenting,deref_check

include $(UTSBASE)/Makefile.kmod.targ
