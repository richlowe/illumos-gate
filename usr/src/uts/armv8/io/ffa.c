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
 * Arm Firmware Framework for Arm A-profile (FF-A) - DEN0077A v1.3.
 *
 * This module implements the normal-world FF-A client.  It discovers the
 * Secure Partition Manager Core (SPMC) via SMCCC at boot and provides
 * synchronous messaging to secure partitions via FFA_MSG_SEND_DIRECT_REQ2.
 *
 * Initialisation occurs once the kernel virtual memory subsystem is available.
 *
 * Specification references are against DEN0077A v1.3.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/time.h>
#include <sys/uuid.h>
#include <vm/hat.h>
#include <vm/seg_kmem.h>
#include <vm/page.h>
#include <sys/smccc.h>
#include <sys/ffa.h>

/*
 * Tunable: retry warning watermark for YIELD/INTERRUPT loops.
 */
int ffa_retry_warn_seconds = 30;

/*
 * Partition information descriptor (Table 6.1).
 *
 * Returned in the RX buffer by FFA_PARTITION_INFO_GET.
 */
#define	FFA_PART_DESC_SZ	48

typedef struct ffa_part_desc {
	uint16_t	fpd_id;
	uint16_t	fpd_ec_count;
	uint32_t	fpd_properties;
	uint8_t		fpd_proto_uuid[16];
	uint8_t		fpd_image_uuid[16];
	uint32_t	fpd_ffa_version;
	uint32_t	fpd_reserved;
} ffa_part_desc_t;

/*
 * Module state.  Initialised once in ffa_init, read-only thereafter.
 */
static struct ffa_state {
	boolean_t	fs_available;
	uint32_t	fs_version;
	uint16_t	fs_vm_id;
	caddr_t		fs_tx_buf;
	caddr_t		fs_rx_buf;
	size_t		fs_rxtx_bufsz;
	kmutex_t	fs_tx_lock;
	kmutex_t	fs_rx_lock;
} ffa_state;

/*
 * Internal helpers
 */

/*
 * Map an FF-A error code to an errno value.
 */
static int
ffa_err_to_errno(int32_t ffa_err)
{
	switch (ffa_err) {
	case FFA_ERR_NOT_SUPPORTED:
		return (ENOTSUP);
	case FFA_ERR_INVALID_PARAMETERS:
		return (EINVAL);
	case FFA_ERR_NO_MEMORY:
		return (ENOMEM);
	case FFA_ERR_BUSY:
		return (EBUSY);
	case FFA_ERR_INTERRUPTED:
		return (EINTR);
	case FFA_ERR_DENIED:
		return (EACCES);
	case FFA_ERR_RETRY:
		return (EAGAIN);
	case FFA_ERR_ABORTED:
		return (ECANCELED);
	case FFA_ERR_NO_DATA:
		return (ENODATA);
	case FFA_ERR_NOT_READY:
		return (ENXIO);
	default:
		return (EIO);
	}
}

/*
 * Pack a uuid_t into two 64-bit registers per FF-A Table 6.4.
 *
 * "Bytes 0..7 with byte 0 in the low-order bits"
 *
 * On LE AArch64 this is just a memcpy from the uuid_t byte array.
 */
static void
ffa_uuid_to_regs64(const uuid_t *uuid, uint64_t *lo, uint64_t *hi)
{
	ASSERT3P(uuid, !=, NULL);
	ASSERT3P(lo, !=, NULL);
	ASSERT3P(hi, !=, NULL);

	(void) memcpy(lo, uuid, 8);
	(void) memcpy(hi, (const uint8_t *)uuid + 8, 8);
}

/*
 * Pack a uuid_t into four 32-bit registers per SMCCC §5.3.
 *
 * Same "byte N in low-order bits" convention as ffa_uuid_to_regs64.
 */
