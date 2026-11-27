/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2026 Michael van der Westhuizen
 */

/*
 * Collaborative Processor Performance Control (CPPC) driver.
 *
 * Implements ACPI CPPC (ACPI 6.5 Section 8.4.7) for aarch64 platforms.
 * CPPC defines an abstract performance scale that the OS uses to request and
 * monitor CPU performance levels.  This driver evaluates the per-CPU _CPC ACPI
 * objects, parses the register descriptors, and provides a frequency-based
 * interface to the cpudrv_mach layer.
 *
 * Performance Domain Coordination
 *
 * The driver constructs performance domains from _PSD objects.  Under hardware
 * coordination, each CPU is placed into its own single-member domain and the
 * platform firmware handles cross-CPU coordination transparently.  Under
 * software coordination, CPUs that share a _PSD Domain ID are grouped into
 * multi-member domains.  cppc_set_speed() aggregates desired performance levels
 * across the domain (max policy) and writes the domain target to the
 * appropriate registers.
 *
 * Software coordination domains are constructed and operational but have not
 * been validated on hardware.
 */

/*
 * Synthetic Speed Table Generation
 *
 * CPPC operates in abstract performance levels, not frequencies.  The cpudrv
 * framework, however, requires speed tables expressed in MHz.  We synthesize
 * a frequency table from the CPPC performance range using the following
 * approach:
 *
 * 1. Anchor points:  We select up to four anchor performance levels from
 *    the _CPC data:
 *    - Highest Performance   (maximum achievable, may be above nominal)
 *    - Nominal Performance   (sustained guaranteed performance)
 *    - Lowest Nonlinear Perf (below this, perf/power is nonlinear)
 *    - Lowest Performance    (minimum the platform supports)
 *
 *    Adjacent duplicates are collapsed (e.g., if highest == nominal, only
 *    one anchor is kept).
 *
 * 2. Interpolation:  Between each pair of adjacent anchors, if the gap exceeds
 *    2 performance levels, one midpoint is inserted.  This provides finer
 *    granularity in the interesting transition regions without generating an
 *    unwieldy number of speeds.
 *
 * 3. Frequency conversion:  Each performance level is converted to MHz using
 *    the scaling factor from _CPC entries 22 (NominalFreq) and 3
 *    (NominalPerformance):
 *
 *      freq_mhz = (perf_level * nominal_freq_mhz) / nominal_perf
 *
 *    This linear scaling is specified by the CPPC architecture.
 *
 * 4. The resulting table is sorted in descending order (highest frequency
 *    first) and deduplicated.  Typical result: 4-7 speeds.
 *
 * When setting a speed, the MHz value is converted back to a performance level
 * using the inverse of the above formula, clamped to the [Lowest, Highest]
 * range, and written to the Desired Performance register.
 */

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/smp_impldefs.h>
#include <sys/acpipcc.h>
#include <sys/acpicppc.h>
#include <sys/acpi/acpi.h>
#include <sys/acpi/accommon.h>
#include <sys/acpica.h>
#include <sys/controlregs.h>

/*
 * ACPI address space IDs relevant to CPPC.
 */
#define	CPPC_SPACE_SYSMEM	0x00
#define	CPPC_SPACE_PCC		0x0A
#define	CPPC_SPACE_FFH		0x7F

/*
 * FFH address values for AMU counters (DEN0048D).
 */
#define	FFH_AMU_DELIVERED	0x0	/* AMEVCNTR0_EL0[0] - core freq */
#define	FFH_AMU_REFERENCE	0x1	/* AMEVCNTR0_EL0[1] - const freq */

/*
 * CPPC-over-PCC command codes (ACPI 6.6).  The CPPC PCC interface
 * defines command 0 as read and command 1 as write.
 */
#define	CPPC_PCC_CMD_READ	0
#define	CPPC_PCC_CMD_WRITE	1

/*
 * _CPC package entry indices (ACPI 6.5, Table 8.14).
 */
#define	CPC_NUM_ENTRIES		0
#define	CPC_REVISION		1
#define	CPC_HIGHEST_PERF	2
#define	CPC_NOMINAL_PERF	3
#define	CPC_LOWEST_NL_PERF	4
#define	CPC_LOWEST_PERF		5
#define	CPC_GUARANTEED_REG	6
#define	CPC_DESIRED_REG		7
#define	CPC_MIN_PERF_REG	8
#define	CPC_MAX_PERF_REG	9
#define	CPC_PERF_REDUCE_TOL	10
#define	CPC_TIME_WINDOW		11
#define	CPC_CTR_WRAP_TIME	12
#define	CPC_REF_CTR_REG		13
#define	CPC_DELIVERED_CTR_REG	14
#define	CPC_PERF_LIMITED_REG	15
#define	CPC_ENABLE_REG		16
#define	CPC_AUTO_SEL_ENABLE	17
#define	CPC_AUTO_ACT_WINDOW	18
#define	CPC_EPP_REG		19
#define	CPC_REF_PERF		20
#define	CPC_LOWEST_FREQ		21
#define	CPC_NOMINAL_FREQ	22
#define	CPC_NENTRIES_REV3	23
#define	CPC_NENTRIES_REV4	25

/*
 * _CPC revisions with known package layouts.  The layout is fixed per revision
 * and new revisions only append entries (ACPI 6.6, 8.4.6.1, Table 8.23):
 * revision 3 carries 23 entries (0-22), revision 4 appends
 * OSPMNominalPerformanceRegister (23) and ResourcePriorityRegisters (24)
 * for 25.
 */
#define	CPC_REV3		3
#define	CPC_REV4		4

/*
 * _PSD coordination types.
 */
#define	PSD_COORD_SW_ALL	0xFC
#define	PSD_COORD_SW_ANY	0xFE
#define	PSD_COORD_HW_ALL	0xFD

/*
 * _PSD package element indices.
 */
#define	PSD_NUM_ENTRIES		0
#define	PSD_REVISION		1
#define	PSD_DOMAIN		2
#define	PSD_COORD_TYPE		3
#define	PSD_NUM_PROCS		4
#define	PSD_PKG_COUNT		5

/*
 * Resource template header for extracting a GAS from an ACPI buffer.
 *
 * A Register() resource template consists of a 3-byte descriptor header
 * followed by the 12-byte ACPI_GENERIC_ADDRESS structure. The first byte of
 * the header is the descriptor type, which must be a Generic Register
 * descriptor (ACPI 6.6 §6.4.3.7).
 */
#define	GAS_RT_HDR_LEN		3
#define	GAS_RT_MIN_LEN		(GAS_RT_HDR_LEN + sizeof (ACPI_GENERIC_ADDRESS))

/*
 * Maximum number of synthetic speeds we generate.
 *
 * 4 anchors + up to 3 midpoints = 7, plus a small margin.
 */
#define	CPPC_MAX_SPEEDS		10

/*
 * AMU sampling parameters for cppc_get_speed.
 */
#define	CPPC_AMU_RETRIES	10
#define	CPPC_AMU_DELAY_USEC	20
#define	CPPC_AMU_STABLE_PERF	2	/* stable within 2 perf levels */

/*
 * Per-CPU CPPC register descriptor.
 */
typedef struct cppc_reg {
	uint8_t		cr_spaceid;	/* ACPI address space ID */
	uint8_t		cr_width;	/* register bit width */
	uint8_t		cr_access;	/* access size (PCC subspace ID) */
	uint64_t	cr_addr;	/* address or offset */
	boolean_t	cr_valid;	/* register is usable */
	caddr_t		cr_va;		/* mapped VA (SystemMemory only) */
} cppc_reg_t;

/*
 * Per-CPU CPPC state.
 *
 * Allocated by cppc_cpu_init() and hung off struct machcpu via the mcpu_cppc
 * pointer.
 */
