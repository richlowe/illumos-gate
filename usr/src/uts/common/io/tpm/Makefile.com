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
# Copyright (c) 2018, Joyent, Inc.
# Copyright 2021 Jason King

MODULE		= tpm
MOD_SRCDIR	= $(UTSBASE)/common/io/tpm
OBJS		= tpm.o tpm_hcall.o

include $(UTSBASE)/Makefile.kmod

# This is for everything except /usr/include/tss/
INC_PATH	+= -I$(ROOT)/usr/include
# This is for /usr/include/tss/, which is not built in illumos
INC_PATH	+= -I$(ADJUNCT_PROTO)/usr/include

CERRWARN	+= -_gcc=-Wno-parentheses

include $(UTSBASE)/Makefile.kmod.targ