static void
ffa_uuid_to_regs32(const uuid_t *uuid, uint32_t *w1, uint32_t *w2,
    uint32_t *w3, uint32_t *w4)
{
	ASSERT3P(uuid, !=, NULL);
	ASSERT3P(w1, !=, NULL);
	ASSERT3P(w2, !=, NULL);
	ASSERT3P(w3, !=, NULL);
	ASSERT3P(w4, !=, NULL);

	(void) memcpy(w1, uuid, 4);
	(void) memcpy(w2, (const uint8_t *)uuid + 4, 4);
	(void) memcpy(w3, (const uint8_t *)uuid + 8, 4);
	(void) memcpy(w4, (const uint8_t *)uuid + 12, 4);
}

/*
 * Free a buffer allocated by ffa_buf_alloc.
 */
static void
ffa_buf_free(caddr_t va, size_t size)
{
	pgcnt_t npages;
	pgcnt_t i;
	caddr_t a;
	page_t *pp;

	npages = btopr(size);

	hat_unload(kas.a_hat, va, size, HAT_UNLOAD_UNLOCK);
	for (i = 0, a = va; i < npages; i++, a += PAGESIZE) {
		pp = page_find(&kvp,
		    (u_offset_t)(uintptr_t)a);
		if (pp != NULL) {
			if (!page_tryupgrade(pp)) {
				page_unlock(pp);
				pp = page_lookup(&kvp,
				    (u_offset_t)(uintptr_t)a,
				    SE_EXCL);
			}

			if (pp != NULL) {
				page_destroy(pp, 0);
			}
		}
	}

	page_unresv(npages);
	vmem_free(heap_arena, va, npages * PAGESIZE);
}

/*
 * Allocate a physically contiguous, page-aligned, wired kernel buffer
 * and return both the VA and PA.
 */
static int
ffa_buf_alloc(size_t size, caddr_t *vap, uint64_t *pap)
{
	extern page_t *page_create_io(vnode_t *, u_offset_t, uint_t,
	    uint_t, struct as *, caddr_t, ddi_dma_attr_t *);

	caddr_t va;
	pfn_t pfn;
	pgcnt_t npages;
	size_t asize;
	page_t *ppl;

	ASSERT3P(vap, !=, NULL);
	ASSERT3P(pap, !=, NULL);

	npages = btopr(size);
	ASSERT(size > 0);

	asize = npages * PAGESIZE;
	va = vmem_alloc(heap_arena, asize, VM_SLEEP);
	if (va == NULL) {
		return (ENOMEM);
	}

	if (page_resv(npages, KM_SLEEP) == 0) {
		vmem_free(heap_arena, va, asize);
		return (ENOMEM);
	}

	ppl = page_create_io(&kvp,
	    (u_offset_t)(uintptr_t)va, asize,
	    PG_EXCL | PG_WAIT | PG_PHYSCONTIG,
	    &kas, va, NULL);
	if (ppl == NULL) {
		page_unresv(npages);
		vmem_free(heap_arena, va, asize);
		return (ENOMEM);
	}

	while (ppl != NULL) {
		page_t *pp = ppl;

		page_sub(&ppl, pp);
		ASSERT(page_iolock_assert(pp));
		page_io_unlock(pp);
		page_downgrade(pp);
		hat_memload(kas.a_hat,
		    (caddr_t)(uintptr_t)pp->p_offset,
		    pp,
		    (PROT_ALL & ~PROT_USER) | HAT_NOSYNC,
		    HAT_LOAD_LOCK);
	}

	(void) memset(va, 0, size);

	pfn = hat_getpfnum(kas.a_hat, va);
	if (pfn == PFN_INVALID) {
		ffa_buf_free(va, size);
		return (EFAULT);
	}

	*vap = va;
	*pap = (uint64_t)mmu_ptob((paddr_t)pfn);
	return (0);
}

/*
 * Decode the minimum RXTX buffer size from the FFA_FEATURES return value.
 */
