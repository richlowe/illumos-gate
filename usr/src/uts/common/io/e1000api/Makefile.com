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

# Copyright 2023 Richard Lowe


#
#	e1000/igb common objs
#
#	Historically e1000g and igb had separate copies of all of the common
#	code. At this time while they are now sharing the same copy of it, they
#	are building it into their own modules which is due to the differences
#	in the osdep and debug portions of their code.
#
#	Client Makefiles should include this file and append $(E1000API_OBJS)
#	to their objects list
#

# XXXMK: Should be sorted, but wsdiff
E1000API_OBJS =				\
		e1000_80003es2lan.o	\
		e1000_82540.o		\
		e1000_82541.o		\
		e1000_82542.o		\
		e1000_82543.o		\
		e1000_82571.o		\
		e1000_api.o		\
		e1000_ich8lan.o		\
		e1000_mac.o		\
		e1000_manage.o		\
		e1000_nvm.o		\
		e1000_phy.o		\
		e1000_82575.o		\
		e1000_i210.o		\
		e1000_mbx.o		\
		e1000_vf.o		\
		e1000_illumos.o

E1000API_CFLAGS=	-I$(UTSBASE)/common/io/e1000api
