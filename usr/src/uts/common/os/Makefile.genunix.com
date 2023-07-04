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
# Copyright 2017 Hayashi Naoyuki
# Copyright 2019 Joyent, Inc.
# Copyright 2021 OmniOS Community Edition (OmniOSce) Association.
#

UTSBASE = ../..

MODULE = genunix
GENUNIX = $(OBJS_DIR)/$(MODULE)

aarch64_OBJS	=	\
	archdep.o	\
	syscall.o	\
	fpu.o		\
	float.o		\
	getcontext.o	\
	install_utrap.o	\
	kdi_svc.o

intel_OBJS	=	\
	archdep.o	\
	getcontext.o	\
	install_utrap.o	\
	lwp_private.o	\
	prom_enter.o	\
	prom_exit.o	\
	sendsig.o	\
	syscall.o

# 'PROM' routines
intel_OBJS	+=	\
	prom_env.o	\
	prom_emul.o	\
	prom_getchar.o	\
	prom_init.o	\
	prom_node.o	\
	prom_printf.o	\
	prom_prop.o	\
	prom_putchar.o	\
	prom_version.o

# Common genunix objects
OBJS	=			\
	$($(UTSMACH)_OBJS)	\
	access.o		\
	acl.o			\
	acl_common.o		\
	adjtime.o		\
	alarm.o			\
	aio_subr.o		\
	auditsys.o		\
	audit_core.o		\
	audit_zone.o		\
	audit_memory.o		\
	autoconf.o		\
	avl.o			\
	bdev_dsort.o		\
	bio.o			\
	bitext.o		\
	bitmap.o		\
	blabel.o		\
	bootbanner.o		\
	brandsys.o		\
	bz2blocksort.o		\
	bz2compress.o		\
	bz2decompress.o		\
	bz2randtable.o		\
	bz2bzlib.o		\
	bz2crctable.o		\
	bz2huffman.o		\
	callb.o			\
	callout.o		\
	chdir.o			\
	chmod.o			\
	chown.o			\
	cladm.o			\
	class.o			\
	clock.o			\
	clock_highres.o		\
	clock_process.o		\
	clock_realtime.o	\
	clock_thread.o		\
	close.o			\
	compress.o		\
	condvar.o		\
	conf.o			\
	console.o		\
	contract.o		\
	copyops.o		\
	core.o			\
	corectl.o		\
	cred.o			\
	cs_stubs.o		\
	dacf.o			\
	dacf_clnt.o		\
	damap.o			\
	cyclic.o		\
	ddi.o			\
	ddifm.o			\
	ddi_hp_impl.o		\
	ddi_hp_ndi.o		\
	ddi_intr.o		\
	ddi_intr_impl.o		\
	ddi_intr_irm.o		\
	ddi_nodeid.o		\
	ddi_periodic.o		\
	ddi_ufm.o		\
	devcfg.o		\
	devcache.o		\
	device.o		\
	devid.o			\
	devid_cache.o		\
	devid_scsi.o		\
	devid_smp.o		\
	devpolicy.o		\
	disp_lock.o		\
	dkioc_free_util.o	\
	dnlc.o			\
	driver.o		\
	dumpsubr.o		\
	driver_lyr.o		\
	dtrace_subr.o		\
	errorq.o		\
	etheraddr.o		\
	evchannels.o		\
	exacct.o		\
	exacct_core.o		\
	exec.o			\
	exit.o			\
	fbio.o			\
	fcntl.o			\
	fdbuffer.o		\
	fdsync.o		\
	fem.o			\
	ffs.o			\
	fio.o			\
	firmload.o		\
	flock.o			\
	fm.o			\
	fork.o			\
	vpm.o			\
	fs_reparse.o		\
	fs_subr.o		\
	fsflush.o		\
	ftrace.o		\
	getcwd.o		\
	getdents.o		\
	getloadavg.o		\
	getpagesizes.o		\
	getpid.o		\
	gfs.o			\
	rusagesys.o		\
	gid.o			\
	groups.o		\
	grow.o			\
	hat_refmod.o		\
	id32.o			\
	id_space.o		\
	inet_ntop.o		\
	instance.o		\
	ioctl.o			\
	ip_cksum.o		\
	issetugid.o		\
	ippconf.o		\
	kcpc.o			\
	kdi.o			\
	kiconv.o		\
	klpd.o			\
	kmem.o			\
	ksensor.o		\
	ksyms_snapshot.o	\
	l_strplumb.o		\
	labelsys.o		\
	link.o			\
	list.o			\
	lockstat_subr.o		\
	log_sysevent.o		\
	logsubr.o		\
	lookup.o		\
	lseek.o			\
	ltos.o			\
	lwp.o			\
	lwp_create.o		\
	lwp_info.o		\
	lwp_self.o		\
	lwp_sobj.o		\
	lwp_timer.o		\
	lwpsys.o		\
	main.o			\
	mmapobjsys.o		\
	memcntl.o		\
	memstr.o		\
	lgrpsys.o		\
	mkdir.o			\
	mknod.o			\
	mount.o			\
	move.o			\
	msacct.o		\
	nbmlock.o		\
	ndifm.o			\
	nice.o			\
	netstack.o		\
	ntptime.o		\
	nvpair.o		\
	nvpair_alloc_system.o	\
	nvpair_alloc_fixed.o	\
	fnvpair.o		\
	octet.o			\
	open.o			\
	p_online.o		\
	pathconf.o		\
	pathname.o		\
	pause.o			\
	serializer.o		\
	pci_intr_lib.o		\
	pci_cap.o		\
	pcifm.o			\
	pgrp.o			\
	pgrpsys.o		\
	pid.o			\
	pkp_hash.o		\
	policy.o		\
	poll.o			\
	pool.o			\
	pool_pset.o		\
	port_subr.o		\
	ppriv.o			\
	printf.o		\
	priocntl.o		\
	priv.o			\
	priv_const.o		\
	proc.o			\
	psecflags.o		\
	procset.o		\
	processor_bind.o	\
	processor_info.o	\
	profil.o		\
	project.o		\
	qsort.o			\
	getrandom.o		\
	rctl.o			\
	rctlsys.o		\
	readlink.o		\
	refhash.o		\
	refstr.o		\
	rename.o		\
	resolvepath.o		\
	retire_store.o		\
	process.o		\
	rlimit.o		\
	rmap.o			\
	rw.o			\
	rwstlock.o		\
	sad_conf.o		\
	sid.o			\
	sidsys.o		\
	sched.o			\
	schedctl.o		\
	sctp_crc32.o		\
	secflags.o		\
	seg_dev.o		\
	seg_hole.o		\
	seg_kp.o		\
	seg_kpm.o		\
	seg_map.o		\
	seg_vn.o		\
	seg_spt.o		\
	seg_umap.o		\
	semaphore.o		\
	sendfile.o		\
	session.o		\
	share.o			\
	shuttle.o		\
	sig.o			\
	sigaction.o		\
	sigaltstack.o		\
	signotify.o		\
	sigpending.o		\
	sigprocmask.o		\
	sigqueue.o		\
	sigsendset.o		\
	sigsuspend.o		\
	sigtimedwait.o		\
	sleepq.o		\
	sock_conf.o		\
	space.o			\
	sscanf.o		\
	stat.o			\
	statfs.o		\
	statvfs.o		\
	stol.o			\
	str_conf.o		\
	strcalls.o		\
	stream.o		\
	streamio.o		\
	strext.o		\
	strsubr.o		\
	strsun.o		\
	subr.o			\
	sunddi.o		\
	sunmdi.o		\
	sunndi.o		\
	sunpci.o		\
	sunpm.o			\
	sundlpi.o		\
	suntpi.o		\
	swap_subr.o		\
	swap_vnops.o		\
	symlink.o		\
	sync.o			\
	sysclass.o		\
	sysconfig.o		\
	sysent.o		\
	sysfs.o			\
	systeminfo.o		\
	task.o			\
	taskq.o			\
	tasksys.o		\
	time.o			\
	timer.o			\
	times.o			\
	timers.o		\
	thread.o		\
	tlabel.o		\
	turnstile.o		\
	tty_common.o		\
	u8_textprep.o		\
	uadmin.o		\
	uconv.o			\
	ucredsys.o		\
	uid.o			\
	umask.o			\
	umount.o		\
	uname.o			\
	unix_bb.o		\
	unlink.o		\
	upanic.o		\
	urw.o			\
	utime.o			\
	utssys.o		\
	uucopy.o		\
	vfs.o			\
	vfs_conf.o		\
	vmem.o			\
	vm_anon.o		\
	vm_as.o			\
	vm_meter.o		\
	vm_pageout.o		\
	vm_pvn.o		\
	vm_rm.o			\
	vm_seg.o		\
	vm_subr.o		\
	vm_swap.o		\
	vm_usage.o		\
	vnode.o			\
	vuid_queue.o		\
	vuid_store.o		\
	waitq.o			\
	watchpoint.o		\
	yield.o			\
	scsi_confdata.o		\
	xattr.o			\
	xattr_common.o		\
	xdr_mblk.o		\
	xdr_mem.o		\
	xdr.o			\
	xdr_array.o		\
	xdr_refer.o		\
	zone.o