static size_t
ffa_rxtx_min_size(uint32_t w2)
{
	size_t specsz;

	switch (w2 & FFA_RXTX_MIN_SZ_MASK) {
	case FFA_RXTX_MIN_SZ_16K:
		specsz = 0x4000;
		break;
	case FFA_RXTX_MIN_SZ_64K:
		specsz = 0x10000;
		break;
	case FFA_RXTX_MIN_SZ_4K:	/* fallthrough */
	default:
		specsz = 0x1000;
		break;
	}

	return (MAX(specsz, MMU_PAGESIZE));
}

/*
 * Version negotiation
 */

/*
 * Negotiate the FF-A version with the SPMC (§13.2).
 *
 * FFA_VERSION is a 32-bit call: w0 = FID, w1 = our version.
 *
 * Returns the negotiated version in w0, or FFA_ERR_NOT_SUPPORTED.
 */
static int
ffa_negotiate_version(uint32_t *verp)
{
	smccc32_args_t args = {
		.w = { FFA_VERSION_FID, FFA_MY_VERSION },
	};
	int32_t ret;

	ASSERT3P(verp, !=, NULL);

	if (smccc32_call(&args) != DDI_SUCCESS) {
		return (ENOTSUP);
	}

	ret = (int32_t)args.w[0];
	if (ret == (int32_t)FFA_ERR_NOT_SUPPORTED) {
		return (ENOTSUP);
	}

	/*
	 * Bit 31 set in the return value indicates an error per the
	 * version negotiation rules (§13.2.2).
	 */
	if (ret & (1U << 31)) {
		return (EIO);
	}

	*verp = (uint32_t)ret;
	return (0);
}

/*
 * ID and feature discovery
 */

/*
 * Retrieve our endpoint ID (§13.10).
 */
static int
ffa_id_get(uint16_t *idp)
{
	smccc32_args_t args = {
		.w = { FFA_ID_GET },
	};

	ASSERT3P(idp, !=, NULL);

	if (smccc32_call(&args) != DDI_SUCCESS) {
		return (EIO);
	}

	if (args.w[0] == FFA_ERROR) {
		return (ffa_err_to_errno((int32_t)args.w[2]));
	}

	if (args.w[0] != FFA_SUCCESS32) {
		return (EIO);
	}

	*idp = (uint16_t)(args.w[2] & 0xFFFF);
	return (0);
}

/*
 * Query a feature via FFA_FEATURES (§13.3).
 *
 * For function IDs: returns properties in *w2p and *w3p.
 * For feature IDs (bit 31 clear): same.
 */
static int
ffa_features(uint32_t feature_id, uint32_t *w2p, uint32_t *w3p)
{
	smccc32_args_t args = {
		.w = { FFA_FEATURES, feature_id },
	};

	if (smccc32_call(&args) != DDI_SUCCESS) {
		return (EIO);
	}

	if (args.w[0] == FFA_ERROR) {
		return (ffa_err_to_errno((int32_t)args.w[2]));
	}

	if (args.w[0] != FFA_SUCCESS32) {
		return (EIO);
	}

	if (w2p != NULL) {
		*w2p = args.w[2];
	}

	if (w3p != NULL) {
		*w3p = args.w[3];
	}

	return (0);
}

/*
 * RXTX buffer management
 */

/*
 * Map the RXTX buffer pair with the SPMC (§13.6).
 *
 * We use the 64-bit variant (FFA_RXTX_MAP64) since we are always 64bit.
 *
 * x1 [63:0] = TX buffer PA
 * x2 [63:0] = RX buffer PA
 * x3 [5:0]  = page count (in 4k pages)
 */
static int
ffa_rxtx_map(uint64_t tx_pa, uint64_t rx_pa, uint32_t page_count)
{
	smccc64_args_t args = {
		.x = {
			FFA_RXTX_MAP64,
			tx_pa,
			rx_pa,
			page_count & FFA_RXTX_PAGE_CNT_MASK
		},
	};

	if (smccc64_call(&args) != DDI_SUCCESS) {
		return (EIO);
	}

	if ((uint32_t)args.x[0] == FFA_ERROR) {
		return (ffa_err_to_errno((int32_t)args.x[2]));
	}

	if ((uint32_t)args.x[0] != FFA_SUCCESS32) {
		return (EIO);
	}

	return (0);
}

