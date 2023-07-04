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

MODULE		= xpvtap
MOD_SRCDIR	= $(UTSBASE)/common/xen/io/xpvtap
OBJS		= xpvtap.o blk_common.o seg_mf.o
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

#
# use Solaris specific code in xen public header files
#
ALL_DEFS	+= -D_SOLARIS

CERRWARN	+= -_gcc=-Wno-unused-label

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/i86xpv/vm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
