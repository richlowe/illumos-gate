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

MODULE		= ath
MOD_SRCDIR	= $(UTSBASE)/common/io/ath

OBJS		=		\
		ath_aux.o	\
		ath_main.o	\
		ath_osdep.o	\
		ath_rate.o	\
		hal.o

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/mac misc/net80211

CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-empty-body

# needs work
$(OBJS_DIR)/ath_rate.o := SMOFF += index_overflow

include $(UTSBASE)/Makefile.kmod.targ

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
