#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2018, Joyent, Inc.
#

MODULE		= acpica
MOD_SRCDIR	= $(UTSBASE)/intel/io/acpica

# ACPICA objs
OBJS		=		\
		dmbuffer.o	\
		dmcstyle.o	\
		dmdeferred.o	\
		dmnames.o	\
		dmopcode.o	\
		dmresrc.o	\
		dmresrcl.o	\
		dmresrcl2.o	\
		dmresrcs.o	\
		dmutils.o	\
		dmwalk.o	\
		dsargs.o	\
		dscontrol.o	\
		dsdebug.o	\
		dsfield.o	\
		dsinit.o	\
		dsmethod.o	\
		dsmthdat.o	\
		dsobject.o	\
		dsopcode.o	\
		dspkginit.o	\
		dsutils.o	\
		dswexec.o	\
		dswload.o	\
		dswload2.o	\
		dswscope.o	\
		dswstate.o	\
		evevent.o	\
		evglock.o	\
		evgpe.o		\
		evgpeblk.o	\
		evgpeinit.o	\
		evgpeutil.o	\
		evhandler.o	\
		evmisc.o	\
		evregion.o	\
		evrgnini.o	\
		evsci.o		\
		evxface.o	\
		evxfevnt.o	\
		evxfgpe.o	\
		evxfregn.o	\
		exconcat.o	\
		exconfig.o	\
		exconvrt.o	\
		excreate.o	\
		exdebug.o	\
		exdump.o	\
		exfield.o	\
		exfldio.o	\
		exmisc.o	\
		exmutex.o	\
		exnames.o	\
		exoparg1.o	\
		exoparg2.o	\
		exoparg3.o	\
		exoparg6.o	\
		exprep.o	\
		exregion.o	\
		exresnte.o	\
		exresolv.o	\
		exresop.o	\
		exstore.o	\
		exstoren.o	\
		exstorob.o	\
		exsystem.o	\
		extrace.o	\
		exutils.o	\
		hwacpi.o	\
		hwesleep.o	\
		hwgpe.o		\
		hwpci.o		\
		hwregs.o	\
		hwsleep.o	\
		hwtimer.o	\
		hwvalid.o	\
		hwxface.o	\
		hwxfsleep.o	\
		nsaccess.o	\
		nsalloc.o	\
		nsarguments.o	\
		nsconvert.o	\
		nsdump.o	\
		nsdumpdv.o	\
		nseval.o	\
		nsinit.o	\
		nsload.o	\
		nsnames.o	\
		nsobject.o	\
		nsparse.o	\
		nspredef.o	\
		nsprepkg.o	\
		nsrepair.o	\
		nsrepair2.o	\
		nssearch.o	\
		nsutils.o	\
		nswalk.o	\
		nsxfeval.o	\
		nsxfname.o	\
		nsxfobj.o	\
		psargs.o	\
		psloop.o	\
		psobject.o	\
		psopcode.o	\
		psopinfo.o	\
		psparse.o	\
		psscope.o	\
		pstree.o	\
		psutils.o	\
		pswalk.o	\
		psxface.o	\
		rsaddr.o	\
		rscalc.o	\
		rscreate.o	\
		rsdump.o	\
		rsdumpinfo.o	\
		rsinfo.o	\
		rsio.o		\
		rsirq.o		\
		rslist.o	\
		rsmemory.o	\
		rsmisc.o	\
		rsserial.o	\
		rsutils.o	\
		rsxface.o	\
		tbdata.o	\
		tbfadt.o	\
		tbfind.o	\
		tbinstal.o	\
		tbprint.o	\
		tbutils.o	\
		tbxface.o	\
		tbxfload.o	\
		tbxfroot.o	\
		utaddress.o	\
		utalloc.o	\
		utascii.o	\
		utbuffer.o	\
		utcache.o	\
		utclib.o	\
		utcopy.o	\
		utdebug.o	\
		utdecode.o	\
		utdelete.o	\
		uterror.o	\
		uteval.o	\
		utexcep.o	\
		utglobal.o	\
		uthex.o		\
		utids.o		\
		utinit.o	\
		utlock.o	\
		utmath.o	\
		utmisc.o	\
		utmutex.o	\
		utnonansi.o	\
		utobject.o	\
		utosi.o		\
		utownerid.o	\
		utpredef.o	\
		utresdecode.o	\
		utresrc.o	\
		utstate.o	\
		utstring.o	\
		utstrsuppt.o	\
		utstrtoul64.o	\
		uttrack.o	\
		utuuid.o	\
		utxface.o	\
		utxferror.o	\
		utxfinit.o	\
		utxfmutex.o

# Illumos objs
OBJS		+=		\
		acpi_enum.o	\
		acpica.o	\
		acpica_ec.o	\
		ahids.o		\
		isapnp_devs.o	\
		osl.o		\
		osl_ml.o

ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

INC_PATH        += -I$(UTSBASE)/intel/sys/acpi
INC_PATH	+= -I$(UTSBASE)/i86pc
INC_PATH	+= -I$(SRC)/common

#
# For now, disable these warnings; maintainers should endeavor
# to investigate and remove these for maximum coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN += -_gcc=-Wno-unused-variable
CERRWARN += -_gcc=-Wno-parentheses
CERRWARN += $(CNOWARN_UNINIT)
CERRWARN += -_gcc=-Wno-unused-function

SMOFF += all_func_returns

CFLAGS += -DPWRDMN -DACPI_USE_LOCAL_CACHE -DACPI_DEBUG_OUTPUT

# We are using stack pointer value there
$(OBJS_DIR)/utdebug.o := CERRWARN += -_gcc=-Wno-dangling-pointer

include $(UTSBASE)/Makefile.kmod.targ

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/disassembler/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/dispatcher/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/events/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/executer/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/hardware/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/namespace/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/parser/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/resources/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/tables/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

$(OBJS_DIR)/%.o:		$(SRC)/common/acpica/utilities/%.c
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
