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
# Copyright 2001 Sun Microsystems, Inc. All rights reserved.
# Use is subject to license terms.
#
# @(#)Lib.mk 1.13 06/12/12
#

# Definitions of the libraries supplied by make/dmake.
# You must correctly define TOP before including this file!

MAKE_TOP	= $(TOP)/Make

LIBBSD_DIR	= $(MAKE_TOP)/lib/bsd
LIBBSD		= $(LIBBSD_DIR)/$(VARIANT)/libbsd.a

LIBDMRXM_DIR	= $(MAKE_TOP)/lib/dmrxm
LIBDMRXM	= $(LIBDMRXM_DIR)/$(VARIANT)/libdmrxm.a

LIBDMRXS_DIR	= $(MAKE_TOP)/lib/dmrxs
LIBDMRXS	= $(LIBDMRXS_DIR)/$(VARIANT)/libdmrxs.a

LIBMAKESTATE_DIR = $(MAKE_TOP)/lib/makestate
LIBMAKESTATE	= $(LIBMAKESTATE_DIR)/$(VARIANT)/libmakestate.a

LIBMKSDMSI18N_DIR = $(MAKE_TOP)/lib/mksdmsi18n
LIBMKSDMSI18N	= $(LIBMKSDMSI18N_DIR)/$(VARIANT)/libmksdmsi18n.a

LIBMKSH_DIR	= $(MAKE_TOP)/lib/mksh
LIBMKSH		= $(LIBMKSH_DIR)/$(VARIANT)/libmksh.a

LIBVROOT_DIR	= $(MAKE_TOP)/lib/vroot
LIBVROOT	= $(LIBVROOT_DIR)/$(VARIANT)/libvroot.a

LIBDM_DIR	= $(MAKE_TOP)/lib/dm
LIBDM		= $(LIBDM_DIR)/$(VARIANT)/libdm.a

LIBDMCONF_DIR	= $(MAKE_TOP)/lib/dmconf
LIBDMCONF	= $(LIBDMCONF_DIR)/$(VARIANT)/libdmconf.a

LIBDMRC_DIR	= $(MAKE_TOP)/lib/dmrc
LIBDMRC		= $(LIBDMRC_DIR)/$(VARIANT)/libdmrc.a

LIBDMTHREAD_DIR	= $(MAKE_TOP)/lib/dmthread
LIBDMTHREAD	= $(LIBDMTHREAD_DIR)/$(VARIANT)/libdmthread.a

LIBRX_DIR	= $(MAKE_TOP)/lib/rx
LIBRX		= $(LIBRX_DIR)/$(VARIANT)/librx.a

