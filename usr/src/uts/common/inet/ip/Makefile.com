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
# Copyright 2019 Joyent, Inc.
#

MODULE		= ip
MOD_SRCDIR	= $(UTSBASE)/common/inet/ip

# IP
# XXXMK: should be sorted, but wsdiff
OBJS		=			\
		igmp.o			\
		ipmp.o			\
		ip.o			\
		ip6.o			\
		ip6_asp.o		\
		ip6_if.o		\
		ip6_ire.o		\
		ip6_rts.o		\
		ip_if.o			\
		ip_ire.o		\
		ip_listutils.o		\
		ip_mroute.o		\
		ip_multi.o		\
		ip2mac.o		\
		ip_ndp.o		\
		ip_rts.o		\
		ip_srcid.o		\
		ipddi.o			\
		ipdrop.o		\
		mi.o			\
		nd.o			\
		tunables.o		\
		optcom.o		\
		snmpcom.o		\
		ipsec_loader.o		\
		spd.o			\
		ipclassifier.o		\
		inet_common.o		\
		ip_squeue.o		\
		squeue.o		\
		ip_sadb.o		\
		ip_ftable.o		\
		proto_set.o		\
		radix.o			\
		ip_dummy.o		\
		ip_helper_stream.o	\
		ip_tunables.o		\
		ip_output.o		\
		ip_input.o		\
		ip6_input.o		\
		ip6_output.o		\
		ip_arp.o		\
		conn_opt.o		\
		ip_attr.o		\
		ip_dce.o

# ICMP
OBJS		+=	\
		icmp.o	\
		icmp_opt_data.o

# RTS
OBJS		+=	\
		rts.o	\
		rts_opt_data.o

# TCP
# XXXMK: should be sorted but wsdiff
OBJS		+=		\
		tcp.o		\
		tcp_fusion.o	\
		tcp_opt_data.o	\
		tcp_sack.o	\
		tcp_stats.o	\
		tcp_misc.o	\
		tcp_timers.o	\
		tcp_time_wait.o \
		tcp_tpi.o	\
		tcp_output.o	\
		tcp_input.o	\
		tcp_socket.o	\
		tcp_bind.o	\
		tcp_cluster.o	\
		tcp_tunables.o	\
		tcp_sig.o

# UDP
# XXXMK: should be sorted but wsdiff
OBJS		+=		\
		udp.o		\
		udp_opt_data.o	\
		udp_tunables.o	\
		udp_stats.o

# SCTP
# XXXMK: should be sorted but wsdiff
OBJS		+=			\
		sctp.o			\
		sctp_opt_data.o		\
		sctp_output.o		\
		sctp_init.o		\
		sctp_input.o		\
		sctp_cookie.o		\
		sctp_conn.o		\
		sctp_error.o		\
		sctp_snmp.o		\
		sctp_tunables.o		\
		sctp_shutdown.o		\
		sctp_common.o		\
		sctp_timer.o		\
		sctp_heartbeat.o	\
		sctp_hash.o		\
		sctp_bind.o		\
		sctp_notify.o		\
		sctp_asconf.o		\
		sctp_addr.o		\
		tn_ipopt.o		\
		tnet.o			\
		ip_netinfo.o		\
		sctp_misc.o

# ILB
# XXXMK: should be sorted, but wsdiff
OBJS		+=		\
		ilb.o		\
		ilb_nat.o	\
		ilb_conn.o	\
		ilb_alg_hash.o	\
		ilb_alg_rr.o

# Common
OBJS		+=		\
		inet_hash.o

ROOTLINK	= $(ROOT_STRMOD_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

ALL_TARGET	+= $(SRC_CONFFILE)
INSTALL_TARGET	+= $(ROOTLINK) $(ROOT_CONFFILE)

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-type-limits

# false positives
SMOFF += index_overflow

# need work still
$(OBJS_DIR)/igmp.o := SMOFF += shift_to_zero
$(OBJS_DIR)/tnet.o := SMOFF += shift_to_zero
SMOFF += signed,all_func_returns
SMOFF += signed_integer_overflow_check

#
# To get the BPF header files included by ipnet.h
#
INC_PATH	+= -I$(UTSBASE)/common/io/bpf

#
# Depends on md5 and swrand (for SCTP). SCTP needs to depend on
# swrand as it needs random numbers early on during boot before
# kCF subsystem can load swrand.
#
DEPENDS_ON	= misc/md5 crypto/swrand misc/hook misc/neti

#
# Depends on the congestion control framework for TCP connections.
# We make several different algorithms available by default.
#
DEPENDS_ON	+= misc/cc cc/cc_sunreno cc/cc_newreno cc/cc_cubic

# Need to clobber all build types due to ipctf.a
CLOBBER_DEPS	+= clobber.obj64 clobber.debug64

include $(UTSBASE)/Makefile.sischeck
include $(UTSBASE)/Makefile.kmod.targ

$(ROOTLINK):	$(ROOT_STRMOD_DIR) $(ROOTMODULE)
	-$(RM) $@; ln $(ROOTMODULE) $@

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/inet/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/inet/ilb/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/inet/sctp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/inet/tcp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/inet/udp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/inet/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/net/patricia/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

#
# The ip CTF data is merged into the genunix module because these types are
# complex and heavily shared.  The genunix build will execute one of the
# rules below to create an archive, ipctf.a, containing the ip objects.  The
# real ip will be uniquified against genunix later in the build, and will
# emerge containing very few types.
#
$(OBJS_DIR)/ipctf.a: $(OBJECTS)
	-$(RM) $@
	$(AR) -r $@ $(OBJECTS)

$(OBJECTS): $(OBJS_DIR)

CLOBBERFILES += $(OBJS_DIR)/ipctf.a

ipctf.obj64: FRC
	@BUILD_TYPE=OBJ64 VERSION='$(VERSION)' $(MAKE) obj64/ipctf.a

ipctf.debug64: FRC
	@BUILD_TYPE=DBG64 VERSION='$(VERSION)' $(MAKE) debug64/ipctf.a
