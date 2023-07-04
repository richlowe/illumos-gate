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
# Copyright 2017 Hayashi Naoyuki
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

UTSBASE	= ../../..
include $(UTSBASE)/../Makefile.master

UNIX		= unix

# XXXARM: I might be mis-using this.  These are what would be the platmod, if
# we didn't need it so early for console I/O
CORE_OBJS += rpi4.o rpi4_stubs.o

OBJECTS		= $(SPECIAL_OBJS:%=$(OBJS_DIR)/%) \
		  $(CORE_OBJS:%=$(OBJS_DIR)/%) \
		  $(MACH_NOT_YET_KMODS:%=$(OBJS_DIR)/%)

KRTLD_MAPFILE	= $(UTSBASE)/$(MACH)/krtld/mapfile
KRTLD_OBJECTS	= $(KRTLD_OBJS:%=$(OBJS_DIR)/%)
KRTLD_O		= $(OBJS_DIR)/krtld.o

ROOTMODULE	= $(ROOT_RPI4_KERN_DIR)/$(UNIX)
UNIX_BIN	= $(OBJS_DIR)/$(UNIX)

LIBS		= $(GENLIB) $(PLATLIB) $(CPULIB)

GENUNIX_DIR	= $(UTSBASE)/$(MACH)/genunix
GENUNIX		= genunix

# XXXARM: We have no platmods as yet, but this is left here so we remember how
# they work.
#
#PLAT_DIR	= $(UTSBASE)/armv8/platmod
#PLATOPTS	= -L $(PLAT_DIR)/$(OBJS_DIR) -l $(PLATMOD)

GENOPTS		= -L $(GENUNIX_DIR)/$(OBJS_DIR) -l $(GENUNIX)

LIBOPTS		= $(GENOPTS) $(PLATOPTS)

CTFEXTRAOBJS	= $(OBJS_DIR)/vers.o

#
#	Include common rules.
#
include $(UTSBASE)/armv8/os/Makefile.unix.com
include $(UTSBASE)/common/os/Makefile.unix.com
include ../Makefile.rpi4

GENASSYM_CF		= $(UTSBASE)/armv8/ml/genassym.cf
ASSYM_H			= $(OBJS_DIR)/assym.h
INC_PATH		+= -I$(OBJS_DIR)
INC_PATH		+= -I$(UTSBASE)/armv8/rpi4
INC_PATH		+= -I$(SRC)/contrib/libfdt

#
#	Define targets
#
ALL_TARGET	= $(UNIX_BIN)
INSTALL_TARGET	= $(UNIX_BIN) $(ROOTMODULE)

#
#	This is UNIX_DIR. Use a short path.
#
UNIX_DIR	= .

CLEANFILES	+= $(UNIX_O) $(MODSTUBS_O) $(KRTLD_O) $(KRTLD_OBJECTS) \
		   $(OBJS_DIR)/vers.c $(OBJS_DIR)/vers.o \
		   $(CPU_OBJ) $(CPULIB) \
		   $(DTRACESTUBS_O) $(DTRACESTUBS_O:%.o=%.s) $(DTRACESTUBS) \
		   $(ASSYM_H) $(ZLIB_OBJS:%=$(OBJS_DIR)/%)

CLOBBERFILES	= $(CLEANFILES) $(UNIX_BIN)

#
# For now, disable these lint checks; maintainers should endeavor
# to investigate and remove these for maximum lint coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-uninitialized
CERRWARN	+= -_gcc=-Wno-char-subscripts
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-clobbered
CERRWARN	+= -_gcc=-Wno-empty-body
CERRWARN	+= -_gcc=-Wno-unused-value
CERRWARN	+= -_gcc=-Wno-switch

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:  	$(INSTALL_DEPS)

$(UNIX_O):	$(OBJECTS) $(CTFEXTRAOBJS)
	$(LD) -r -o $@ $(OBJECTS) $(OBJS_DIR)/vers.o

$(UNIX_BIN):	$(UNIX_O) $(MODSTUBS_O) $(KRTLD_O) $(MAPFILE) $(GENLIB) $(DTRACESTUBS)
	$(LD) -dy -b -o $@ -znointerp -M $(MAPFILE) \
	    $(UNIX_O) $(MODSTUBS_O) $(KRTLD_O) $(LIBOPTS) $(DTRACESTUBS)
	$(CTFMERGE_UNIQUIFY_AGAINST_GENUNIX) $(LIBGEN_OBJS)
	$(POST_PROCESS)