/*
 * Release the RX buffer after reading from it (§13.5).
 *
 * Lets the secure partition manager know that the buffer is once again
 * available for its use.
 */
static int
ffa_rx_release(void)
{
	smccc32_args_t args = {
		.w = { FFA_RX_RELEASE },
	};

	if (smccc32_call(&args) != DDI_SUCCESS) {
		return (EIO);
	}

	if (args.w[0] == FFA_ERROR) {
		return (ffa_err_to_errno((int32_t)args.w[2]));
	}

	return (0);
}

/*
 * Partition discovery
 */

/*
 * Look up a partition by UUID using the register-based interface
 * (FFA_PARTITION_INFO_GET_REGS, §13.9, v1.2+).
 *
 * Descriptors are returned packed in x3-x17 (6 registers per descriptor, max
 * 2 per call).  We only need the first match.
 */
static int
ffa_partition_lookup_regs(const uuid_t *uuid, uint16_t *part_id)
{
	uint64_t uuid_lo, uuid_hi;
	uint64_t meta;
	smccc64_args_t args = {
		.x = { FFA_PARTITION_INFO_GET_REGS },
	};

	ASSERT3P(uuid, !=, NULL);
	ASSERT3P(part_id, !=, NULL);

	ffa_uuid_to_regs64(uuid, &uuid_lo, &uuid_hi);

	args.x[1] = uuid_lo;
	args.x[2] = uuid_hi;

	if (smccc64_call(&args) != DDI_SUCCESS) {
		return (EIO);
	}

	if ((uint32_t)args.x[0] == FFA_ERROR) {
		return (ffa_err_to_errno((int32_t)args.x[2]));
	}

	if ((uint32_t)args.x[0] != FFA_SUCCESS64) {
		return (EIO);
	}

	/*
	 * x2 = information metadata:
	 *   [15:0]  = last index
	 *   [31:16] = current index
	 *   [47:32] = info tag
	 *   [63:48] = descriptor size
	 */
	meta = args.x[2];
	if ((meta & 0xFFFF) == 0xFFFF) {
		return (ENOENT);	/* no partitions found */
	}

	/*
	 * First descriptor starts at x3:
	 *   [15:0] = partition ID.
	 */
	*part_id = (uint16_t)(args.x[3] & 0xFFFF);
	return (0);
}

/*
 * Look up a partition by UUID using the buffer-based interface
 * (FFA_PARTITION_INFO_GET, §13.8, v1.0+).
 *
 * UUID is packed into w1-w4 per SMCCC §5.3.  Results land in the RX
 * buffer as ffa_part_desc_t entries.  The RX buffer must be released
 * after reading.
 */
static int
ffa_partition_lookup_buf(const uuid_t *uuid, uint16_t *part_id)
{
	smccc32_args_t args = {
		.w = { FFA_PARTITION_INFO_GET },
	};
	uint32_t count;
	ffa_part_desc_t *desc;
	int ret;

	ASSERT3P(uuid, !=, NULL);
	ASSERT3P(part_id, !=, NULL);

	mutex_enter(&ffa_state.fs_rx_lock);

	ffa_uuid_to_regs32(uuid, &args.w[1], &args.w[2],
	    &args.w[3], &args.w[4]);

	if (smccc32_call(&args) != DDI_SUCCESS) {
		mutex_exit(&ffa_state.fs_rx_lock);
		return (EIO);
	}

	if (args.w[0] == FFA_ERROR) {
		ret = ffa_err_to_errno((int32_t)args.w[2]);
		mutex_exit(&ffa_state.fs_rx_lock);
		return (ret);
	}

	if (args.w[0] != FFA_SUCCESS32) {
		mutex_exit(&ffa_state.fs_rx_lock);
		return (EIO);
	}

	count = args.w[2];
	if (count == 0) {
		(void) ffa_rx_release();
		mutex_exit(&ffa_state.fs_rx_lock);
		return (ENOENT);
	}

	desc = (ffa_part_desc_t *)ffa_state.fs_rx_buf;
	*part_id = desc->fpd_id;

	(void) ffa_rx_release();
	mutex_exit(&ffa_state.fs_rx_lock);
	return (0);
}