typedef struct cppc_cpu {
	boolean_t	cc_valid;	/* _CPC parsed, DVFS available */

	/* Performance levels (abstract scale) */
	uint32_t	cc_highest;	/* highest perf level */
	uint32_t	cc_nominal;	/* nominal (sustained) perf level */
	uint32_t	cc_lowest_nl;	/* lowest nonlinear perf level */
	uint32_t	cc_lowest;	/* lowest perf level */
	uint32_t	cc_guaranteed;	/* guaranteed perf (0 = unsupported) */

	/* Stashed register descriptors for deferred level reads */
	cppc_reg_t	cc_highest_reg;
	cppc_reg_t	cc_nominal_reg;
	cppc_reg_t	cc_lowest_nl_reg;
	cppc_reg_t	cc_lowest_reg;
	cppc_reg_t	cc_guaranteed_reg;
	cppc_reg_t	cc_ref_perf_reg;

	/* Frequency info from _CPC tail entries */
	uint32_t	cc_nominal_freq; /* nominal frequency in MHz */
	uint32_t	cc_lowest_freq;	/* lowest frequency in MHz */
	uint32_t	cc_ref_perf;	/* reference perf (or cc_nominal) */
	cppc_reg_t	cc_nominal_freq_reg;
	cppc_reg_t	cc_lowest_freq_reg;

	/* Key register descriptors */
	cppc_reg_t	cc_desired;	/* desired perf register */
	cppc_reg_t	cc_reference;	/* reference perf counter */
	cppc_reg_t	cc_delivered;	/* delivered perf counter */
	cppc_reg_t	cc_enable;	/* CPPC enable register */

	/* PCC channel (if any register uses PCC transport) */
	pcc_chan_t	*cc_pcc_chan;	/* PCC channel handle (NULL if none) */
	uint_t		cc_pcc_chan_id;	/* PCC subspace ID of cc_pcc_chan */
	boolean_t	cc_uses_pcc;	/* at least one register is PCC */
	boolean_t	cc_pcc_mismatch;	/* _CPC uses >1 PCC subspace */

	/* Domain coordination */
	struct cppc_domain *cc_domain;	/* back-pointer to domain */
	uint_t		cc_dom_idx;	/* our slot in domain member list */
	uint32_t	cc_desired_perf; /* this CPU's individual request */

	/* Last requested speed (MHz) for get_speed fallback */
	uint32_t	cc_last_speed;
} cppc_cpu_t;

/*
 * Performance coordination domain.
 *
 * Groups CPUs that share a hardware performance control relationship as
 * described by the _PSD ACPI object.  Under HW_ALL coordination, each CPU is
 * its own single-member domain (the aggregation and write path collapse to
 * identity).
 */
typedef struct cppc_domain {
	uint32_t	cd_id;		/* _PSD Domain ID */
	uint_t		cd_coord;	/* PSD_COORD_HW_ALL/SW_ALL/SW_ANY */
	kmutex_t	cd_lock;
	uint_t		cd_ncpus;	/* members joined so far */
	uint_t		cd_nexpected;	/* from _PSD NumProcessors */
	processorid_t	*cd_cpuids;	/* member CPU ids */
	cppc_cpu_t	**cd_members;	/* member cppc state pointers */
	uint32_t	cd_target;	/* current domain-wide perf target */
	uint_t		cd_pcc_chan_id;	/* PCC subspace ID of this domain */
	boolean_t	cd_pcc_chan_set; /* cd_pcc_chan_id is valid */
} cppc_domain_t;

/*
 * Module-level state.
 */
static cppc_domain_t	*cppc_domains;
static uint_t		cppc_ndomains;
static kmutex_t		cppc_domain_lock;

/*
 * Acquire the PCC channel for the given firmware subspace ID.
 *
 * CPPC mandates (ACPI 6.6 §8.4.6.1.9, "Using PCC Registers") that a single
 * PCC subspace is used for all processors in a performance domain (as defined
 * by _PSD).  When _PSD is not used, then all registers in a _CPC object must
 * use a single PCC subspace.
 *
 * The way we enforce this is to track violations in this function, setting
 * `cc_pcc_mismatch' to leave a breadcrumb that is later checked by the
 * `cppc_cpu_init' function.  The performance domain rule is checked on
 * joining a performance domain.
 */
static void
cppc_set_pcc_chan(cppc_cpu_t *cc, uint_t chan_id)
{
	pcc_chan_t *pc;

	ASSERT3P(cc, !=, NULL);

	if (cc->cc_pcc_chan != NULL) {
		if (cc->cc_pcc_chan_id != chan_id) {
			cc->cc_pcc_mismatch = B_TRUE;
		}

		return;
	}

	pc = pcc_chan_get(chan_id);
	if (pc == NULL) {
		cmn_err(CE_WARN, "!cppc: cannot get PCC channel %u", chan_id);
		return;
	}

	cc->cc_pcc_chan = pc;
	cc->cc_pcc_chan_id = chan_id;
}

/*
 * Free a cppc_cpu_t, releasing its PCC channel handle if held.
 */
static void
cppc_cpu_free(cppc_cpu_t *cc)
{
	ASSERT3P(cc, !=, NULL);

	if (cc->cc_pcc_chan != NULL) {
		pcc_chan_put(cc->cc_pcc_chan);
		cc->cc_pcc_chan = NULL;
	}

	kmem_free(cc, sizeof (cppc_cpu_t));
}

/*
 * Parse a _CPC entry that can be either an integer or a register (buffer
 * containing a resource template with a GAS).
 *
 * If the entry is an integer, *intval is set and *reg is marked invalid
 * (cr_valid = B_FALSE).  If it is a register, *reg is populated and *intval
 * is set to 0.
 *
 * Returns B_TRUE on success, B_FALSE on parse error.
 */
static boolean_t
cppc_parse_entry(ACPI_OBJECT *obj, cppc_reg_t *reg, uint32_t *intval)
{
	ACPI_GENERIC_ADDRESS *gas;

	ASSERT3P(obj, !=, NULL);
	ASSERT3P(reg, !=, NULL);
	ASSERT3P(intval, !=, NULL);

	memset(reg, 0, sizeof (*reg));
	*intval = 0;

	if (obj->Type == ACPI_TYPE_INTEGER) {
		*intval = (uint32_t)obj->Integer.Value;
		reg->cr_valid = B_FALSE;
		return (B_TRUE);
	}

	if (obj->Type != ACPI_TYPE_BUFFER) {
		return (B_FALSE);
	}

	if (obj->Buffer.Length < GAS_RT_MIN_LEN) {
		return (B_FALSE);
	}

	if (obj->Buffer.Pointer[0] != ACPI_RESOURCE_NAME_GENERIC_REGISTER) {
		return (B_FALSE);
	}

	gas = (ACPI_GENERIC_ADDRESS *)(obj->Buffer.Pointer + GAS_RT_HDR_LEN);

	reg->cr_spaceid = gas->SpaceId;
	reg->cr_width = gas->BitWidth;
	reg->cr_access = gas->AccessWidth;
	reg->cr_addr = gas->Address;

	/*
	 * A register with bit width 0 indicates an unimplemented field.
	 */
	if (gas->BitWidth == 0) {
		reg->cr_valid = B_FALSE;
	} else {
		reg->cr_valid = B_TRUE;
	}

	return (B_TRUE);
}

/*
 * Byte length of a CPPC register, from its bit width.
 */
static size_t
cppc_reg_len(cppc_reg_t *reg)
{
	return ((reg->cr_width + 7) / 8);
}

/*
 * Read a 64-bit value from a CPPC register.  Used for performance counters,
 * which are 64-bit on FFH (AMU) and may be 64-bit on PCC.
 *
 * Narrower PCC and SystemMemory registers are read at their natural width and
 * zero-extended.  Widths other than 32 and 64 bits are rejected.
 */
