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
# Copyright 2019, Joyent, Inc.
#

MODULE		= usba
MOD_SRCDIR	= $(UTSBASE)/common/io/usb/usba

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		hcdi.o			\
		usba.o			\
		usbai.o			\
		hubdi.o			\
		parser.o		\
		genconsole.o		\
		usbai_pipe_mgmt.o	\
		usbai_req.o		\
		usbai_util.o		\
		usbai_register.o	\
		usba_devdb.o		\
		usba10_calls.o		\
		usba_ugen.o		\
		usba_bos.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod
include $(UTSBASE)/Makefile.kmod.targ
