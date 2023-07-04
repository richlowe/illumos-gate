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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

MODULE		= softmac
MOD_SRCDIR	= $(UTSBASE)/common/io/softmac

# XXXMK: Should be sorted, but wsdiff
OBJS		=		\
		softmac_main.o	\
		softmac_ctl.o	\
		softmac_capab.o \
		softmac_dev.o	\
		softmac_stat.o	\
		softmac_pkt.o	\
		softmac_fp.o

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOT_CONFFILE)

DEPENDS_ON	=	\
	drv/dld		\
	misc/mac	\
	misc/strplumb	\
	misc/dls

#
# For now, disable these warnings as it is a generic STREAMS problem;
# maintainers should endeavor to investigate and remove these for maximum
# coverage.
#
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-unused-label

include $(UTSBASE)/Makefile.kmod.targ