static void
ffa_init(void)
{
	uint32_t ver;
	uint16_t vm_id;
	uint32_t feat_w2;
	size_t bufsz;
	uint64_t tx_pa, rx_pa;
	uint32_t page_count;
	int ret;

	ffa_state.fs_available = B_FALSE;

	if (!smccc_available()) {
		return;
	}

	/*
	 * Step 1: Version negotiation.
	 */
	ret = ffa_negotiate_version(&ver);
	if (ret != 0) {
		return;
	}

	if (ver < FFA_MIN_VERSION) {
		cmn_err(CE_WARN, "!ffa: firmware version %u.%u below "
		    "minimum %u.%u",
		    FFA_VERSION_MAJOR(ver), FFA_VERSION_MINOR(ver),
		    FFA_VERSION_MAJOR(FFA_MIN_VERSION),
		    FFA_VERSION_MINOR(FFA_MIN_VERSION));
		return;
	}

	ffa_state.fs_version = ver;

	/*
	 * Step 2: Get our endpoint ID.
	 */
	ret = ffa_id_get(&vm_id);
	if (ret != 0) {
		cmn_err(CE_WARN, "ffa: FFA_ID_GET failed (%d)", ret);
		return;
	}
	ffa_state.fs_vm_id = vm_id;

	/*
	 * Step 3: Probe RXTX buffer requirements via FFA_FEATURES.
	 */
	ret = ffa_features(FFA_RXTX_MAP64, &feat_w2, NULL);
	if (ret != 0) {
		cmn_err(CE_WARN, "ffa: FFA_FEATURES(RXTX_MAP) "
		    "failed (%d)", ret);
		return;
	}

	bufsz = ffa_rxtx_min_size(feat_w2);
	ffa_state.fs_rxtx_bufsz = bufsz;

	/*
	 * The RXTX_MAP page count is in 4K translation granules
	 * per the spec (§13.6), regardless of the OS page size.
	 */
	page_count = bufsz / 0x1000;

	/*
	 * Step 4: Allocate TX and RX buffers and map them with the SPMC.
	 */
	ret = ffa_buf_alloc(bufsz, &ffa_state.fs_tx_buf, &tx_pa);
	if (ret != 0) {
		cmn_err(CE_WARN, "ffa: TX buffer alloc failed (%d)", ret);
		return;
	}

	ret = ffa_buf_alloc(bufsz, &ffa_state.fs_rx_buf, &rx_pa);
	if (ret != 0) {
		cmn_err(CE_WARN, "ffa: RX buffer alloc failed (%d)", ret);
		ffa_buf_free(ffa_state.fs_tx_buf, bufsz);
		ffa_state.fs_tx_buf = NULL;
		return;
	}

	mutex_init(&ffa_state.fs_tx_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&ffa_state.fs_rx_lock, NULL, MUTEX_DEFAULT, NULL);

	ret = ffa_rxtx_map(tx_pa, rx_pa, page_count);
	if (ret != 0) {
		cmn_err(CE_WARN, "ffa: FFA_RXTX_MAP failed (%d)", ret);
		mutex_destroy(&ffa_state.fs_tx_lock);
		mutex_destroy(&ffa_state.fs_rx_lock);
		ffa_buf_free(ffa_state.fs_rx_buf, bufsz);
		ffa_buf_free(ffa_state.fs_tx_buf, bufsz);
		ffa_state.fs_tx_buf = NULL;
		ffa_state.fs_rx_buf = NULL;
		return;
	}

	ffa_state.fs_available = B_TRUE;
}

static void
ffa_fini(void)
{
	if (ffa_state.fs_available) {
		mutex_destroy(&ffa_state.fs_tx_lock);
		mutex_destroy(&ffa_state.fs_rx_lock);
		ffa_buf_free(ffa_state.fs_rx_buf, ffa_state.fs_rxtx_bufsz);
		ffa_buf_free(ffa_state.fs_tx_buf, ffa_state.fs_rxtx_bufsz);
	}

	memset(&ffa_state, 0, sizeof (ffa_state));
}