# Common objects that are "not yet" kmods, in a deeply historical sense.
OBJS	+=		\
	tty_ptyconf.o	\
	ptms_conf.o	\
	vcons_conf.o

# 'Module' objects (support for modules, not actual modules)
OBJS	+=		\
	modctl.o	\
	modsubr.o	\
	modsysfile.o	\
	modconf.o	\
	modhash.o

OBJECTS		= $(OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_KERN_DIR)/$(MODULE)

LIBGEN		= $(OBJS_DIR)/libgenunix.so
LIBSTUBS	= $(GENSTUBS_OBJS:%=$(OBJS_DIR)/%)

#
#	Include common rules.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.$(UTSMACH)

#
#	Define targets
#
ALL_TARGET	= $(LIBGEN) $(GENUNIX)
INSTALL_TARGET	= $(LIBGEN) $(GENUNIX) $(ROOTMODULE)

CLOBBERFILES	+= $(GENUNIX)
CLEANFILES	+= $(LIBSTUBS) $(LIBGEN)
BINARY		=

IPCTF_TARGET	= $(IPCTF)

CPPFLAGS	+= -I$(SRC)/common
CPPFLAGS	+= -I$(SRC)/uts/common/fs/zfs

aarch64_PLATDIR	= $(UTSBASE)/armv8
intel_PLATDIR	= $(UTSBASE)/i86pc
CPPFLAGS	+= -I$($(UTSMACH)_PLATDIR)

CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-value
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= $(CNOWARN_UNINIT)
CERRWARN	+= -_gcc=-Wno-clobbered
CERRWARN	+= -_gcc=-Wno-empty-body

#
# Some compilers default to stripping unused debug information from
# objects. Since genunix is used as the uniquification source for CTF data
# in the kernel, explicitly keep as much debug data as possible.
#
CFLAGS		+= $(CALLSYMS)

# very hairy
$(OBJS_DIR)/u8_textprep.o := SMATCH=off

# false positives
SMOFF += index_overflow
$(OBJS_DIR)/seg_vn.o := SMOFF += deref_check
$(OBJS_DIR)/ddi_intr_irm.o := SMOFF += deref_check

# need work still
SMOFF += signed,all_func_returns
$(OBJS_DIR)/clock_highres.o := SMOFF += signed_integer_overflow_check
$(OBJS_DIR)/evchannels.o := SMOFF += allocating_enough_data
$(OBJS_DIR)/klpd.o := SMOFF += cast_assign
$(OBJS_DIR)/lookup.o := SMOFF += strcpy_overflow
$(OBJS_DIR)/process.o := SMOFF += or_vs_and
$(OBJS_DIR)/sunpci.o := SMOFF += deref_check
$(OBJS_DIR)/timers.o := SMOFF += signed_integer_overflow_check

# 3rd party code
$(OBJS_DIR)/bz2bzlib.o := SMOFF += indenting

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:	$(INSTALL_DEPS)

# Due to what seems to be an issue in GCC 4 generated DWARF containing
# symbolic relocations against non-allocatable .debug sections, libgenunix.so
# must be built from a stripped object, thus we create an intermediary
# libgenunix.o we can safely strip.
LIBGENUNIX_O = $(OBJS_DIR)/libgenunix.o
CLEANFILES += $(LIBGENUNIX_O)

$(LIBGENUNIX_O): $(OBJECTS)
	$(LD) -r -o $(OBJS_DIR)/libgenunix.o $(OBJECTS)
	$(STRIP) -x $(OBJS_DIR)/libgenunix.o

$(LIBGEN):	$(LIBGENUNIX_O) $(LIBSTUBS)
	$(BUILD.SO) $(LIBGENUNIX_O) $(LIBSTUBS)

$(IPCTF_TARGET) ipctf_target: FRC
	@cd $(IPDRV_DIR); pwd; $(MAKE) ipctf.$(OBJS_DIR)
	@pwd

$(GENUNIX): $(IPCTF_TARGET) $(OBJECTS)
	$(LD) -ztype=kmod $(LDFLAGS) -o $@ $(OBJECTS)
	$(CTFMERGE_GENUNIX_MERGE)
	$(POST_PROCESS)

#
#	Include common targets.
#
include $(UTSBASE)/$(UTSMACH)/Makefile.targ

#
#	Software workarounds for hardware "features".
#
$(INTEL_BLD)include	$(UTSBASE)/i86pc/Makefile.workarounds
ALL_DEFS += $(WORKAROUND_DEFS)

#
# NB: It is important these be sorted in specificity order, there are places
# where patterns overlap (though thankfully they are rare).
#
# Remember, sort by specificity (machine, platform, arch, common), then by the
# 2nd component.
#
$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/kdi/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/promif/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/$(UTSMACH)/syscall/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/contract/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/disp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/fs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/fs/swapfs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/inet/ip/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/io/scsi/conf/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/ipp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/os/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/pcmcia/cs/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/refhash/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/rpc/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/syscall/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(UTSBASE)/common/vm/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/acl/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/avl/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/bitext/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/bootbanner.o := CPPFLAGS += \
	-DBOOTBANNER1='"$(BOOTBANNER1)"' \
	-DBOOTBANNER2='"$(BOOTBANNER2)"' \
	-DBOOTBANNER3='"$(BOOTBANNER3)"' \
	-DBOOTBANNER4='"$(BOOTBANNER4)"' \
	-DBOOTBANNER5='"$(BOOTBANNER5)"'
$(OBJS_DIR)/%.o:		$(COMMONBASE)/bootbanner/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/bz2%.o:		$(COMMONBASE)/bzip2/%.c
	$(COMPILE.c) -o $@ -I$(COMMONBASE)/bzip2 $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/devid/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/exacct/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/fsreparse/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/idspace/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/list/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/net/dhcp/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/nvpair/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/secflags/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/tsol/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/unicode/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/util/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(COMMONBASE)/xattr/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
