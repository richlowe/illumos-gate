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
# Copyright (c) 2018, Joyent, Inc.
#


#
# Define the module and object file sets.
#
MODULE		= iscsi

# XXXMK: Should be sorted, but wsdiff
OBJS		=			\
		chap.o			\
		iscsi_io.o		\
		iscsi_thread.o		\
		iscsi_ioctl.o		\
		iscsid.o		\
		iscsi.o			\
		iscsi_login.o		\
		isns_client.o		\
		iscsiAuthClient.o	\
		iscsi_lun.o		\
		iscsiAuthClientGlue.o	\
		iscsi_net.o		\
		nvfile.o		\
		iscsi_cmd.o		\
		iscsi_queue.o		\
		persistent.o		\
		iscsi_conn.o		\
		iscsi_sess.o		\
		radius_auth.o		\
		iscsi_crc.o		\
		iscsi_stats.o		\
		radius_packet.o		\
		iscsi_doorclt.o		\
		iscsi_targetparam.o	\
		utils.o			\
		kifconf.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/scsi/adapters/iscsi

#
# Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
# Define targets.
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

# includes
INC_PATH	+= -I$(UTSBASE)/common/io/scsi/adapters/iscsi
INC_PATH	+= -I$(SRC)/common/hdcrc

#
# Note dependancy on misc/scsi.
#
LDFLAGS += -Nmisc/scsi -Nfs/sockfs -Nsys/doorfs -Nmisc/md5 -Nmisc/ksocket
LDFLAGS += -Nmisc/idm


CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= $(CNOWARN_UNINIT)

# needs work
SMATCH=off

#
# Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

#
# Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/scsi/adapters/iscsi/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:                $(UTSBASE)/common/inet/kifconf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/iscsi/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
