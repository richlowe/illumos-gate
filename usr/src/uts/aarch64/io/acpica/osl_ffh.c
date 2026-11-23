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
 * ACPICA OSL address space handler for Arm Functional Fixed Hardware
 * (FFH, address space 0x7F) - DEN0048D v1.3.
 *
 * FFH OpRegion fields use BufferAcc access.  When AML writes to such a
 * field, the handler receives the register buffer, dispatches the
 * firmware call via ffh_opregion_write, and returns the results in
 * the same buffer.
 *
 * Region metadata (Offset and Length) is communicated via an
 * ACPI_FFH_INFO context struct, following the upstream ACPICA
 * convention.  ACPICA populates the handler context from the region
 * object before calling the setup function (see evregion.c); the setup
 * function copies it into a per-region context so the handler can read
 * the region length at dispatch time.
 *
 * AMU counter reads for CPPC (DEN0048D S2.2.1) do not pass through
 * this handler; the CPPC driver calls ffh_amu_read directly because
 * AcpiRead/AcpiWrite only support SystemMemory and SystemIO
 * address spaces.
 */

#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/note.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include "ffh.h"

/*
 * Per-region context, allocated by the setup function and freed on
 * deactivation.  Captures the region's Length from the ACPI_FFH_INFO
 * handler context so the handler knows the actual buffer size.
 */
typedef struct osl_ffh_region_ctx {
	uint64_t	ofrc_length;
} osl_ffh_region_ctx_t;

/*
 * Shared handler context, passed to AcpiInstallAddressSpaceHandler.
 *
 * ACPICA populates Offset and Length from the RegionObj before calling
 * the setup function.
 */
static ACPI_FFH_INFO osl_ffh_ctx;

/*
 * Setup function for FFH address space regions.
 *
 * On ACPI_REGION_ACTIVATE, allocates a per-region context and copies
 * the region Length from the handler context (ACPI_FFH_INFO) that
 * ACPICA populated from the region object.
 *
 * On ACPI_REGION_DEACTIVATE, frees the per-region context.
 */
static ACPI_STATUS
osl_ffh_setup(ACPI_HANDLE region_handle __unused, UINT32 function,
    void *handler_context, void **region_context)
{
	ACPI_FFH_INFO *ffh_info;
	osl_ffh_region_ctx_t *rctx;

	switch (function) {
	case ACPI_REGION_ACTIVATE:
		ASSERT3P(handler_context, !=, NULL);
		ASSERT3P(region_context, !=, NULL);

		ffh_info = (ACPI_FFH_INFO *)handler_context;
		rctx = kmem_zalloc(sizeof (*rctx), KM_SLEEP);
		rctx->ofrc_length = ffh_info->Length;
		*region_context = rctx;
		return (AE_OK);

	case ACPI_REGION_DEACTIVATE:
		ASSERT3P(region_context, !=, NULL);
		rctx = *region_context;
		if (rctx != NULL) {
			kmem_free(rctx, sizeof (*rctx));
			*region_context = NULL;
		}
		return (AE_OK);

	default:
		return (AE_BAD_PARAMETER);
	}
}

/*
 * ACPI address space handler for ACPI_ADR_SPACE_FIXED_HARDWARE (0x7F).
 *
 * For BufferAcc writes, the handler dispatches the firmware call via
 * ffh_opregion_write and overwrites the buffer with the results.
 * The OperationRegion Offset (carried in addr) selects SMC32, SMC64,
 * or FF-A DIRECT_REQ2 respectively.
 *
 * The buffer length comes from the per-region context that the setup
 * function populated from ACPI_FFH_INFO.
 *
 * For BufferAcc reads, the handler returns success without modifying
 * the buffer.  FFH register reads are meaningless without a preceding
 * write; results are always returned in the write path's buffer.
 */
static ACPI_STATUS
osl_ffh_handler(UINT32 func, ACPI_PHYSICAL_ADDRESS addr, UINT32 width __unused,
    UINT64 *val, void *handler_ctx __unused, void *region_ctx)
{
	static volatile int ffh_warned = 0;
	osl_ffh_region_ctx_t *rctx;
	size_t len;
	int ret;

	if (acpica_get_plat_osc(PLAT_OSC_FFH) == 0) {
		if (atomic_cas_uint((volatile uint_t *)&ffh_warned, 0, 1)
		    == 0) {
			cmn_err(CE_NOTE, "!osl_ffh: FFH OpRegion access "
			    "denied, not granted by \\_SB._OSC");
		}
		return (AE_SUPPORT);
	}

	ASSERT3P(region_ctx, !=, NULL);
	rctx = (osl_ffh_region_ctx_t *)region_ctx;
	len = (size_t)rctx->ofrc_length;

	switch (func & ACPI_IO_MASK) {
	case ACPI_WRITE:
		ret = ffh_opregion_write((uint_t)addr, (void *)val, len);
		if (ret != 0) {
			cmn_err(CE_NOTE, "!osl_ffh: write at offset %u "
			    "len %lu failed: %d",
			    (uint_t)addr, (unsigned long)len, ret);
			return (AE_ERROR);
		}
		return (AE_OK);

	case ACPI_READ:
		/*
		 * Read is a no-op for FFH OpRegions.  The buffer was
		 * zeroed by the caller.  Results come back
		 * via the write path.
		 */
		return (AE_OK);

	default:
		return (AE_BAD_PARAMETER);
	}
}

/*
 * Install the FFH address space handler on the ACPI root object.
 *
 * Called from acpica_install_handlers before AcpiEnableSubsystem,
 * so the handler is in place before any AML that references FFH
 * OpRegions can execute.
 *
 * The handler context is a static ACPI_FFH_INFO struct; ACPICA
 * populates it with the region's Offset and Length before each setup
 * call.
 */
int
osl_ffh_install(void)
{
	ACPI_STATUS res;

	res = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
	    ACPI_ADR_SPACE_FIXED_HARDWARE,
	    osl_ffh_handler, osl_ffh_setup, &osl_ffh_ctx);
	if (res != AE_OK && res != AE_SAME_HANDLER) {
		cmn_err(CE_WARN, "!acpica: failed to install FFH "
		    "handler (0x7F): 0x%x", res);
		return (AE_ERROR);
	}

	return (AE_OK);
}
