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
# Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#


#
#	Define the module and object file sets.
#
MODULE		= ath

OBJS		=		\
		ath_aux.o	\
		ath_main.o	\
		ath_osdep.o	\
		ath_rate.o	\
		hal.o
OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Driver depends on GLDv3 & wifi kernel support module.
#
LDFLAGS		+= -Nmisc/mac -Nmisc/net80211

CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-empty-body

# needs work
$(OBJS_DIR)/ath_rate.o := SMOFF += index_overflow

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:               $(UTSBASE)/common/io/ath/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

#
#	The amd64 version of this object has the .eh_frame section tagged
#	as SHT_PROGBITS, while the ABI requires SHT_AMD64_UNWIND. The Solaris
#	ld enforces this, so use elfedit to bring the object in line with
#	this requirement.
#
ATHEROS_HAL=$(UTSBASE)/common/io/ath/hal_x86_$(CLASS).o.uu
$(OBJS_DIR)/hal.o:     $(ATHEROS_HAL)
	uudecode -o $@ $(ATHEROS_HAL)
	if [ `elfedit -r -e 'ehdr:e_machine' $@` = EM_AMD64 ]; \
		then elfedit -e 'shdr:sh_type .eh_frame SHT_AMD64_UNWIND' $@; fi
