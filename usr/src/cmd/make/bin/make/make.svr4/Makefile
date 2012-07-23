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
# @(#)Makefile 1.6 06/12/12
#

TOP=../../../..
include $(TOP)/rules/master.mk

all clean clobber l10n_install:

install:
	$(INSTALL) -d $(DESTDIR)/usr/lib
	$(INSTALL) -d $(DESTDIR)/usr/ccs/lib
	rm -f $(DESTDIR)/usr/lib/svr4.make
	rm -f $(DESTDIR)/usr/ccs/lib/svr4.make
	ln $(DESTDIR)/usr/ccs/bin/make $(DESTDIR)/usr/lib/svr4.make
	(cd $(DESTDIR)/usr/ccs/lib; ln -s ../../lib/svr4.make svr4.make)
