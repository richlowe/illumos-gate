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
# Copyright 2019 Joyent, Inc.
# Copyright 2019 OmniOS Community Edition (OmniOSce) Association.
#

MODULE		= pcie
MOD_SRCDIR	= $(UTSBASE)/i86pc/io/pciex

i86pc_OBJS	=  		\
		pcie_acpi.o	\
		pciehpc_acpi.o	\
		pcie_x86.o

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		$($(UTSMACH)_OBJS)	\
		pcie.o			\
		pcie_fault.o		\
		pcie_hp.o		\
		pciehpc.o		\
		pcishpc.o		\
		pcie_pwr.o		\
		pciev.o			\
		pci_props.o		\
		pci_strings.o

ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/acpica misc/busra misc/pci_prd

INC_PATH	+= -I$(SRC)/common/pci

CERRWARN	+= -_gcc=-Wno-unused-value
CERRWARN	+= -_gcc=-Wno-unused-function # safe

# needs work
SMOFF += all_func_returns,deref_check

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/pciex/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/pciex/hotplug/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/pciex/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/pciex/hotplug/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/pci/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
