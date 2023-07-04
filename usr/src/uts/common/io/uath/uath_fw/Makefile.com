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

MODULE		= uathfw
MOD_SRCDIR	= $(UTSBASE)/common/io/uath/uath_fw
OBJS		= uathfw_mod.o
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

FWOBJ		= $(OBJS_DIR)/$(MODULE).o

include $(UTSBASE)/Makefile.kmod

OBJECTS		+= $(FWOBJ)

# customized section for transforming firmware binary
BINSRC		= $(MOD_SRCDIR)
BINDEST		= $(UTSBASE)/$(UTSMACH)/uathfw
BINCOPY		= uathbin
ORIGIN_SRC	= uathfw.bin

#get build type, 32/64
WRAPTYPE	= $(CLASS:%=-%)

#set elfwrap option
WRAPOPT		= $(WRAPTYPE:-32=)

$(FWOBJ):
	cp $(BINSRC)/$(ORIGIN_SRC) $(BINDEST)/$(BINCOPY)
	$(ELFWRAP) $(WRAPOPT) -o $@ $(BINDEST)/$(BINCOPY)

CLOBBERFILES += $(BINDEST)/$(BINCOPY)

include $(UTSBASE)/Makefile.kmod.targ