/*
 * Public interface
 */

boolean_t
ffa_available(void)
{
	return (ffa_state.fs_available);
}

uint32_t
ffa_version(void)
{
	return (ffa_state.fs_version);
}

/*
 * Look up a Secure partition by UUID, returning its 16-bit partition ID.
 *
 * Tries the register-based path (FFA_PARTITION_INFO_GET_REGS, v1.2+)
 * first for efficiency, then falls back to the buffer-based path
 * (FFA_PARTITION_INFO_GET) for older SPMC versions.
 */
int
ffa_partition_lookup(const uuid_t *uuid, uint16_t *part_id)
{
	int ret;

	ASSERT3P(uuid, !=, NULL);
	ASSERT3P(part_id, !=, NULL);

	if (!ffa_state.fs_available) {
		return (ENXIO);
	}

	if (ffa_state.fs_version >= FFA_VERSION_1_2) {
		ret = ffa_partition_lookup_regs(uuid, part_id);
		if (ret != ENOTSUP) {
			/* non-obviously, this includes the success path */
			return (ret);
		}

		/* fall through to buffer-based path */
	}

	return (ffa_partition_lookup_buf(uuid, part_id));
}

/*
 * Raw DIRECT_REQ2 call with full register return.
 *
 * Issues FFA_MSG_SEND_DIRECT_REQ2 to the specified partition and handles
 * YIELD/INTERRUPT retry via FFA_RUN.  On completion, the full register
 * set (x0-x17) is written to out_regs[0..17] without interpretation.
 * The caller checks out_regs[0] (response FID) to determine success
 * or failure:
 *
 *   FFA_MSG_SEND_DIRECT_RESP2  - normal response, payload in x4-x17
 *   FFA_SUCCESS32/64           - completion without payload
 *   FFA_ERROR                  - firmware error, code in x2
 *
 * in_args provides up to nargs payload values for x4-x(4+nargs-1).
 * out_regs must point to FFA_DIRECT_REQ2_NREGS (18) uint64_t values.
 *
 * Returns 0 on completion (caller interprets out_regs[0]), or an errno
 * if the SMCCC transport itself failed or FF-A is unavailable.
 *
 * This interface is used internally by ffa_direct_req2 and is exposed for
 * use by advanced callers, such as FFH.
 */
int
ffa_direct_req2_raw(uint16_t part_id, const uuid_t *uuid,
    const uint64_t *in_args, uint64_t *out_regs, uint_t nargs)
{
	const hrtime_t ffa_retry_warn_ns =
	    (hrtime_t)ffa_retry_warn_seconds * NANOSEC;
	smccc64_args_t sargs = {
		.x = {
			FFA_MSG_SEND_DIRECT_REQ2,
			((uint64_t)ffa_state.fs_vm_id << 16) |
			(uint64_t)part_id
		},
	};
	hrtime_t start, now;
	boolean_t warned;
	uint32_t fid;
	uint_t i;

	ASSERT3P(uuid, !=, NULL);
	ASSERT3P(out_regs, !=, NULL);
	ASSERT(nargs == 0 || in_args != NULL);

	if (!ffa_state.fs_available) {
		return (ENXIO);
	}

	if (nargs > FFA_DIRECT_REQ2_NARGS) {
		return (EINVAL);
	}

	/*
	 * x2-x3: service UUID in FF-A 64-bit encoding.
	 */
	ffa_uuid_to_regs64(uuid, &sargs.x[2], &sargs.x[3]);

	/*
	 * x4-x17: caller-supplied arguments.
	 */
	for (i = 0; i < nargs; i++) {
		sargs.x[4 + i] = in_args[i];
	}

	if (ffa_retry_warn_ns > 0) {
		start = gethrtime();
		warned = B_FALSE;
	}

	if (smccc64_call(&sargs) != DDI_SUCCESS) {
		return (EIO);
	}

	for (;;) {
		fid = (uint32_t)sargs.x[0];

		if (fid != FFA_INTERRUPT && fid != FFA_YIELD64) {
			break;
		}

		/*
		 * If configured, when a synchronous call has run for more than
		 * a specified number of seconds we warn once.
		 */
		if (ffa_retry_warn_ns > 0) {
			now = gethrtime();
			if (!warned && (now - start) >= ffa_retry_warn_ns) {
				cmn_err(CE_NOTE, "ffa: DIRECT_REQ2 to "
				    "endpoint 0x%x retrying (%lld seconds)",
				    (uint_t)part_id,
				    (longlong_t)((now - start) / NANOSEC));
				warned = B_TRUE;
			}
		}

		/*
		 * The spec requires FFA_RUN to resume a preempted
		 * (INTERRUPT) or blocked (YIELD) partition.  Since
		 * the original call used a 64-bit FID, we must use
		 * the SMC64 variant of FFA_RUN per §11.1.2.
		 */
		sargs = (smccc64_args_t){
			.x = { FFA_RUN64, ((uint64_t)part_id << 16) }
		};

		if (smccc64_call(&sargs) != DDI_SUCCESS) {
			return (EIO);
		}
	}

	for (i = 0; i < FFA_DIRECT_REQ2_NREGS; i++) {
		out_regs[i] = sargs.x[i];
	}

	return (0);
}

