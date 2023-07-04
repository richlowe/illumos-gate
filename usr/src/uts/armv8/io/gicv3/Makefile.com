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
# Copyright 2024 Michael van der Westhuizen
#

#
#	Define the module and object file sets.
#
MODULE		= gicv3
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
MOD_SRCDIR	= $(UTSBASE)/armv8/io/gicv3

include $(UTSBASE)/Makefile.kmod
include $(UTSBASE)/Makefile.kmod.targ
