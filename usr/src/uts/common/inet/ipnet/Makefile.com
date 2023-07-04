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

MODULE          = ipnet
MOD_SRCDIR     = $(UTSBASE)/common/inet/ipnet
OBJS		= ipnet.o ipnet_bpf.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET      += $(SRC_CONFFILE)
INSTALL_TARGET  += $(ROOT_CONFFILE)

DEPENDS_ON	= drv/ip misc/neti misc/hook

# To get the BPF header files
INC_PATH += -I$(UTSBASE)/common/io/bpf

include $(UTSBASE)/Makefile.kmod.targ