/*
 * Send a direct request to a secure partition via FFA_MSG_SEND_DIRECT_REQ2
 * (§15.4).
 *
 * This is a 64-bit call.  The caller provides:
 *   part_id  - target partition ID
 *   uuid     - service UUID (packed into x2-x3)
 *   args     - up to FFA_DIRECT_REQ2_NARGS uint64_t values (x4-x17)
 *   results  - receives up to FFA_DIRECT_REQ2_NARGS values on success
 *   nargs    - number of arg/result registers to use (0..14)
 *
 * Handles FFA_YIELD and FFA_INTERRUPT by retrying.  The retry loop is
 * unbounded per the FF-A spec.
 *
 * Returns 0 on success, errno on failure.
 */
int
ffa_direct_req2(uint16_t part_id, const uuid_t *uuid,
    const uint64_t *args, uint64_t *results, uint_t nargs)
{
	uint64_t raw[FFA_DIRECT_REQ2_NREGS];
	uint32_t fid;
	uint_t i;
	int ret;

	ret = ffa_direct_req2_raw(part_id, uuid, args, raw, nargs);
	if (ret != 0) {
		return (ret);
	}

	fid = (uint32_t)raw[0];

	if (fid == FFA_ERROR) {
		return (ffa_err_to_errno((int32_t)raw[2]));
	}

	/*
	 * FFA_SUCCESS indicates completion without a response payload.
	 */
	if (fid == FFA_SUCCESS64 || fid == FFA_SUCCESS32) {
		return (0);
	}

	if (fid != (uint32_t)FFA_MSG_SEND_DIRECT_RESP2) {
		cmn_err(CE_WARN, "ffa: DIRECT_REQ2 unexpected FID 0x%x", fid);
		return (EIO);
	}

	/*
	 * Copy result registers x4-x17 back to the caller.
	 */
	if (results != NULL) {
		for (i = 0; i < nargs; i++) {
			results[i] = raw[4 + i];
		}
	}

	return (0);
}

static struct modlmisc modlmisc = {
	&mod_miscops,
	"Arm Functional Fixed Hardware (DEN0048)",
};

static struct modlinkage modlinkage = {
	.ml_rev = MODREV_1,
	.ml_linkage = { &modlmisc, NULL }
};

int
_init(void)
{
	int err;

	if ((err = mod_install(&modlinkage)) != 0) {
		return (err);
	}

	ffa_init();
	return (err);
}

int
_fini(void)
{
	int err;

	if ((err = mod_remove(&modlinkage))) {
		return (err);
	}

	ffa_fini();
	return (err);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
