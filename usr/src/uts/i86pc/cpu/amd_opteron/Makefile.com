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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2016 Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= cpu_ms.AuthenticAMD.15
MOD_SRCDIR	= $(UTSBASE)/i86pc/cpu/amd_opteron

OBJS		=		\
		ao_cpu.o	\
		ao_main.o	\
		ao_mca.o	\
		ao_mca_disp.o	\
		ao_poll.o

ROOTMODULE      = $(ROOT_PSM_CPU_DIR)/$(MODULE)

AO_MCA_DISP_C	= $(OBJS_DIR)/ao_mca_disp.c
AO_MCA_DISP_SRC = $(MOD_SRCDIR)/ao_mca_disp.in
AO_GENDISP	= $(UTSBASE)/i86pc/cpu/scripts/ao_gendisp

include $(UTSBASE)/Makefile.kmod

$(OBJS_DIR)/ao_mca.o :=	CERRWARN	+= -_gcc=-Wno-unused-function

CLEANFILES	+= $(AO_MCA_DISP_C)
CPPFLAGS	+= -I$(MOD_SRCDIR) -I$(OBJS_DIR)

DEPENDS_ON	= misc/acpica

include $(UTSBASE)/Makefile.kmod.targ

#
# Create ao_mca_disp.c
#
$(AO_MCA_DISP_C): $(AO_MCA_DISP_SRC) $(AO_GENDISP)
	$(AO_GENDISP) $(AO_MCA_DISP_SRC) >$@

$(OBJS_DIR)/%.o:		$(OBJS_DIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
