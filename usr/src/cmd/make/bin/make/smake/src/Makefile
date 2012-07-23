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
# @(#)Makefile 1.3 06/12/12
#

# Generic makefile for use in src directories.  Knows how to make common things
# in the right $(VARIANT) directory.

#TOP =		../../../../..
include $(TOP)/rules/variant.mk

all :=		TARG = all 
install :=		TARG = install 
clean :=		TARG = clean 
test :=		TARG = test
l10n_install :=	TARG = l10n_install
i18n_install :=	TARG = i18n_install

SRC =		../src
MFLAGS += 	SRC=$(SRC)

# See $(TOP)/rules/master.mk for how these are built.
%.h %.cc %.C %.E %.o all install clean test l10n_install i18n_install: FRC
	@ if [ ! -d ../$(VARIANT) ]; then \
		mkdir ../$(VARIANT) ; \
	fi
	cd ../$(VARIANT); $(MAKE) $(MFLAGS) -f $(SRC)/Variant.mk DESTDIR=$(DESTDIR) $@

FRC:
