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

MODULE		= rpcmod
MOD_SRCDIR	= $(UTSBASE)/common/rpc

# XXXMK: should be sorted but wsdiff
OBJS		=		\
		rpcmod.o	\
		clnt_cots.o	\
		clnt_clts.o 	\
		clnt_gen.o	\
		clnt_perr.o	\
		mt_rpcinit.o	\
		rpc_calmsg.o	\
		rpc_prot.o	\
		rpc_sztypes.o	\
		rpc_subr.o	\
		rpcb_prot.o	\
		svc.o		\
		svc_clts.o	\
		svc_gen.o	\
		svc_cots.o	\
		rpcsys.o	\
		xdr_sizeof.o	\
		clnt_rdma.o	\
		svc_rdma.o	\
		xdr_rdma.o	\
		rdma_subr.o	\
		xdrrdma_sizeof.o

ROOTMODULE	= $(ROOT_STRMOD_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_SYS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INSTALL_TARGET	+= $(ROOTLINK)

DEPENDS_ON	= misc/tlimod

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-unused-function

# needs work
SMOFF += all_func_returns,indenting,no_if_block,deref_check

$(ROOTLINK):	$(ROOT_SYS_DIR) $(ROOTMODULE)
		-$(RM) $@; ln $(ROOTMODULE) $@

include $(UTSBASE)/Makefile.kmod.targ
