#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2022 Oxide Computer Company
#

MODULE		= pci_prd
MOD_SRCDIR	= $(UTSBASE)/i86pc/io/pci/pci_prd
OBJS		= pci_prd_i86pc.o pci_memlist.o
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/acpica

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/pci/pci_autoconfig/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
