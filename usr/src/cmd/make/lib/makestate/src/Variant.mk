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
# Copyright 2005 Sun Microsystems, Inc. All rights reserved.
# Use is subject to license terms.
#
# @(#)Variant.mk 1.12 06/12/12
#

TOP =		../../../..

%.o: $(SRC)/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

include $(TOP)/rules/master.mk

PKG_TOP =	$(TOP)/Make

CSRCS =	\
		ld_file.c \
		lock.c

LIBNAME =	libmakestate.a
DLIBNAME =	libmakestate.so.1
MSG_FILE = libmakestate.msg
I18N_DIRS = $(SRC)

CFLAGS += $(V9FLAGS) -KPIC -DTEXT_DOMAIN=\"SUNW_OST_OSLIB\"
CPPFLAGS=

#include $(TOP)/Make/lib/Lib.mk
include $(TOP)/rules/lib.mk

POUND_SIGN:sh=	echo \#
RELEASE=	5.11
VERSION=	$(RELEASE_VER)
PATCHID=	$(VERSION)
DATE:sh		= date '+%B %Y'
RELEASE_DATE=	$(DATE)
PATCH_DATE=	$(RELEASE_DATE)
RELEASE_CM=	"@($(POUND_SIGN))RELEASE VERSION SunOS $(RELEASE) $(PATCHID) $(PATCH_DATE)"

PROCESS_COMMENT=	mcs -a $(RELEASE_CM)
POST_PROCESS_SO=	$(PROCESS_COMMENT) $@

$(DLIBNAME) : $(LIBNAME)
	$(CC) $(V9FLAGS) -o $@ -dy -G -ztext -h $@ ld_file.o lock.o -lelf
	mcs -d $@
	${POST_PROCESS_SO}

all: $(DLIBNAME)

install: all
	${INSTALL} -d ${DESTDIR}/usr/lib$(VAR_DIR)
	${RM} ${DESTDIR}/usr/lib$(VAR_DIR)/$(DLIBNAME)
	${INSTALL} $(DLIBNAME) ${DESTDIR}/usr/lib$(VAR_DIR)
