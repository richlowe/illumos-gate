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

MAN3SNMPFILES =	SSAAgentIsAlive.3snmp		\
	 	SSAOidCmp.3snmp			\
	 	SSAStringCpy.3snmp

MAN3SNMPSOFILES =	SSAGetTrapPort.3snmp	\
		 	SSAOidCpy.3snmp		\
		 	SSAOidDup.3snmp		\
		 	SSAOidFree.3snmp	\
		 	SSAOidInit.3snmp	\
		 	SSAOidNew.3snmp		\
		 	SSAOidStrToOid.3snmp	\
		 	SSAOidString.3snmp	\
		 	SSAOidZero.3snmp	\
		 	SSARegSubagent.3snmp	\
		 	SSARegSubtable.3snmp	\
		 	SSARegSubtree.3snmp	\
		 	SSASendTrap.3snmp	\
		 	SSAStringInit.3snmp	\
		 	SSAStringToChar.3snmp	\
		 	SSAStringZero.3snmp	\
		 	SSASubagentOpen.3snmp

MAN3SNMPFILES += $(MAN3SNMPSOFILES)

SSAGetTrapPort.3snmp	:= SOSRC = man3snmp/SSAAgentIsAlive.3snmp
SSAOidCpy.3snmp		:= SOSRC = man3snmp/SSAOidCmp.3snmp
SSAOidDup.3snmp		:= SOSRC = man3snmp/SSAOidCmp.3snmp
SSAOidFree.3snmp	:= SOSRC = man3snmp/SSAOidCmp.3snmp
SSAOidInit.3snmp	:= SOSRC = man3snmp/SSAOidCmp.3snmp
SSAOidNew.3snmp		:= SOSRC = man3snmp/SSAOidCmp.3snmp
SSAOidStrToOid.3snmp	:= SOSRC = man3snmp/SSAOidCmp.3snmp
SSAOidString.3snmp	:= SOSRC = man3snmp/SSAOidCmp.3snmp
SSAOidZero.3snmp	:= SOSRC = man3snmp/SSAOidCmp.3snmp
SSARegSubagent.3snmp	:= SOSRC = man3snmp/SSAAgentIsAlive.3snmp
SSARegSubtable.3snmp	:= SOSRC = man3snmp/SSAAgentIsAlive.3snmp
SSARegSubtree.3snmp	:= SOSRC = man3snmp/SSAAgentIsAlive.3snmp
SSASendTrap.3snmp	:= SOSRC = man3snmp/SSAAgentIsAlive.3snmp
SSAStringInit.3snmp	:= SOSRC = man3snmp/SSAStringCpy.3snmp
SSAStringToChar.3snmp	:= SOSRC = man3snmp/SSAStringCpy.3snmp
SSAStringZero.3snmp	:= SOSRC = man3snmp/SSAStringCpy.3snmp
SSASubagentOpen.3snmp	:= SOSRC = man3snmp/SSAAgentIsAlive.3snmp

.KEEP_STATE:

include ../Makefile.man