static int
cppc_reg_read(cppc_cpu_t *cc, cppc_reg_t *reg, uint64_t *val)
{
	uint32_t v32;

	ASSERT3P(cc, !=, NULL);
	ASSERT3P(reg, !=, NULL);
	ASSERT3P(val, !=, NULL);

	if (!reg->cr_valid) {
		return (DDI_FAILURE);
	}

	switch (reg->cr_spaceid) {
	case CPPC_SPACE_FFH:
		if (reg->cr_addr == FFH_AMU_DELIVERED) {
			*val = read_amevcntr00();
		} else if (reg->cr_addr == FFH_AMU_REFERENCE) {
			*val = read_amevcntr01();
		} else {
			return (DDI_FAILURE);
		}

		return (DDI_SUCCESS);

	case CPPC_SPACE_PCC:
		if (reg->cr_width == 64) {
			return (pcc_chan_read64(cc->cc_pcc_chan,
			    (uint32_t)reg->cr_addr, val));
		} else if (reg->cr_width == 32) {
			if (pcc_chan_read32(cc->cc_pcc_chan,
			    (uint32_t)reg->cr_addr, &v32) != DDI_SUCCESS) {
				return (DDI_FAILURE);
			}
			*val = v32;
			return (DDI_SUCCESS);
		}

		return (DDI_FAILURE);

	case CPPC_SPACE_SYSMEM:
		if (reg->cr_va == NULL) {
			return (DDI_FAILURE);
		}

		if (reg->cr_width == 64) {
			*val = *(volatile uint64_t *)(void *)reg->cr_va;
		} else if (reg->cr_width == 32) {
			*val = *(volatile uint32_t *)(void *)reg->cr_va;
		} else {
			return (DDI_FAILURE);
		}

		return (DDI_SUCCESS);

	default:
		cmn_err(CE_WARN, "!cppc: unsupported read space ID 0x%x "
		    "for CPU %d", reg->cr_spaceid, CPU->cpu_id);
		return (DDI_FAILURE);
	}
}

/*
 * Write a value to a CPPC register, at the register's natural width.
 *
 * Only 32-bit and 64-bit registers are supported.
 */
static int
cppc_reg_write(cppc_cpu_t *cc, cppc_reg_t *reg, uint64_t val)
{
	ASSERT3P(cc, !=, NULL);
	ASSERT3P(reg, !=, NULL);

	if (!reg->cr_valid) {
		return (DDI_FAILURE);
	}

	switch (reg->cr_spaceid) {
	case CPPC_SPACE_PCC:
		if (reg->cr_width == 64) {
			return (pcc_chan_write64(cc->cc_pcc_chan,
			    (uint32_t)reg->cr_addr, val));
		} else if (reg->cr_width == 32) {
			return (pcc_chan_write32(cc->cc_pcc_chan,
			    (uint32_t)reg->cr_addr, (uint32_t)val));
		}

		return (DDI_FAILURE);

	case CPPC_SPACE_SYSMEM:
		if (reg->cr_va == NULL) {
			return (DDI_FAILURE);
		}

		if (reg->cr_width == 64) {
			*(volatile uint64_t *)(void *)reg->cr_va = val;
		} else if (reg->cr_width == 32) {
			*(volatile uint32_t *)(void *)reg->cr_va =
			    (uint32_t)val;
		} else {
			return (DDI_FAILURE);
		}

		return (DDI_SUCCESS);

	default:
		/*
		 * FFH (Functional Fixed Hardware) writes would require an
		 * xcall to execute the MSR on the target CPU.  No known
		 * aarch64 platform uses FFH for writable CPPC registers
		 * (FFH is used for AMU counters, which are read-only).  If
		 * this fires, the SW_ALL fan-out in cppc_set_speed() needs
		 * xcall infrastructure to write on each domain member's CPU.
		 */
		cmn_err(CE_WARN, "!cppc: unsupported write space ID 0x%x "
		    "for CPU %d", reg->cr_spaceid, CPU->cpu_id);
		return (DDI_FAILURE);
	}
}

/*
 * Map a SystemMemory CPPC register.  Called during _CPC parsing for each
 * register with space ID CPPC_SPACE_SYSMEM.
 */
static void
cppc_map_sysmem_reg(cppc_reg_t *reg)
{
	size_t len;

	ASSERT3P(reg, !=, NULL);

	if (!reg->cr_valid || reg->cr_spaceid != CPPC_SPACE_SYSMEM) {
		return;
	}

	if (reg->cr_addr == 0) {
		reg->cr_valid = B_FALSE;
		return;
	}

	len = cppc_reg_len(reg);
	if (len == 0) {
		len = 4;
	}

	reg->cr_va = psm_map_phys((paddr_t)reg->cr_addr, len,
	    PROT_READ | PROT_WRITE);
	if (reg->cr_va == NULL) {
		cmn_err(CE_WARN, "!cppc: failed to map SystemMemory "
		    "register at 0x%llx",
		    (unsigned long long)reg->cr_addr);
		reg->cr_valid = B_FALSE;
	}
}

/*
 * Join (or create) a performance coordination domain for this CPU.
 *
 * Evaluates _PSD on the ACPI handle to determine the coordination
 * type and domain membership.  Under HW_ALL, each CPU gets its own
 * single-member domain regardless of _PSD grouping (the firmware
 * handles cross-CPU coordination and the domain infrastructure is
 * transparent).  Under SW_ALL or SW_ANY, CPUs sharing a _PSD Domain
 * ID are grouped into a multi-member domain.
 *
 * If _PSD is absent, hardware coordination is assumed per the ACPI
 * spec default and a single-member domain is synthesised.
 *
 * Returns DDI_SUCCESS on success, DDI_FAILURE on parse error, if
 * the domain is already full, if the domain table is full, or if
 * the CPU uses a different PCC subspace from the domain.
 */
