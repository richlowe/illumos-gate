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
# @(#)Makefile 1.7 06/12/12
#

include version.mk

TOP		= ../../../../..
PKG_TOP		= $(TOP)/Make
PKG_LIB_TOP	= $(PKG_TOP)/lib
I18N_LIBS	= bsd dmrxm dmrxs mksh vroot
I18N_DIRS	= $(I18N_LIBS:%=$(PKG_LIB_TOP)/%/src)
SRC		= .
APPPATH		= .
LIBNAME		= libmksdmsi18n
TEXTDOMAIN	= $(LIBNAME)_$(VERSION)
MSG_FILE	= $(TEXTDOMAIN).msg

all clean:
install: catalogs .WAIT i18n_install

#
# This is a hack until the top level makefiles for i18n/l10n message catalogs
# are better organized and written.
# This "catalogs" target was cp'ed from $(TOP)/rules/lib.mk
#
catalogs:
	$(GENMSG) -l $(SRC)/genmsg.project -o $(MSG_FILE) `find $(I18N_DIRS) \( -name '*.cc' -o -name '*.c' -o -name '*.y' \) -print | grep -v /SCCS/`
	rm -f *.cc.new

include $(PKG_TOP)/prodver.mk
include $(TOP)/rules/i18n-install.mk

