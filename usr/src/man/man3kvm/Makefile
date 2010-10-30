#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet
# at http://www.illumos.org/license/CDDL.
#

# Copyright 2010, Richard Lowe

include ../../Makefile.master

MAN3KVMFILES = 	kvm_getu.3kvm		\
	 	kvm_kread.3kvm		\
	 	kvm_nextproc.3kvm	\
	 	kvm_nlist.3kvm		\
	 	kvm_open.3kvm		\
	 	kvm_read.3kvm

MAN3KVMSOFILES =	kvm_close.3kvm		\
		 	kvm_getcmd.3kvm		\
		 	kvm_getproc.3kvm	\
		 	kvm_kwrite.3kvm		\
		 	kvm_setproc.3kvm	\
		 	kvm_uread.3kvm		\
		 	kvm_uwrite.3kvm		\
		 	kvm_write.3kvm

MAN3KVMFILES += $(MAN3KVMSOFILES)

kvm_close.3kvm		:= SOSRC = man3kvm/kvm_open.3kvm
kvm_getcmd.3kvm		:= SOSRC = man3kvm/kvm_getu.3kvm
kvm_getproc.3kvm	:= SOSRC = man3kvm/kvm_nextproc.3kvm
kvm_kwrite.3kvm		:= SOSRC = man3kvm/kvm_kread.3kvm
kvm_setproc.3kvm	:= SOSRC = man3kvm/kvm_nextproc.3kvm
kvm_uread.3kvm		:= SOSRC = man3kvm/kvm_kread.3kvm
kvm_uwrite.3kvm		:= SOSRC = man3kvm/kvm_kread.3kvm
kvm_write.3kvm		:= SOSRC = man3kvm/kvm_read.3kvm

include ../Makefile.man
