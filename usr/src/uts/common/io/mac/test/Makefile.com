
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2023 Oxide Computer Company
#

MODULE		= mac_test
MOD_SRCDIR	= $(UTSBASE)/common/io/mac/test
ROOTMODULE	= $(USR_KTEST_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

DEPENDS_ON	= misc/mac drv/ktest

include $(UTSBASE)/Makefile.kmod.targ