static int
cppc_domain_join(ACPI_HANDLE hdl, processorid_t cpuid, cppc_cpu_t *cc)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *pkg;
	ACPI_OBJECT *inner;
	ACPI_OBJECT *elems;
	ACPI_STATUS status;
	uint32_t domain_id;
	uint32_t coord_type;
	uint32_t num_procs;
	cppc_domain_t *dom;
	uint_t i;

	ASSERT3P(cc, !=, NULL);

	buf.Length = ACPI_ALLOCATE_BUFFER;
	buf.Pointer = NULL;

	status = AcpiEvaluateObject(hdl, "_PSD", NULL, &buf);
	if (ACPI_FAILURE(status)) {
		/* _PSD absent */
		coord_type = PSD_COORD_HW_ALL;
		domain_id = (uint32_t)cpuid;
		num_procs = 1;
	} else {
		pkg = (ACPI_OBJECT *)buf.Pointer;
		if (pkg->Type != ACPI_TYPE_PACKAGE ||
		    pkg->Package.Count < 1) {
			AcpiOsFree(buf.Pointer);
			return (DDI_FAILURE);
		}

		inner = &pkg->Package.Elements[0];
		if (inner->Type != ACPI_TYPE_PACKAGE ||
		    inner->Package.Count < PSD_PKG_COUNT) {
			AcpiOsFree(buf.Pointer);
			return (DDI_FAILURE);
		}

		elems = inner->Package.Elements;
		if (elems[PSD_DOMAIN].Type != ACPI_TYPE_INTEGER ||
		    elems[PSD_COORD_TYPE].Type != ACPI_TYPE_INTEGER ||
		    elems[PSD_NUM_PROCS].Type != ACPI_TYPE_INTEGER) {
			AcpiOsFree(buf.Pointer);
			return (DDI_FAILURE);
		}

		domain_id = (uint32_t)elems[PSD_DOMAIN].Integer.Value;
		coord_type = (uint32_t)elems[PSD_COORD_TYPE].Integer.Value;
		num_procs = (uint32_t)elems[PSD_NUM_PROCS].Integer.Value;
		AcpiOsFree(buf.Pointer);

		if (num_procs == 0) {
			return (DDI_FAILURE);
		}

		if (num_procs > max_ncpus) {
			cmn_err(CE_NOTE, "!cppc: domain processor count %u "
			    "greater than max_ncpus (%d), clamping",
			    num_procs, max_ncpus);
			num_procs = max_ncpus;
		}
	}

	if (coord_type != PSD_COORD_HW_ALL) {
		cmn_err(CE_WARN, "cppc: CPU %d: software-coordinated "
		    "DVFS (type 0x%x) in domain %u - untested",
		    cpuid, coord_type, domain_id);
	}

	mutex_enter(&cppc_domain_lock);

	if (coord_type == PSD_COORD_HW_ALL) {
		/*
		 * Under hardware coordination, each CPU gets its own
		 * single-member domain.  The aggregation computes max
		 * across one member (identity) and writes to one CPU.
		 * This makes the domain path a no-op for HW_ALL.
		 */
		if (cppc_ndomains >= (uint_t)max_ncpus) {
			cmn_err(CE_WARN, "!cppc: CPU %d: domain table full "
			    "(%u/%d domains)", cpuid, cppc_ndomains, max_ncpus);
			mutex_exit(&cppc_domain_lock);
			return (DDI_FAILURE);
		}
		dom = &cppc_domains[cppc_ndomains];
		dom->cd_id = (uint32_t)cpuid;
		dom->cd_coord = PSD_COORD_HW_ALL;
		mutex_init(&dom->cd_lock, NULL, MUTEX_DEFAULT, NULL);
		dom->cd_nexpected = 1;
		dom->cd_cpuids = kmem_zalloc(sizeof (processorid_t),
		    KM_SLEEP);
		dom->cd_members = kmem_zalloc(sizeof (cppc_cpu_t *),
		    KM_SLEEP);
		dom->cd_cpuids[0] = cpuid;
		dom->cd_members[0] = cc;
		dom->cd_ncpus = 1;
		dom->cd_target = 0;
		cc->cc_domain = dom;
		cc->cc_dom_idx = 0;
		cppc_ndomains++;
	} else {
		/*
		 * Software coordination - find or create a
		 * multi-member domain from the _PSD Domain ID.
		 */
		dom = NULL;
		for (i = 0; i < cppc_ndomains; i++) {
			if (cppc_domains[i].cd_id == domain_id) {
				dom = &cppc_domains[i];
				break;
			}
		}

		if (dom == NULL) {
			if (cppc_ndomains >= (uint_t)max_ncpus) {
				cmn_err(CE_WARN, "!cppc: CPU %d: domain "
				    "table full (%u/%d domains)",
				    cpuid, cppc_ndomains, max_ncpus);
				mutex_exit(&cppc_domain_lock);
				return (DDI_FAILURE);
			}
			dom = &cppc_domains[cppc_ndomains];
			dom->cd_id = domain_id;
			dom->cd_coord = coord_type;
			mutex_init(&dom->cd_lock, NULL,
			    MUTEX_DEFAULT, NULL);
			dom->cd_nexpected = num_procs;
			dom->cd_cpuids = kmem_zalloc(
			    num_procs * sizeof (processorid_t),
			    KM_SLEEP);
			dom->cd_members = kmem_zalloc(
			    num_procs * sizeof (cppc_cpu_t *),
			    KM_SLEEP);
			dom->cd_ncpus = 0;
			dom->cd_target = 0;
			dom->cd_pcc_chan_id = 0;
			dom->cd_pcc_chan_set = B_FALSE;
			cppc_ndomains++;
		}

		if (dom->cd_ncpus >= dom->cd_nexpected) {
			cmn_err(CE_WARN, "!cppc: CPU %d: domain %u "
			    "full (%u/%u members)",
			    cpuid, dom->cd_id,
			    dom->cd_ncpus, dom->cd_nexpected);
			mutex_exit(&cppc_domain_lock);
			return (DDI_FAILURE);
		}

		/*
		 * ACPI 6.6 §8.4.6.1.9 requires all PCC registers for all
		 * processors in a performance domain to be in a single PCC
		 * subspace.
		 */
		if (cc->cc_uses_pcc) {
			if (!dom->cd_pcc_chan_set) {
				dom->cd_pcc_chan_id = cc->cc_pcc_chan_id;
				dom->cd_pcc_chan_set = B_TRUE;
			} else if (dom->cd_pcc_chan_id !=
			    cc->cc_pcc_chan_id) {
				cmn_err(CE_WARN, "!cppc: CPU %d: domain %u "
				    "uses PCC subspace %u, CPU uses %u",
				    cpuid, dom->cd_id, dom->cd_pcc_chan_id,
				    cc->cc_pcc_chan_id);
				mutex_exit(&cppc_domain_lock);
				return (DDI_FAILURE);
			}
		}

		cc->cc_dom_idx = dom->cd_ncpus;
		dom->cd_cpuids[dom->cd_ncpus] = cpuid;
		dom->cd_members[dom->cd_ncpus] = cc;
		dom->cd_ncpus++;
		cc->cc_domain = dom;
	}

	mutex_exit(&cppc_domain_lock);
	return (DDI_SUCCESS);
}

/*
 * Evaluate _CPC on the given ACPI handle and populate the per-CPU CPPC state.
 */
