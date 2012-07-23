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
# Copyright 1996 Sun Microsystems, Inc. All rights reserved.
# Use is subject to license terms.
#
# @(#)Common.mk 1.3 06/12/12
#

TOP		= ../../../../..
include $(TOP)/rules/master.mk

PKG_TOP		= $(TOP)/Make

LIBBSD_DIR	= $(PKG_TOP)/libbsd
LIBBSD		= $(LIBBSD_DIR)/$(VARIANT)/libbsd.a

LIBMKSDMSI18N_DIR = $(PKG_TOP)/libmksdmsi18n
LIBMKSDMSI18N	= $(LIBMKSDMSI18N_DIR)/$(VARIANT)/libmksdmsi18n.a

LIBMKSH_DIR	= $(PKG_TOP)/libmksh
LIBMKSH		= $(LIBMKSH_DIR)/$(VARIANT)/libmksh.a

LIBVROOT_DIR	= $(PKG_TOP)/libvroot
LIBVROOT	= $(LIBVROOT_DIR)/$(VARIANT)/libvroot.a

SRC		= ../src
include $(TOP)/rules/derived.mk