symcheck:	$(UNIX_O) $(KRTLD_O) $(MODSTUBS_O) $(LIBS)
	$(LD) -dy -b -o $(SYM_MOD) -M $(MAPFILE) \
	$(UNIX_O) $(KRTLD_O) $(MODSTUBS_O) $(LIBOPTS) $(DTRACESTUBS)

$(KRTLD_O):	$(KRTLD_OBJECTS)
	$(LD) -r -M$(KRTLD_MAPFILE) -o $@ $(KRTLD_OBJECTS)

# XXXARM: We have no platmods as yet, but this is left here so we remember how
# they work.
# $(PLATLIB):
# 	?@(cd $(PLAT_DIR); pwd; $(MAKE) all.targ)
# 	?@pwd

$(OBJS_DIR)/dtracestubs.s:	$(UNIX_O) $(LIBS)
	$(NM) -u $(UNIX_O) $(LIBS) | $(GREP) __dtrace_probe_ | $(SORT) | \
	    $(UNIQ) | $(AWK) \
	    '{ print "\t.globl "$$1"\n\t.type "$$1", @function\n"$$1":\n"}' \
	    > $(OBJS_DIR)/dtracestubs.s

$(DTRACESTUBS):	$(DTRACESTUBS_O)
	$(BUILD.SO) $(DTRACESTUBS_O)

$(ASSYM_H): $(GENASSYM_CF)
	$(GENASSYM) -- $(CC) $(CPPFLAGS) < $(GENASSYM_CF) > $(ASSYM_H)

#
# NB: It is important these be sorted in specificity order, there are places
# where patterns overlap (though thankfully they are rare).
#
# Remember, sort by specificity (machine, platform, arch, common), then by the
# 2nd component.
#
$(OBJS_DIR)/%.o:		$(UTSBASE)/armv8/rpi4/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/armv8/conf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/armv8/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/armv8/ml/%.S $(ASSYM_H)
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/armv8/os/%.c
	$(COMPILE.c) -_gcc=-fno-stack-protector -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/armv8/vm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/aarch64/fs/proc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/aarch64/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)


$(OBJS_DIR)/%.o:		$(UTSBASE)/aarch64/kdi/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/aarch64/kdi/%.S $(ASSYM_H)
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/aarch64/ml/%.S $(ASSYM_H)
	$(COMPILE.s) -o $@ $<

$(OBJS_DIR)/%.o:		$(UTSBASE)/aarch64/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/aarch64/promif/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/conf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/disp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/vm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/decompress.o := CPPFLAGS += -I$(SRC)/contrib/zlib
$(OBJS_DIR)/zmod.o := CPPFLAGS += -I$(SRC)/contrib/zlib
$(OBJS_DIR)/zmod_subr.o := CPPFLAGS += -I$(SRC)/contrib/zlib
$(OBJS_DIR)/%.o:		$(UTSBASE)/common/zmod/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/zlib_obj.o:		$(ZLIB_OBJS:%=$(OBJS_DIR)/%)
	$(LD) -r $(BREDUCE) -M$(UTSBASE)/common/zmod/mapfile -o $@ \
	    $(ZLIB_OBJS:%=$(OBJS_DIR)/%)
	$(CTFMERGE) $(CTFMRGFLAGS) -o $@ $(ZLIB_OBJS:%=$(OBJS_DIR)/%)

$(OBJS_DIR)/%.o:		$(SRC)/contrib/zlib/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/atomic/aarch64/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/ilstr/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/pci/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/util/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)


#
# krtld compiled into unix
#
KRTLD_INC_PATH	= -I$(UTSBASE)/common/krtld -I$(UTSBASE)/aarch64/sys
KRTLD_INC_PATH	+= -I$(UTSBASE)/aarch64/krtld

KRTLD_CPPFLAGS	= -D_KRTLD -DELF_TARGET_AARCH64 -DMODDIR_SUFFIX=\"aarch64\"

$(OBJS_DIR)/%.o:	$(UTSBASE)/aarch64/krtld/%.c
	$(COMPILE.c) $(KRTLD_INC_PATH) $(KRTLD_CPPFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:	$(UTSBASE)/aarch64/krtld/%.S
	$(COMPILE.s) $(KRTLD_INC_PATH) $(KRTLD_CPPFLAGS) -o $@ $<

$(OBJS_DIR)/%.o:	$(UTSBASE)/common/krtld/%.c
	$(COMPILE.c) $(KRTLD_INC_PATH) $(KRTLD_CPPFLAGS) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:	$(SRC)/contrib/libfdt/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:	$(SRC)/common/fs/%.c
	$(COMPILE.c) $(KRTLD_INC_PATH) $(KRTLD_CPPFLAGS) -o $@ $<
	$(CTFCONVERT_O)

#
#	Include common targets.
#
include ../Makefile.targ
