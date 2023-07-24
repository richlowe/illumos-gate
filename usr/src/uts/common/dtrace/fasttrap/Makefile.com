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
# Use is subject to license terms.
#


MODULE		= fasttrap
MOD_SRCDIR	= $(UTSBASE)/common/dtrace/fasttrap
CONF_SRCDIR	= $(UTSBASE)/$(UTSMACH)/dtrace
OBJS		= fasttrap.o fasttrap_isa.o
ROOTLINK	= $(ROOT_DTRACE_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOTLINK) $(ROOT_CONFFILE)

CFLAGS		+= $(CCVERBOSE)
CERRWARN	+= $(CNOWARN_UNINIT)
CPPFLAGS	+= -I$(SRC)/common

DEPENDS_ON	= drv/dtrace

include $(UTSBASE)/Makefile.kmod.targ

$(ROOTLINK):	$(ROOT_DTRACE_DIR) $(ROOTMODULE)
	-$(RM) $@; ln $(ROOTMODULE) $@

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/dtrace/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
