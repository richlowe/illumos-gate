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
# Copyright 2013 Pluribus Networks Inc.
# Copyright 2019 Joyent, Inc.
# Copyright 2022 Oxide Computer Company
#


MODULE		= vmm

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
	 	vmm.o			\
	 	vmm_sol_dev.o		\
	 	vmm_host.o		\
	 	vmm_instruction_emul.o	\
	 	vmm_ioport.o		\
	 	vmm_lapic.o		\
	 	vmm_stat.o		\
	 	vmm_util.o		\
	 	vmm_cpuid.o		\
	 	x86.o			\
	 	iommu.o			\
	 	vatpic.o		\
	 	vatpit.o		\
	 	vhpet.o			\
	 	vioapic.o		\
	 	vlapic.o		\
	 	vrtc.o			\
	 	vpmtmr.o		\
	 	vmcs.o			\
	 	vmx_msr.o		\
	 	vmx.o			\
	 	vmx_support.o		\
	 	svm.o			\
	 	svm_msr.o		\
	 	vmcb.o			\
	 	svm_support.o		\
	 	vmm_gpt.o		\
	 	seg_vmm.o		\
	 	vmm_reservoir.o		\
	 	vmm_sol_glue.o		\
	 	vmm_sol_ept.o		\
	 	vmm_sol_rvi.o		\
	 	vmm_support.o		\
	 	vmm_vm.o		\
	 	vmm_zsd.o		\
	 	vmm_time_support.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

include $(UTSBASE)/intel/io/vmm/Makefile.vmm

ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

ALL_BUILDS	= $(ALL_BUILDSONLY64)
DEF_BUILDS	= $(DEF_BUILDSONLY64)

.PARALLEL:	$(OBJECTS)

.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/vmm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/vmm/amd/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/vmm/intel/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/vmm/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/vmm/%.S
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/vmm/intel/%.S
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/intel/io/vmm/amd/%.S
	$(COMPILE.s) -o $@ $<

$(ASSYM_VMX): $(OFFSETS_VMX) $(GENASSYM)
	$(OFFSETS_CREATE) -I$(UTSBASE)/intel/io/vmm < $(OFFSETS_VMX) >$@
$(ASSYM_SVM): $(OFFSETS_SVM) $(GENASSYM)
	$(OFFSETS_CREATE) -I$(UTSBASE)/intel/io/vmm < $(OFFSETS_SVM) >$@

$(OBJS_DIR)/vmx_support.o:  $(ASSYM_VMX)
$(OBJS_DIR)/svm_support.o:  $(ASSYM_SVM)