static int
cppc_parse_cpc(ACPI_HANDLE hdl, cppc_cpu_t *cc, processorid_t cpuid)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *pkg;
	ACPI_OBJECT *elems;
	ACPI_STATUS status;
	cppc_reg_t reg;
	uint32_t intval;
	uint32_t num_entries;
	uint32_t revision;
	int i;

	ASSERT3P(hdl, !=, NULL);
	ASSERT3P(cc, !=, NULL);

	buf.Length = ACPI_ALLOCATE_BUFFER;
	buf.Pointer = NULL;

	status = AcpiEvaluateObject(hdl, "_CPC", NULL, &buf);
	if (ACPI_FAILURE(status)) {
		return (DDI_FAILURE);
	}

	pkg = (ACPI_OBJECT *)buf.Pointer;
	if (pkg->Type != ACPI_TYPE_PACKAGE ||
	    pkg->Package.Count < CPC_NENTRIES_REV3) {
		AcpiOsFree(buf.Pointer);
		return (DDI_FAILURE);
	}

	elems = pkg->Package.Elements;

	/*
	 * Entries 0-1: NumEntries and Revision.  Accept exactly the known
	 * (revision, entry count) pairs: (3, 23) and (4, 25).  This will need
	 * rework as new CPPC revisions are implemented and validated.
	 */
	if (elems[CPC_NUM_ENTRIES].Type != ACPI_TYPE_INTEGER ||
	    elems[CPC_REVISION].Type != ACPI_TYPE_INTEGER) {
		AcpiOsFree(buf.Pointer);
		return (DDI_FAILURE);
	}
	num_entries = (uint32_t)elems[CPC_NUM_ENTRIES].Integer.Value;
	revision = (uint32_t)elems[CPC_REVISION].Integer.Value;

	if (!((revision == CPC_REV3 &&
	    num_entries == CPC_NENTRIES_REV3) ||
	    (revision == CPC_REV4 && num_entries == CPC_NENTRIES_REV4))) {
		cmn_err(CE_WARN, "!cppc: CPU %d: unsupported _CPC revision "
		    "%u with %u entries", cpuid, revision, num_entries);
		AcpiOsFree(buf.Pointer);
		return (DDI_FAILURE);
	}
	if (pkg->Package.Count < num_entries) {
		cmn_err(CE_WARN, "!cppc: CPU %d: _CPC declares %u entries "
		    "but package holds only %u", cpuid, num_entries,
		    pkg->Package.Count);
		AcpiOsFree(buf.Pointer);
		return (DDI_FAILURE);
	}

	/*
	 * Parse performance level entries (2-5).
	 * These can be integers or registers.  If a register, we need
	 * to read it via the appropriate transport after setup.
	 */
	for (i = CPC_HIGHEST_PERF; i <= CPC_LOWEST_PERF; i++) {
		if (!cppc_parse_entry(&elems[i], &reg, &intval)) {
			cmn_err(CE_WARN, "!cppc: CPU %d: failed to parse "
			    "_CPC entry %d", cpuid, i);
			AcpiOsFree(buf.Pointer);
			return (DDI_FAILURE);
		}

		/*
		 * For PCC registers, extract the channel ID from the
		 * access size field of the GAS.
		 */
		if (reg.cr_valid && reg.cr_spaceid == CPPC_SPACE_PCC) {
			cc->cc_uses_pcc = B_TRUE;
			cppc_set_pcc_chan(cc, (uint_t)reg.cr_access);
		}

		switch (i) {
		case CPC_HIGHEST_PERF:
			cc->cc_highest_reg = reg;
			if (!reg.cr_valid) {
				cc->cc_highest = intval;
			}
			break;
		case CPC_NOMINAL_PERF:
			cc->cc_nominal_reg = reg;
			if (!reg.cr_valid) {
				cc->cc_nominal = intval;
			}
			break;
		case CPC_LOWEST_NL_PERF:
			cc->cc_lowest_nl_reg = reg;
			if (!reg.cr_valid) {
				cc->cc_lowest_nl = intval;
			}
			break;
		case CPC_LOWEST_PERF:
			cc->cc_lowest_reg = reg;
			if (!reg.cr_valid) {
				cc->cc_lowest = intval;
			}
			break;
		}
	}

	/* Entry 6: GuaranteedPerformanceRegister */
	if (cppc_parse_entry(&elems[CPC_GUARANTEED_REG],
	    &cc->cc_guaranteed_reg, &intval)) {
		cc->cc_guaranteed = intval;
		if (cc->cc_guaranteed_reg.cr_valid &&
		    cc->cc_guaranteed_reg.cr_spaceid == CPPC_SPACE_PCC) {
			cc->cc_uses_pcc = B_TRUE;
			cppc_set_pcc_chan(cc,
			    (uint_t)cc->cc_guaranteed_reg.cr_access);
		}
	}

	/* Entry 7: DesiredPerformanceRegister */
	if (!cppc_parse_entry(&elems[CPC_DESIRED_REG], &cc->cc_desired,
	    &intval)) {
		cmn_err(CE_WARN, "!cppc: CPU %d: failed to parse "
		    "DesiredPerformance register", cpuid);
		AcpiOsFree(buf.Pointer);
		return (DDI_FAILURE);
	}
	if (cc->cc_desired.cr_valid &&
	    cc->cc_desired.cr_spaceid == CPPC_SPACE_PCC) {
		cc->cc_uses_pcc = B_TRUE;
		cppc_set_pcc_chan(cc, (uint_t)cc->cc_desired.cr_access);
	}

	/*
	 * Entries 8-12: MinPerf, MaxPerf, PerfReductionTolerance,
	 * TimeWindow, CounterWraparoundTime - parsed but not stored;
	 * not needed for basic DVFS.
	 */

	/* Entry 13: ReferencePerformanceCounterRegister */
	if (!cppc_parse_entry(&elems[CPC_REF_CTR_REG], &cc->cc_reference,
	    &intval)) {
		cmn_err(CE_WARN, "!cppc: CPU %d: failed to parse "
		    "ReferenceCounter register", cpuid);
		AcpiOsFree(buf.Pointer);
		return (DDI_FAILURE);
	}
	if (cc->cc_reference.cr_valid &&
	    cc->cc_reference.cr_spaceid == CPPC_SPACE_PCC) {
		cc->cc_uses_pcc = B_TRUE;
		cppc_set_pcc_chan(cc, (uint_t)cc->cc_reference.cr_access);
	}

	/* Entry 14: DeliveredPerformanceCounterRegister */
	if (!cppc_parse_entry(&elems[CPC_DELIVERED_CTR_REG],
	    &cc->cc_delivered, &intval)) {
		cmn_err(CE_WARN, "!cppc: CPU %d: failed to parse "
		    "DeliveredCounter register", cpuid);
		AcpiOsFree(buf.Pointer);
		return (DDI_FAILURE);
	}
	if (cc->cc_delivered.cr_valid &&
	    cc->cc_delivered.cr_spaceid == CPPC_SPACE_PCC) {
		cc->cc_uses_pcc = B_TRUE;
		cppc_set_pcc_chan(cc, (uint_t)cc->cc_delivered.cr_access);
	}

	/* Entry 16: CPPCEnableRegister */
	(void) cppc_parse_entry(&elems[CPC_ENABLE_REG], &cc->cc_enable,
	    &intval);
	if (cc->cc_enable.cr_valid &&
	    cc->cc_enable.cr_spaceid == CPPC_SPACE_PCC) {
		cc->cc_uses_pcc = B_TRUE;
		cppc_set_pcc_chan(cc, (uint_t)cc->cc_enable.cr_access);
	}

	/* Entry 20: ReferencePerformance */
	if (cppc_parse_entry(&elems[CPC_REF_PERF], &cc->cc_ref_perf_reg,
	    &intval)) {
		cc->cc_ref_perf = intval;
		if (cc->cc_ref_perf_reg.cr_valid &&
		    cc->cc_ref_perf_reg.cr_spaceid == CPPC_SPACE_PCC) {
			cc->cc_uses_pcc = B_TRUE;
			cppc_set_pcc_chan(cc,
			    (uint_t)cc->cc_ref_perf_reg.cr_access);
		}
	}
	if (cc->cc_ref_perf == 0) {
		cc->cc_ref_perf = cc->cc_nominal;
	}

	/* Entry 21: LowestFrequency (MHz) */
	if (cppc_parse_entry(&elems[CPC_LOWEST_FREQ],
	    &cc->cc_lowest_freq_reg, &intval)) {
		cc->cc_lowest_freq = intval;
		if (cc->cc_lowest_freq_reg.cr_valid &&
		    cc->cc_lowest_freq_reg.cr_spaceid == CPPC_SPACE_PCC) {
			cc->cc_uses_pcc = B_TRUE;
			cppc_set_pcc_chan(cc,
			    (uint_t)cc->cc_lowest_freq_reg.cr_access);
		}
	}

	/* Entry 22: NominalFrequency (MHz) */
	if (cppc_parse_entry(&elems[CPC_NOMINAL_FREQ],
	    &cc->cc_nominal_freq_reg, &intval)) {
		cc->cc_nominal_freq = intval;
		if (cc->cc_nominal_freq_reg.cr_valid &&
		    cc->cc_nominal_freq_reg.cr_spaceid == CPPC_SPACE_PCC) {
			cc->cc_uses_pcc = B_TRUE;
			cppc_set_pcc_chan(cc,
			    (uint_t)cc->cc_nominal_freq_reg.cr_access);
		}
	}

	AcpiOsFree(buf.Pointer);
	return (DDI_SUCCESS);
}

/*
 * Read the deferred PCC register values for performance levels that
 * are stored as PCC registers rather than integer constants.
 *
 * Register descriptors were stashed during _CPC parsing, so no
 * re-evaluation is needed here - we just issue a CMD_READ and pull
 * the values from the already-known offsets.
 *
 * Must be called after PCC is initialised and the channel is usable.
 * Sends a single CMD_READ and reads all needed values.
 */
