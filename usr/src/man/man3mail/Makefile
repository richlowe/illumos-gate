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

MAN3MAILFILES =	maillock.3mail

MAN3MAILSOFILES =	mailunlock.3mail	\
			touchlock.3mail

MAN3MAILFILES += $(MAN3MAILSOFILES)

mailunlock.3mail	:= SOSRC = man3mail/maillock.3mail
touchlock.3mail		:= SOSRC = man3mail/maillock.3mail

.KEEP_STATE:

include ../Makefile.man
