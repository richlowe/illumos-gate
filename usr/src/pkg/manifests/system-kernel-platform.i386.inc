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
# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright 2012 Nexenta Systems, Inc. All rights reserved.
# Copyright 2014 Gary Mills
# Copyright 2019 Joyent, Inc.
# Copyright 2020 Peter Tribble.
# Copyright 2020 OmniOS Community Edition (OmniOSce) Association.
#

dir  path=platform/i86pc group=sys
dir  path=platform/i86pc/$(ARCH64) group=sys
dir  path=platform/i86pc/kernel group=sys
dir  path=platform/i86pc/kernel/$(ARCH64) group=sys
file path=platform/i86pc/kernel/$(ARCH64)/unix group=sys mode=0755 \
    pkg.depend.bypass-generate=dtracestubs
dir  path=platform/i86pc/kernel/cpu group=sys
dir  path=platform/i86pc/kernel/cpu/$(ARCH64) group=sys
file path=platform/i86pc/kernel/cpu/$(ARCH64)/cpu.generic group=sys mode=0755
file path=platform/i86pc/kernel/cpu/$(ARCH64)/cpu_ms.AuthenticAMD group=sys \
    mode=0755
file path=platform/i86pc/kernel/cpu/$(ARCH64)/cpu_ms.AuthenticAMD.15 group=sys \
    mode=0755
file path=platform/i86pc/kernel/cpu/$(ARCH64)/cpu_ms.GenuineIntel group=sys \
    mode=0755
dir  path=platform/i86pc/kernel/dacf group=sys
dir  path=platform/i86pc/kernel/dacf/$(ARCH64) group=sys
file path=platform/i86pc/kernel/dacf/$(ARCH64)/consconfig_dacf group=sys \
    mode=0755
dir  path=platform/i86pc/kernel/drv group=sys
dir  path=platform/i86pc/kernel/drv/$(ARCH64) group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/acpinex group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/acpippm group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/amd_iommu group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/cpudrv group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/isa group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/npe group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/pci group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/pit_beep group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/ppm group=sys
file path=platform/i86pc/kernel/drv/$(ARCH64)/rootnex group=sys
file path=platform/i86pc/kernel/drv/acpippm.conf group=sys
file path=platform/i86pc/kernel/drv/amd_iommu.conf group=sys
file path=platform/i86pc/kernel/drv/pit_beep.conf group=sys
file path=platform/i86pc/kernel/drv/ppm.conf group=sys
file path=platform/i86pc/kernel/drv/rootnex.conf group=sys
dir  path=platform/i86pc/kernel/mach group=sys
dir  path=platform/i86pc/kernel/mach/$(ARCH64) group=sys
file path=platform/i86pc/kernel/mach/$(ARCH64)/apix group=sys mode=0755
file path=platform/i86pc/kernel/mach/$(ARCH64)/pcplusmp group=sys mode=0755
file path=platform/i86pc/kernel/mach/$(ARCH64)/uppc group=sys mode=0755
dir  path=platform/i86pc/kernel/misc group=sys
dir  path=platform/i86pc/kernel/misc/$(ARCH64) group=sys
file path=platform/i86pc/kernel/misc/$(ARCH64)/acpidev group=sys mode=0755
file path=platform/i86pc/kernel/misc/$(ARCH64)/gfx_private group=sys mode=0755
file path=platform/i86pc/kernel/misc/$(ARCH64)/pci_prd group=sys mode=0755
file path=platform/i86pc/kernel/misc/$(ARCH64)/pcie group=sys mode=0755
dir  path=platform/i86pc/ucode group=sys
dir  path=platform/i86xpv group=sys
dir  path=platform/i86xpv/kernel group=sys
dir  path=platform/i86xpv/kernel/$(ARCH64) group=sys
file path=platform/i86xpv/kernel/$(ARCH64)/unix group=sys mode=0755 \
    pkg.depend.bypass-generate=dtracestubs
dir  path=platform/i86xpv/kernel/cpu group=sys
dir  path=platform/i86xpv/kernel/cpu/$(ARCH64) group=sys
file path=platform/i86xpv/kernel/cpu/$(ARCH64)/cpu.generic group=sys mode=0755
file path=platform/i86xpv/kernel/cpu/$(ARCH64)/cpu_ms.AuthenticAMD group=sys \
    mode=0755