static int
cppc_read_pcc_levels(cppc_cpu_t *cc)
{
	cppc_reg_t *regs[8];
	uint32_t *vals[8];
	uint32_t val;
	int ret;
	int i;

	ASSERT3P(cc, !=, NULL);

	if (!cc->cc_uses_pcc) {
		return (DDI_SUCCESS);
	}

	regs[0] = &cc->cc_highest_reg;
	vals[0] = &cc->cc_highest;
	regs[1] = &cc->cc_nominal_reg;
	vals[1] = &cc->cc_nominal;
	regs[2] = &cc->cc_lowest_nl_reg;
	vals[2] = &cc->cc_lowest_nl;
	regs[3] = &cc->cc_lowest_reg;
	vals[3] = &cc->cc_lowest;
	regs[4] = &cc->cc_guaranteed_reg;
	vals[4] = &cc->cc_guaranteed;
	regs[5] = &cc->cc_ref_perf_reg;
	vals[5] = &cc->cc_ref_perf;
	regs[6] = &cc->cc_nominal_freq_reg;
	vals[6] = &cc->cc_nominal_freq;
	regs[7] = &cc->cc_lowest_freq_reg;
	vals[7] = &cc->cc_lowest_freq;

	/* Begin a transaction and send CMD_READ to populate registers */
	ret = pcc_chan_txn_begin(cc->cc_pcc_chan);
	if (ret != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	ret = pcc_chan_txn_commit(cc->cc_pcc_chan, CPPC_PCC_CMD_READ, 0);
	if (ret != DDI_SUCCESS) {
		pcc_chan_txn_end(cc->cc_pcc_chan);
		return (DDI_FAILURE);
	}

	for (i = 0; i < 8; i++) {
		if (!regs[i]->cr_valid ||
		    regs[i]->cr_spaceid != CPPC_SPACE_PCC) {
			continue;
		}

		if (pcc_chan_read32(cc->cc_pcc_chan,
		    (uint32_t)regs[i]->cr_addr, &val) == DDI_SUCCESS) {
			*vals[i] = val;
		}
	}

	pcc_chan_txn_end(cc->cc_pcc_chan);

	/* Ensure ref_perf has a sane value */
	if (cc->cc_ref_perf == 0) {
		cc->cc_ref_perf = cc->cc_nominal;
	}

	return (DDI_SUCCESS);
}

/*
 * Read performance level values from SystemMemory registers.
 *
 * Similar to the PCC path but for directly-mapped MMIO.
 */
static int
cppc_read_sysmem_levels(cppc_cpu_t *cc)
{
	cppc_reg_t *regs[4];
	uint32_t *vals[4];
	int i;

	ASSERT3P(cc, !=, NULL);

	regs[0] = &cc->cc_highest_reg;
	vals[0] = &cc->cc_highest;
	regs[1] = &cc->cc_nominal_reg;
	vals[1] = &cc->cc_nominal;
	regs[2] = &cc->cc_lowest_nl_reg;
	vals[2] = &cc->cc_lowest_nl;
	regs[3] = &cc->cc_lowest_reg;
	vals[3] = &cc->cc_lowest;

	for (i = 0; i < 4; i++) {
		volatile uint32_t *va;
		caddr_t mapped;
		size_t len;

		if (!regs[i]->cr_valid ||
		    regs[i]->cr_spaceid != CPPC_SPACE_SYSMEM) {
			continue;
		}

		len = (regs[i]->cr_width + 7) / 8;
		if (len == 0) {
			len = 4;
		}

		mapped = psm_map_phys((paddr_t)regs[i]->cr_addr, len,
		    PROT_READ);
		if (mapped == NULL) {
			continue;
		}

		va = (volatile uint32_t *)(void *)mapped;
		*vals[i] = *va;
		psm_unmap_phys(mapped, len);
	}

	return (DDI_SUCCESS);
}

/*
 * Enable CPPC on this CPU (when the CPPCEnable register is present).
 */
static void
cppc_enable(cppc_cpu_t *cc)
{
	ASSERT3P(cc, !=, NULL);

	if (!cc->cc_enable.cr_valid) {
		return;
	}

	if (cc->cc_uses_pcc && cc->cc_enable.cr_spaceid == CPPC_SPACE_PCC) {
		if (pcc_chan_txn_begin(cc->cc_pcc_chan) == DDI_SUCCESS) {
			(void) cppc_reg_write(cc, &cc->cc_enable, 1);
			(void) pcc_chan_txn_commit(cc->cc_pcc_chan,
			    CPPC_PCC_CMD_WRITE, cppc_reg_len(&cc->cc_enable));
			pcc_chan_txn_end(cc->cc_pcc_chan);
		}
	} else {
		(void) cppc_reg_write(cc, &cc->cc_enable, 1);
	}
}

/*
 * Initialize CPPC for one CPU.
 *
 * Allocates a cppc_cpu_t, parses _CPC, initialises transports,
 * validates performance data, enables CPPC, and joins the
 * appropriate coordination domain.
 *
 * On success, installs the cppc_cpu_t pointer in the machcpu
 * structure so subsequent DVFS calls can find it.  On failure,
 * cleans up and returns DDI_FAILURE (not fatal - DVFS is simply
 * unavailable for this CPU).
 */
int
cppc_cpu_init(cpu_t *cp, void *acpi_hdl)
{
	ACPI_HANDLE hdl;
	cppc_cpu_t *cc;
	processorid_t cpuid;
	int ret;

	ASSERT3P(cp, !=, NULL);
	ASSERT3P(acpi_hdl, !=, NULL);

	hdl = (ACPI_HANDLE)acpi_hdl;
	cpuid = cp->cpu_id;

	cc = kmem_zalloc(sizeof (cppc_cpu_t), KM_SLEEP);

	/* Parse _CPC */
	ret = cppc_parse_cpc(hdl, cc, cpuid);
	if (ret != DDI_SUCCESS) {
		cppc_cpu_free(cc);
		return (DDI_FAILURE);
	}

	/*
	 * PCC initialisation is done by acpica.  If any _CPC register
	 * uses PCC but no channel handle could be acquired, the
	 * firmware description is unusable for this CPU.
	 */
	if (cc->cc_uses_pcc && cc->cc_pcc_chan == NULL) {
		cmn_err(CE_WARN, "!cppc: CPU %d: no usable PCC channel",
		    cpuid);
		cppc_cpu_free(cc);
		return (DDI_FAILURE);
	}

	/*
	 * ACPI CPPC does not support more than one PCC subspace in performance
	 * domain (or in a _CPC object).
	 */
	if (cc->cc_pcc_mismatch) {
		cmn_err(CE_WARN, "!cppc: CPU %d: _CPC uses more than one "
		    "PCC subspace", cpuid);
		cppc_cpu_free(cc);
		return (DDI_FAILURE);
	}

	/*
	 * Map SystemMemory registers.  This must happen before we
	 * try to read from them.
	 */
	cppc_map_sysmem_reg(&cc->cc_desired);
	cppc_map_sysmem_reg(&cc->cc_reference);
	cppc_map_sysmem_reg(&cc->cc_delivered);
	cppc_map_sysmem_reg(&cc->cc_enable);

	/*
	 * Read deferred performance level values.  These may be stored
	 * as PCC or SystemMemory registers.  Integer values were already
	 * captured during _CPC parsing.
	 */
	if (cc->cc_uses_pcc) {
		ret = cppc_read_pcc_levels(cc);
		if (ret != DDI_SUCCESS) {
			cmn_err(CE_WARN, "!cppc: CPU %d: failed to read "
			    "PCC perf levels", cpuid);
			cppc_cpu_free(cc);
			return (DDI_FAILURE);
		}
	}
	(void) cppc_read_sysmem_levels(cc);

	/*
	 * Validate that we have usable performance data.
	 */
	if (cc->cc_highest == 0 || cc->cc_nominal == 0 ||
	    cc->cc_lowest == 0) {
		cmn_err(CE_WARN, "!cppc: CPU %d: incomplete performance "
		    "data (highest=%u nominal=%u lowest=%u)",
		    cpuid, cc->cc_highest, cc->cc_nominal, cc->cc_lowest);
		cppc_cpu_free(cc);
		return (DDI_FAILURE);
	}

	if (cc->cc_lowest_nl == 0) {
		cc->cc_lowest_nl = cc->cc_lowest;
	}

	/*
	 * Validate the performance level ordering.
	 */
	if (cc->cc_highest < cc->cc_nominal ||
	    cc->cc_nominal < cc->cc_lowest_nl ||
	    cc->cc_lowest_nl < cc->cc_lowest) {
		cmn_err(CE_WARN, "!cppc: CPU %d: performance levels out "
		    "of order (highest=%u nominal=%u lowest_nl=%u "
		    "lowest=%u)", cpuid, cc->cc_highest, cc->cc_nominal,
		    cc->cc_lowest_nl, cc->cc_lowest);
		cppc_cpu_free(cc);
		return (DDI_FAILURE);
	}

	/*
	 * We need either NominalFrequency from _CPC or the ability to
	 * derive it.  Without a frequency reference, we cannot convert
	 * between performance levels and MHz.
	 */
	if (cc->cc_nominal_freq == 0) {
		cmn_err(CE_WARN, "!cppc: CPU %d: NominalFrequency is 0, "
		    "cannot derive speed table", cpuid);
		cppc_cpu_free(cc);
		return (DDI_FAILURE);
	}

	/*
	 * Validate that the desired performance register is writable.
	 */
	if (!cc->cc_desired.cr_valid) {
		cmn_err(CE_WARN, "!cppc: CPU %d: DesiredPerformance "
		    "register is not available", cpuid);
		cppc_cpu_free(cc);
		return (DDI_FAILURE);
	}

	/* Enable CPPC if the platform requires it */
	cppc_enable(cc);

	/* Seed desired performance and last speed to nominal */
	cc->cc_desired_perf = cc->cc_nominal;
	cc->cc_last_speed = cc->cc_nominal_freq;

	/*
	 * Join the coordination domain.  This evaluates _PSD and
	 * places this CPU into the appropriate domain.  Done last
	 * so that the cppc_cpu_t is fully initialised before it
	 * becomes visible to other CPUs via the domain member list.
	 */
	ret = cppc_domain_join(hdl, cpuid, cc);
	if (ret != DDI_SUCCESS) {
		cppc_cpu_free(cc);
		return (DDI_FAILURE);
	}

	cc->cc_valid = B_TRUE;

	/* Publish to machcpu */
	cp->cpu_m.mcpu_cppc = cc;

	cmn_err(CE_CONT, "!cppc: CPU %d: highest=%u nominal=%u "
	    "lowest_nl=%u lowest=%u nominal_freq=%u MHz\n",
	    cpuid, cc->cc_highest, cc->cc_nominal,
	    cc->cc_lowest_nl, cc->cc_lowest, cc->cc_nominal_freq);

	return (DDI_SUCCESS);
}

boolean_t
cppc_cpu_available(cpu_t *cp)
{
	cppc_cpu_t *cc;

	ASSERT3P(cp, !=, NULL);

	cc = cp->cpu_m.mcpu_cppc;
	if (cc == NULL) {
		return (B_FALSE);
	}

	return (cc->cc_valid);
}

/*
 * Convert a CPPC performance level to MHz.
 */
static int
cppc_perf_to_mhz(cppc_cpu_t *cc, uint32_t perf)
{
	ASSERT3P(cc, !=, NULL);
	ASSERT(cc->cc_nominal != 0);

	return ((int)((uint64_t)perf * cc->cc_nominal_freq /
	    cc->cc_nominal));
}

/*
 * Convert MHz to a CPPC performance level.
 */
static uint32_t
cppc_mhz_to_perf(cppc_cpu_t *cc, int mhz)
{
	uint32_t perf;

	ASSERT3P(cc, !=, NULL);
	ASSERT(cc->cc_nominal_freq != 0);

	perf = (uint32_t)((uint64_t)mhz * cc->cc_nominal /
	    cc->cc_nominal_freq);

	/* Clamp to valid range */
	if (perf > cc->cc_highest) {
		perf = cc->cc_highest;
	}
	if (perf < cc->cc_lowest) {
		perf = cc->cc_lowest;
	}

	return (perf);
}

int
cppc_get_speeds(cpu_t *cp, int **speeds, int *nspeeds)
{
	cppc_cpu_t *cc;
	uint32_t anchors[CPPC_MAX_SPEEDS];
	int freqs[CPPC_MAX_SPEEDS];
	int nanchors;
	int nfreqs;
	int *result;
	int i;
	int j;

	ASSERT3P(cp, !=, NULL);
	ASSERT3P(speeds, !=, NULL);
	ASSERT3P(nspeeds, !=, NULL);

	cc = cp->cpu_m.mcpu_cppc;
	if (cc == NULL || !cc->cc_valid) {
		return (DDI_FAILURE);
	}

	/*
	 * Build the anchor set from the four key performance levels.
	 * Deduplicate adjacent equal values (e.g., highest == nominal
	 * on some platforms).
	 */
	nanchors = 0;
	anchors[nanchors++] = cc->cc_highest;

	if (cc->cc_nominal != cc->cc_highest) {
		anchors[nanchors++] = cc->cc_nominal;
	}

	if (cc->cc_lowest_nl != cc->cc_nominal &&
	    cc->cc_lowest_nl != cc->cc_lowest) {
		anchors[nanchors++] = cc->cc_lowest_nl;
	}

	if (cc->cc_lowest != anchors[nanchors - 1]) {
		anchors[nanchors++] = cc->cc_lowest;
	}

	/*
	 * Insert midpoints between adjacent anchors when the gap is
	 * larger than 2 performance levels.  This provides finer
	 * granularity in the transition regions.
	 */
	nfreqs = 0;
	for (i = 0; i < nanchors; i++) {
		freqs[nfreqs++] = cppc_perf_to_mhz(cc, anchors[i]);

		if (i + 1 < nanchors && nfreqs < CPPC_MAX_SPEEDS) {
			uint32_t gap;
			uint32_t mid;

			if (anchors[i] > anchors[i + 1]) {
				gap = anchors[i] - anchors[i + 1];
			} else {
				gap = anchors[i + 1] - anchors[i];
			}

			if (gap > 2) {
				mid = (anchors[i] + anchors[i + 1]) / 2;
				freqs[nfreqs++] = cppc_perf_to_mhz(cc, mid);
			}
		}
	}

	/*
	 * Sort descending (highest frequency first).
	 */
	for (i = 1; i < nfreqs; i++) {
		int key;

		key = freqs[i];
		j = i - 1;
		while (j >= 0 && freqs[j] < key) {
			freqs[j + 1] = freqs[j];
			j--;
		}

		freqs[j + 1] = key;
	}

	/* Deduplicate */
	j = 0;
	for (i = 1; i < nfreqs; i++) {
		if (freqs[i] != freqs[j]) {
			freqs[++j] = freqs[i];
		}
	}
	nfreqs = j + 1;

	/* Remove any zero entries */
	while (nfreqs > 0 && freqs[nfreqs - 1] == 0) {
		nfreqs--;
	}

	if (nfreqs == 0) {
		return (DDI_FAILURE);
	}

	result = kmem_alloc(nfreqs * sizeof (int), KM_SLEEP);
	memcpy(result, freqs, nfreqs * sizeof (int));

	*speeds = result;
	*nspeeds = nfreqs;
	return (DDI_SUCCESS);
}

void
cppc_free_speeds(int *speeds, int nspeeds)
{
	if (speeds != NULL && nspeeds > 0) {
		kmem_free(speeds, nspeeds * sizeof (int));
	}
}

/*
 * Write a DesiredPerformance value for a single CPU.
 *
 * The caller is responsible for domain-level locking.
 */
static int
cppc_write_desired(cppc_cpu_t *cc, uint32_t perf)
{
	int ret;

	ASSERT3P(cc, !=, NULL);

	if (cc->cc_uses_pcc && cc->cc_desired.cr_spaceid == CPPC_SPACE_PCC) {
		ret = pcc_chan_txn_begin(cc->cc_pcc_chan);
		if (ret == DDI_SUCCESS) {
			ret = cppc_reg_write(cc, &cc->cc_desired, perf);
			if (ret == DDI_SUCCESS) {
				ret = pcc_chan_txn_commit(cc->cc_pcc_chan,
				    CPPC_PCC_CMD_WRITE,
				    cppc_reg_len(&cc->cc_desired));
			}

			pcc_chan_txn_end(cc->cc_pcc_chan);
		}
	} else {
		ret = cppc_reg_write(cc, &cc->cc_desired, perf);
	}

	return (ret);
}

/*
 * Set the desired CPU speed in MHz.
 *
 * Converts the frequency to an abstract CPPC performance level, then
 * aggregates across the coordination domain using max policy.  If the
 * domain-wide target has changed, writes the new target to the
 * appropriate registers based on the coordination type:
 *
 *   HW_ALL:  Write to this CPU's register only (hardware coordinates).
 *   SW_ANY:  Write to this CPU's register only (platform applies
 *            the value domain-wide).
 *   SW_ALL:  Write to every domain member's register.
 *
 * Updates cc_last_speed for all domain members to the target MHz.
 */
int
cppc_set_speed(cpu_t *cp, int speed_mhz)
{
	cppc_cpu_t *cc;
	cppc_domain_t *dom;
	uint32_t perf;
	uint32_t target;
	int target_mhz;
	int ret;
	uint_t i;

	ASSERT3P(cp, !=, NULL);

	cc = cp->cpu_m.mcpu_cppc;
	if (cc == NULL || !cc->cc_valid) {
		return (DDI_FAILURE);
	}

	dom = cc->cc_domain;
	ASSERT3P(dom, !=, NULL);

	perf = cppc_mhz_to_perf(cc, speed_mhz);

	mutex_enter(&dom->cd_lock);

	cc->cc_desired_perf = perf;

	/* Aggregate: max across all domain members */
	target = 0;
	for (i = 0; i < dom->cd_ncpus; i++) {
		if (dom->cd_members[i]->cc_desired_perf > target) {
			target = dom->cd_members[i]->cc_desired_perf;
		}
	}

	target_mhz = cppc_perf_to_mhz(cc, target);

	if (target == dom->cd_target) {
		/*
		 * Domain target unchanged - no register write
		 * needed.  Still update this CPU's last_speed to
		 * reflect the domain target.
		 */
		cc->cc_last_speed = (uint32_t)target_mhz;
		mutex_exit(&dom->cd_lock);
		return (DDI_SUCCESS);
	}

	dom->cd_target = target;

	switch (dom->cd_coord) {
	case PSD_COORD_HW_ALL:
	case PSD_COORD_SW_ANY:
		/*
		 * HW_ALL: hardware coordinates across CPUs, write
		 * to this CPU only.
		 * SW_ANY: platform accepts a write from any member,
		 * write to this CPU only.
		 */
		ret = cppc_write_desired(cc, target);
		break;

	case PSD_COORD_SW_ALL:
		/*
		 * All domain members must be programmed with the
		 * same value.  Write to each member in turn.
		 */
		ret = DDI_SUCCESS;
		for (i = 0; i < dom->cd_ncpus; i++) {
			ret = cppc_write_desired(dom->cd_members[i], target);
			if (ret != DDI_SUCCESS) {
				break;
			}
		}

		break;

	default:
		ret = DDI_FAILURE;
		break;
	}

	/* Update last_speed for all domain members */
	for (i = 0; i < dom->cd_ncpus; i++) {
		dom->cd_members[i]->cc_last_speed = (uint32_t)target_mhz;
	}

	mutex_exit(&dom->cd_lock);
	return (ret);
}

/*
 * Get the current delivered CPU speed in MHz.
 *
 * Attempts to read the actual delivered frequency using AMU counters (via FFH).
 * If counters are unavailable, falls back to the last successfully set speed.
 *
 * The AMU counters are read from the current CPU, so this must be called on
 * the CPU whose speed is being queried.  The caller is responsible for running
 * on the target CPU (e.g. via xcall).
 */
uint64_t
cppc_get_speed(cpu_t *cp)
{
	cppc_cpu_t *cc;
	uint64_t del_s, ref_s, del_e, ref_e;
	uint64_t delta_del, delta_ref;
	uint32_t perf;
	uint32_t last_perf;
	int stable;
	int ret;
	int i;

	ASSERT3P(cp, !=, NULL);

	cc = cp->cpu_m.mcpu_cppc;
	if (cc == NULL || !cc->cc_valid) {
		return (0);
	}

	/*
	 * If we have FFH-based counters (AMU), compute delivered performance
	 * from counter deltas using a retry-until-stable pattern.
	 *
	 * Each iteration takes two snapshots of the delivered (core cycle) and
	 * reference (constant frequency) counters with a short delay between
	 * them, computes the performance ratio, and checks whether consecutive
	 * readings agree.  Three consecutive stable readings are required to
	 * filter out jitter from non-atomic MRS pairs.
	 *
	 *   delivered_perf = (delta_delivered / delta_reference) * ref_perf
	 */
	if (cc->cc_delivered.cr_valid &&
	    cc->cc_delivered.cr_spaceid == CPPC_SPACE_FFH &&
	    cc->cc_reference.cr_valid &&
	    cc->cc_reference.cr_spaceid == CPPC_SPACE_FFH) {
		last_perf = 0;
		stable = 0;

		for (i = 0; i < CPPC_AMU_RETRIES; i++) {
			ret = cppc_reg_read(cc, &cc->cc_delivered, &del_s);
			if (ret != DDI_SUCCESS) {
				return ((uint64_t)cc->cc_last_speed);
			}

			ret = cppc_reg_read(cc, &cc->cc_reference, &ref_s);
			if (ret != DDI_SUCCESS) {
				return ((uint64_t)cc->cc_last_speed);
			}

			drv_usecwait(CPPC_AMU_DELAY_USEC);

			ret = cppc_reg_read(cc, &cc->cc_delivered, &del_e);
			if (ret != DDI_SUCCESS) {
				return ((uint64_t)cc->cc_last_speed);
			}

			ret = cppc_reg_read(cc, &cc->cc_reference, &ref_e);
			if (ret != DDI_SUCCESS) {
				return ((uint64_t)cc->cc_last_speed);
			}

			delta_del = del_e - del_s;
			delta_ref = ref_e - ref_s;

			if (delta_ref == 0 || delta_del == 0) {
				continue;
			}

			perf = (uint32_t)((delta_del * cc->cc_ref_perf) /
			    delta_ref);
			if (perf == 0) {
				continue;
			}

			if (perf > cc->cc_highest) {
				perf = cc->cc_highest;
			}

			if (perf < cc->cc_lowest) {
				perf = cc->cc_lowest;
			}

			if (last_perf != 0) {
				uint32_t diff;

				if (perf > last_perf) {
					diff = perf - last_perf;
				} else {
					diff = last_perf - perf;
				}

				if (diff <= CPPC_AMU_STABLE_PERF) {
					stable++;

					if (stable >= 2) {
						return ((uint64_t)
						    cppc_perf_to_mhz(cc,
						    perf));
					}
				} else {
					stable = 0;
				}
			}

			last_perf = perf;
		}

		/*
		 * Never stabilised, use the last requested speed.
		 */
		return ((uint64_t)cc->cc_last_speed);
	}

	/*
	 * PCC-based counter reads need a PCC transaction, which is expensive.
	 *
	 * Just use the last requested speed to avoid these overheads on every
	 * query.
	 */
	return ((uint64_t)cc->cc_last_speed);
}

/*
 * Module-level initialisation.
 *
 * Allocates the domain table (sized to max_ncpus, the worst case of one domain
 * per CPU under HW_ALL) and initialises the domain table lock.
 */
static void
cppc_init(void)
{
	ASSERT(max_ncpus > 0);

	cppc_domains = kmem_zalloc(
	    max_ncpus * sizeof (cppc_domain_t), KM_SLEEP);
	cppc_ndomains = 0;
	mutex_init(&cppc_domain_lock, NULL, MUTEX_DEFAULT, NULL);
}

/*
 * Module infrastructure
 */

static struct modlmisc modlmisc = {
	.misc_modops	= &mod_miscops,
	.misc_linkinfo	= "ACPI CPPC driver"
};

static struct modlinkage modlinkage = {
	.ml_rev		= MODREV_1,
	.ml_linkage	= { &modlmisc, NULL }
};

int
_init(void)
{
	cppc_init();
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	/* Do not allow unload - per-CPU state may be in use */
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
