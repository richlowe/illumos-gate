/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 * Copyright (c) 2018, Joyent, Inc.
 */
/*
 * Copyright (c) 2009-2010, Intel Corporation.
 * All rights reserved.
 * Copyright 2019 Western Digital Corporation.
 * Copyright 2026 Michael van der Westhuizen
 */

#ifndef _SYS_ACPICA_H
#define	_SYS_ACPICA_H

#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/acpi/acpi.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialization state of the ACPI CA subsystem
 */
#define	ACPICA_NOT_INITIALIZED	(0)
#define	ACPICA_INITIALIZED	(1)

extern int acpica_init(void);

/*
 * acpi_status property values
 */
#define	ACPI_BOOT_INIT		0x00000001
#define	ACPI_BOOT_ENABLE	0x00000002
#define	ACPI_BOOT_BOOTCONF	0x00000010

/* ACPICA subsystem has been fully initialized except SCI interrupt. */
#define	ACPI_FEATURE_FULL_INIT	0x1
/* ACPI SCI interrupt has been enabled. */
#define	ACPI_FEATURE_SCI_EVENT	0x2
/* ACPI device configuration has been enabled. */
#define	ACPI_FEATURE_DEVCFG	0x4
/* ACPI _OSI method should report support of ACPI Module Device. */
#define	ACPI_FEATURE_OSI_MODULE	0x8

/* ACPI device configuration features. */
#define	ACPI_DEVCFG_CPU		0x1
#define	ACPI_DEVCFG_MEMORY	0x2
#define	ACPI_DEVCFG_CONTAINER	0x4
#define	ACPI_DEVCFG_PCI		0x8

/*
 * Function prototypes
 */
extern ACPI_STATUS acpica_eval_int(ACPI_HANDLE, char *, int *);
extern void acpica_ddi_save_resources(dev_info_t *);
extern void acpica_ddi_restore_resources(dev_info_t *);
extern void acpica_write_cpupm_capabilities(boolean_t, boolean_t);

extern ACPI_STATUS acpica_tag_devinfo(dev_info_t *, ACPI_HANDLE);
extern ACPI_STATUS acpica_untag_devinfo(dev_info_t *, ACPI_HANDLE);
extern ACPI_STATUS acpica_get_devinfo(ACPI_HANDLE, dev_info_t **);
extern ACPI_STATUS acpica_get_handle(dev_info_t *, ACPI_HANDLE *);

extern void acpica_pci_cfgspace_init(void);
extern void acpica_pci_cfgspace_register(dev_info_t *);
extern ACPI_STATUS acpica_add_processor_to_map(UINT32, ACPI_HANDLE, UINT64);
extern ACPI_STATUS acpica_remove_processor_from_map(UINT32);
extern ACPI_STATUS acpica_map_cpu(processorid_t, UINT32);
extern ACPI_STATUS acpica_unmap_cpu(processorid_t);
extern ACPI_STATUS acpica_get_cpu_object_by_cpuid(processorid_t, ACPI_HANDLE *);
extern ACPI_STATUS acpica_get_cpu_object_by_mpidr(UINT64, ACPI_HANDLE *);
extern ACPI_STATUS acpica_get_cpu_id_by_object(ACPI_HANDLE, processorid_t *);
extern ACPI_STATUS acpica_get_procid_by_object(ACPI_HANDLE, UINT32 *);
extern ACPI_STATUS acpica_get_busno(ACPI_HANDLE, int *);

extern uint64_t acpica_get_core_feature(uint64_t);
extern void acpica_set_core_feature(uint64_t);
extern void acpica_clear_core_feature(uint64_t);
extern uint64_t acpica_get_devcfg_feature(uint64_t);
extern void acpica_set_devcfg_feature(uint64_t);
extern void acpica_clear_devcfg_feature(uint64_t);

extern uint64_t acpica_get_plat_osc(uint64_t);
extern void acpica_set_plat_osc(uint64_t);
extern void acpica_clear_plat_osc(uint64_t);

/*
 * Platform-wide \_SB._OSC Support Field (DWORD 2) bit definitions.
 * ACPI 6.6, Table 6.13.
 */
#define	PLAT_OSC_PROC_AGG	0x00000001	/* Processor Aggr Device */
#define	PLAT_OSC_PPC_OST	0x00000002	/* _PPC _OST Processing */
#define	PLAT_OSC_PR3		0x00000004	/* _PR3 (D3hot/D3) Support */
#define	PLAT_OSC_EJECT_OST	0x00000008	/* Insertion/Ejection _OST */
#define	PLAT_OSC_APEI		0x00000010	/* APEI Support */
#define	PLAT_OSC_CPPC		0x00000020	/* CPPC Support */
#define	PLAT_OSC_CPPC2		0x00000040	/* CPPC Rev 2 (Autonomous) */
#define	PLAT_OSC_PLAT_LPI	0x00000080	/* Platform Coordinated LPI */
#define	PLAT_OSC_OS_LPI		0x00000100	/* OS Initiated LPI */
#define	PLAT_OSC_TFP		0x00000200	/* Fast Thermal Sampling */
#define	PLAT_OSC_GT16_PSTATE	0x00000400	/* >16 P-states */
#define	PLAT_OSC_GED		0x00000800	/* Generic Event Device */
#define	PLAT_OSC_CPPC_DIVERSE	0x00001000	/* Diverse CPPC Highest Perf */
#define	PLAT_OSC_INTR_RSRC	0x00002000	/* Interrupt ResourceSource */
#define	PLAT_OSC_CPPC_FLEX_AS	0x00004000	/* Flexible CPPC Addr Spaces */
#define	PLAT_OSC_GHES_ASSIST	0x00008000	/* GHES_ASSIST */
#define	PLAT_OSC_CPPC_MULTI_PCC	0x00010000	/* Multi PCC for CPPC */
#define	PLAT_OSC_GEN_INIT	0x00020000	/* Generic Initiator (SRAT) */
#define	PLAT_OSC_USB4		0x00040000	/* Native USB4 */
#define	PLAT_OSC_BATT_LIMIT	0x00080000	/* Battery Charge Limiting */
#define	PLAT_OSC_BAR_GAS	0x00100000	/* PCI BAR Target GAS */
#define	PLAT_OSC_PRM		0x00200000	/* Platform Runtime Mechanism */
#define	PLAT_OSC_FFH		0x00400000	/* FFH Operation Regions */
#define	PLAT_OSC_DYN_GPE	0x00800000	/* Dynamic GPE Cap */
#define	PLAT_OSC_RSRC_USAGE	0x01000000	/* Honor ResourceUsage */

extern ACPI_STATUS acpica_get_object_status(ACPI_HANDLE, int *);

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_ACPICA_H */
