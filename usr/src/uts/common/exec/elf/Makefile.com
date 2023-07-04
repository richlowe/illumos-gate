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
# Copyright 2019, Joyent, Inc.
#

MODULE		= elfexec
MOD_SRCDIR	= $(UTSBASE)/common/exec/elf

intel_OBJS	= elf32.o elf32_notes.o old32_notes.o

# XXXMK: should be sorted, but wsdiff
OBJS		=			\
		$($(UTSMACH)_OBJS)	\
		elf.o			\
		elf_notes.o		\
		old_notes.o		\
		core_shstrtab.o

ROOTMODULE	= $(ROOT_EXEC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(SRC)/common/core

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(COMMONBASE)/core/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/elf32.o:	$(UTSBASE)/common/exec/elf/elf.c
	$(COMPILE.c) -o $@ -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/elf.c
	$(CTFCONVERT_O)

$(OBJS_DIR)/elf32_notes.o: $(UTSBASE)/common/exec/elf/elf_notes.c
	$(COMPILE.c) -o $@ -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/elf_notes.c
	$(CTFCONVERT_O)

$(OBJS_DIR)/old32_notes.o: $(UTSBASE)/common/exec/elf/old_notes.c
	$(COMPILE.c) -o $@ -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/old_notes.c
	$(CTFCONVERT_O)
