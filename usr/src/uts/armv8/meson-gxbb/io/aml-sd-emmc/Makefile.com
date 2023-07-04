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
# Copyright 2017 Hayashi Naoyuki
#

MODULE		= aml-sd-emmc
MOD_SRCDIR	= $(UTSBASE)/armv8/meson-gxbb/io/aml-sd-emmc
ROOTMODULE	= $(ROOT_MESON_GXBB_DRV_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INC_PATH	+= -I$(SRC)/uts/armv8/meson-gxbb/

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN += -_gcc=-Wno-unused-function

DEPENDS_ON = drv/blkdev

include $(UTSBASE)/Makefile.kmod.targ
