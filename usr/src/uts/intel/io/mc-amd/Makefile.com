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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

include $(SRC)/common/mc/mc-amd/Makefile.mcamd

MODULE		= mc-amd
MOD_SRCDIR	= $(UTSBASE)/intel/io/mc-amd

OBJS		=		\
	mcamd_dimmcfg.o		\
	mcamd_drv.o		\
	mcamd_err.o		\
	mcamd_misc.o		\
	mcamd_patounum.o	\
	mcamd_pcicfg.o		\
	mcamd_rowcol.o		\
	mcamd_rowcol_tbl.o	\
	mcamd_subr.o		\
	mcamd_synd.o		\
	mcamd_unumtopa.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

INC_PATH	+= -I$(MOD_SRCDIR) -I$(OBJS_DIR) -I$(SRC)/common/mc/mc-amd
INC_PATH	+= -I$(SRC)/common/util

MCAMD_OFF_H	= $(OBJS_DIR)/mcamd_off.h
MCAMD_OFF_SRC	= $(MOD_SRCDIR)/mcamd_off.in

CLEANFILES	+= $(MCAMD_OFF_H)
CLOBBERFILES	+= $(MCAMD_OFF_H)

CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)

$(OBJECTS): $(OBJS_DIR) $(MCAMD_OFF_H)

#
# Create mcamd_off.h
#
$(MCAMD_OFF_H): $(MCAMD_OFF_SRC)
	$(OFFSETS_CREATE) <$(MCAMD_OFF_SRC) >$@

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/io/mc-amd/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/mc/mc-amd/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
