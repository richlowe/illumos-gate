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
# Copyright 2017 Hayashi Naoyuki
#

UTSBASE = ../..
UTSMACH = aarch64
include $(SRC)/uts/common/os/Makefile.genunix.com

# XXXARM: We need this because we use the NetBSD genassym,
# and so are special
$(OBJS_DIR)/float.o:	$(ASSYM_H)

$(ASSYM_H): $(GENASSYM_CF)
	$(GENASSYM) -- $(CC) $(CPPFLAGS) < $(GENASSYM_CF) > $(ASSYM_H)