file path=platform/i86xpv/kernel/cpu/$(ARCH64)/cpu_ms.AuthenticAMD.15 \
    group=sys mode=0755
file path=platform/i86xpv/kernel/cpu/$(ARCH64)/cpu_ms.GenuineIntel group=sys \
    mode=0755
dir  path=platform/i86xpv/kernel/dacf group=sys
dir  path=platform/i86xpv/kernel/dacf/$(ARCH64) group=sys
file path=platform/i86xpv/kernel/dacf/$(ARCH64)/consconfig_dacf group=sys \
    mode=0755
dir  path=platform/i86xpv/kernel/drv group=sys
dir  path=platform/i86xpv/kernel/drv/$(ARCH64) group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/balloon group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/domcaps group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/evtchn group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/isa group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/npe group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/pci group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/pit_beep group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/privcmd group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/rootnex group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xdb group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xdf group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xenbus group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xencons group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xnbe group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xnbo group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xnbu group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xnf group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xpvd group=sys
file path=platform/i86xpv/kernel/drv/$(ARCH64)/xpvtap group=sys
file path=platform/i86xpv/kernel/drv/pit_beep.conf group=sys
file path=platform/i86xpv/kernel/drv/xencons.conf group=sys
dir  path=platform/i86xpv/kernel/mach group=sys
dir  path=platform/i86xpv/kernel/mach/$(ARCH64) group=sys
file path=platform/i86xpv/kernel/mach/$(ARCH64)/xpv_psm group=sys mode=0755
file path=platform/i86xpv/kernel/mach/$(ARCH64)/xpv_uppc group=sys mode=0755
dir  path=platform/i86xpv/kernel/misc group=sys
dir  path=platform/i86xpv/kernel/misc/$(ARCH64) group=sys
file path=platform/i86xpv/kernel/misc/$(ARCH64)/gfx_private group=sys mode=0755
file path=platform/i86xpv/kernel/misc/$(ARCH64)/pci_prd group=sys mode=0755
file path=platform/i86xpv/kernel/misc/$(ARCH64)/pcie group=sys mode=0755
file path=platform/i86xpv/kernel/misc/$(ARCH64)/xnb group=sys mode=0755
file path=platform/i86xpv/kernel/misc/$(ARCH64)/xpv_autoconfig group=sys \
    mode=0755
dir  path=platform/i86xpv/kernel/tod group=sys
dir  path=platform/i86xpv/kernel/tod/$(ARCH64) group=sys
file path=platform/i86xpv/kernel/tod/$(ARCH64)/xpvtod group=sys mode=0755
file path=usr/share/man/man4d/npe.4d
link path=usr/share/man/man5/isa.5 target=sysbus.5
file path=usr/share/man/man5/sysbus.5
driver name=acpinex alias=acpivirtnex
driver name=acpippm
driver name=amd_iommu perms="* 0644 root sys" \
    alias=pci1002,5a23 \
    alias=pci1022,11ff
driver name=balloon perms="* 0444 root sys"
driver name=cpudrv alias=cpu
driver name=domcaps perms="* 0444 root sys"
driver name=evtchn perms="* 0666 root sys"
driver name=isa class=sysbus alias=pciclass,060100
driver name=npe alias=pciex_root_complex
driver name=pci class=pci
driver name=pit_beep alias=SUNW,pit_beep
driver name=privcmd perms="* 0666 root sys"
driver name=rootnex
driver name=xdb
driver name=xdf
driver name=xenbus perms="* 0666 root sys"
driver name=xencons
driver name=xnbe alias=xnb,ioemu
driver name=xnbo \
    alias=xnb \
    alias=xnb,SUNW_mac
driver name=xnbu alias=xnb,netfront
driver name=xnf
driver name=xpvd
driver name=xpvtap perms="* 0666 root sys"
legacy pkg=SUNWcakr.i arch=$(ARCH).i86pc \
    desc="core kernel software for a specific hardware platform group" \
    name="Core Solaris Kernel Architecture (Root)"
legacy pkg=SUNWcakrx.i arch=$(ARCH).i86pc \
    desc="core kernel software for the i86xpv virtual hardware platform" \
    name="Core Kernel Architecture i86xpv, (Root)"
license usr/src/uts/intel/THIRDPARTYLICENSE \
    license=usr/src/uts/intel/THIRDPARTYLICENSE
depend type=require fmri=system/microcode/amd
depend type=require fmri=system/microcode/intel
