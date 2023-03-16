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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright 2017 Hayashi Naoyuki

@ _START_

# Message file for cmd/sgs/rtld (ld.so.1) - 64bit aarch64 specific.

@ MSG_REL_NOFIT		"relocation error: %s: file %s: symbol %s: \
			 value 0x%llx does not fit"

@ MSG_REL_VALNONALIGN	"relocation error: %s: file %s: symbol %s: \
			 value 0x%llx is non-aligned"
@ _END_

@ MSG_SYM_LAAARCH64PLTENTER	"la_aarch64_pltenter"

@ MSG_SYM_PLT		"_PROCEDURE_LINKAGE_TABLE_"
