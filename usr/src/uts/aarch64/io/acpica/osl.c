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
 * Copyright 2018 Joyent, Inc.
 * Copyright 2019 Western Digital Corporation
 * Copyright 2026 Michael van der Westhuizen
 */
/*
 * Copyright (c) 2009-2010, Intel Corporation.
 * All rights reserved.
 */
/*
 * ACPI CA OSL for illumos aarch64
 */

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/pci.h>
#include <sys/kobj.h>
#include <sys/taskq.h>
#include <sys/strlog.h>
#include <sys/note.h>
#include <sys/promif.h>

#include <sys/smp_impldefs.h>

#include <sys/disp.h>
#include <sys/cpuvar.h>

#include <sys/efi.h>
#include <sys/acpi/acconfig.h>
#include <sys/acpi/acpi.h>

#include <sys/acpi/accommon.h>
#include <sys/acpica.h>

#define	MAX_DAT_FILE_SIZE	(64*1024)

static ACPI_STATUS acpica_set_devinfo(ACPI_HANDLE, dev_info_t *);
static ACPI_STATUS acpica_unset_devinfo(ACPI_HANDLE);
static void acpica_devinfo_handler(ACPI_HANDLE, void *);

/*
 * Event queue vars
 */
int acpica_eventq_init = 0;
ddi_taskq_t *osl_eventq[OSL_EC_BURST_HANDLER+1];

/*
 * Priorities relative to minclsyspri that each taskq
 * run at; OSL_NOTIFY_HANDLER needs to run at a higher
 * priority than OSL_GPE_HANDLER.  There's an implicit
 * assumption that no priority here results in exceeding
 * maxclsyspri.
 * Note: these initializations need to match the order of
 * ACPI_EXECUTE_TYPE.
 */
int osl_eventq_pri_delta[OSL_EC_BURST_HANDLER+1] = {
	0,	/* OSL_GLOBAL_LOCK_HANDLER */
	2,	/* OSL_NOTIFY_HANDLER */
	0,	/* OSL_GPE_HANDLER */
	0,	/* OSL_DEBUGGER_THREAD */
	0,	/* OSL_EC_POLL_HANDLER */
	0	/* OSL_EC_BURST_HANDLER */
};

static const char acpi_table_path[] = "/boot/acpi/tables/";

/* features supported by ACPICA and ACPI device configuration. */
uint64_t acpica_core_features = ACPI_FEATURE_OSI_MODULE;
static uint64_t acpica_devcfg_features = 0;

/* Platform-wide _OSC granted capabilities (\_SB._OSC DWORD 2). */
static uint64_t acpica_plat_osc_caps = 0;

/* for use by acpippm for S3 */
int acpica_use_safe_delay = 0;

/* CPU mapping data */
struct cpu_map_item {
	processorid_t	cpu_id;
	UINT32		proc_id;
	UINT64		mpidr;
	ACPI_HANDLE	obj;
};

kmutex_t cpu_map_lock;
static struct cpu_map_item **cpu_map = NULL;
static int cpu_map_count_max = 0;
static int cpu_map_count = 0;

/* buffer for AcpiOsVprintf() */
#define	ACPI_OSL_PR_BUFLEN	1024
static char *acpi_osl_pr_buffer = NULL;
static int acpi_osl_pr_buflen;

/*
 *
 */
static void
discard_event_queues()
{
	int	i;

	/*
	 * destroy event queues
	 */
	for (i = OSL_GLOBAL_LOCK_HANDLER; i <= OSL_EC_BURST_HANDLER; i++) {
		if (osl_eventq[i]) {
			ddi_taskq_destroy(osl_eventq[i]);
		}
	}
}


/*
 *
 */
static ACPI_STATUS
init_event_queues()
{
	char	namebuf[32];
	int	i, error = 0;

	/*
	 * Initialize event queues
	 */

	/* Always allocate only 1 thread per queue to force FIFO execution */
	for (i = OSL_GLOBAL_LOCK_HANDLER; i <= OSL_EC_BURST_HANDLER; i++) {
		snprintf(namebuf, 32, "ACPI%d", i);
		osl_eventq[i] = ddi_taskq_create(NULL, namebuf, 1,
		    osl_eventq_pri_delta[i] + minclsyspri, 0);
		if (osl_eventq[i] == NULL) {
			error++;
		}
	}

	if (error != 0) {
		discard_event_queues();
#ifdef	DEBUG
		cmn_err(CE_WARN, "!acpica: could not initialize event queues");
#endif
		return (AE_ERROR);
	}

	acpica_eventq_init = 1;
	return (AE_OK);
}

/*
 * One-time initialization of OSL layer
 */
ACPI_STATUS
AcpiOsInitialize(void)
{
	/*
	 * Allocate buffer for AcpiOsVprintf() here to avoid
	 * kmem_alloc()/kmem_free() at high PIL
	 */
	acpi_osl_pr_buffer = kmem_alloc(ACPI_OSL_PR_BUFLEN, KM_SLEEP);
	if (acpi_osl_pr_buffer != NULL) {
		acpi_osl_pr_buflen = ACPI_OSL_PR_BUFLEN;
	}

	return (AE_OK);
}

/*
 * One-time shut-down of OSL layer
 */
ACPI_STATUS
AcpiOsTerminate(void)
{
	if (acpi_osl_pr_buffer != NULL) {
		kmem_free(acpi_osl_pr_buffer, acpi_osl_pr_buflen);
	}

	discard_event_queues();
	return (AE_OK);
}


ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer()
{
	ACPI_PHYSICAL_ADDRESS Address;

	/*
	 * For UEFI firmware, the root pointer is stored in the UEFI system
	 * table as a configuration table. The loader processes this table
	 * and passes the variable to unix in the boot environment, which
	 * is then stored as a boot property named `acpi-root-tab'.
	 */
	Address = ddi_prop_get_int64(DDI_DEV_T_ANY, ddi_root_node(),
	    DDI_PROP_DONTPASS, "acpi-root-tab", 0);

	return (Address);
}

ACPI_STATUS
AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal __unused,
    ACPI_STRING *NewVal)
{
	*NewVal = 0;
	return (AE_OK);
}

static void
acpica_strncpy(char *dest, const char *src, int len)
{
	/*LINTED*/
	while ((*dest++ = *src++) && (--len > 0)) {
		/* copy the string */;
	}

	*dest = '\0';
}

ACPI_STATUS
AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable,
    ACPI_TABLE_HEADER **NewTable)
{
	char signature[5];
	char oemid[7];
	char oemtableid[9];
	struct _buf *file;
	char *buf1, *buf2;
	int count;
	char acpi_table_loc[128];

	acpica_strncpy(signature, ExistingTable->Signature, 4);
	acpica_strncpy(oemid, ExistingTable->OemId, 6);
	acpica_strncpy(oemtableid, ExistingTable->OemTableId, 8);

	/* File name format is "signature_oemid_oemtableid.dat" */
	(void) strcpy(acpi_table_loc, acpi_table_path);
	(void) strcat(acpi_table_loc, signature); /* for example, DSDT */
	(void) strcat(acpi_table_loc, "_");
	(void) strcat(acpi_table_loc, oemid); /* for example, IntelR */
	(void) strcat(acpi_table_loc, "_");
	(void) strcat(acpi_table_loc, oemtableid); /* for example, AWRDACPI */
	(void) strcat(acpi_table_loc, ".dat");

	file = kobj_open_file(acpi_table_loc);
	if (file == (struct _buf *)-1) {
		*NewTable = 0;
		return (AE_OK);
	} else {
		buf1 = (char *)kmem_alloc(MAX_DAT_FILE_SIZE, KM_SLEEP);
		/*
		 * Read the whole buffer so a file that fills it is
		 * detected below as too big, rather than being
		 * silently truncated. A file exactly MAX_DAT_FILE_SIZE
		 * is rejected too: a full read is indistinguishable
		 * from a larger file, but the maximum read size (64k)
		 * is large enough to make this condition unlikely.
		 */
		count = kobj_read_file(file, buf1, MAX_DAT_FILE_SIZE, 0);
		if (count >= MAX_DAT_FILE_SIZE) {
			cmn_err(CE_WARN, "!acpica: table %s file size too big",
			    acpi_table_loc);
			*NewTable = 0;
		} else {
			buf2 = (char *)kmem_alloc(count, KM_SLEEP);
			(void) memcpy(buf2, buf1, count);
			*NewTable = (ACPI_TABLE_HEADER *)buf2;
			cmn_err(CE_NOTE, "!acpica: replacing table: %s",
			    acpi_table_loc);
		}
	}
	kobj_close_file(file);
	kmem_free(buf1, MAX_DAT_FILE_SIZE);

	return (AE_OK);
}

ACPI_STATUS
AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable,
    ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
	return (AE_SUPPORT);
}

/*
 * ACPI semaphore implementation
 */
typedef struct {
	kmutex_t	mutex;
	kcondvar_t	cv;
	uint32_t	available;
	uint32_t	initial;
	uint32_t	maximum;
} acpi_sema_t;

/*
 *
 */
void
acpi_sema_init(acpi_sema_t *sp, unsigned max, unsigned count)
{
	mutex_init(&sp->mutex, NULL, MUTEX_DRIVER, NULL);
	cv_init(&sp->cv, NULL, CV_DRIVER, NULL);
	/* no need to enter mutex here at creation */
	sp->available = count;
	sp->initial = count;
	sp->maximum = max;
}

/*
 *
 */
void
acpi_sema_destroy(acpi_sema_t *sp)
{
	cv_destroy(&sp->cv);
	mutex_destroy(&sp->mutex);
}

/*
 *
 */
ACPI_STATUS
acpi_sema_p(acpi_sema_t *sp, unsigned count, uint16_t wait_time)
{
	ACPI_STATUS rv = AE_OK;
	clock_t deadline;

	mutex_enter(&sp->mutex);

	if (sp->available >= count) {
		/*
		 * Enough units available, no blocking
		 */
		sp->available -= count;
		mutex_exit(&sp->mutex);
		return (rv);
	} else if (wait_time == 0) {
		/*
		 * Not enough units available and timeout
		 * specifies no blocking
		 */
		rv = AE_TIME;
		mutex_exit(&sp->mutex);
		return (rv);
	}

	/*
	 * Not enough units available and timeout specifies waiting
	 */
	if (wait_time != ACPI_WAIT_FOREVER) {
		deadline = ddi_get_lbolt() +
		    (clock_t)drv_usectohz(wait_time * 1000);
	}

	do {
		if (wait_time == ACPI_WAIT_FOREVER) {
			cv_wait(&sp->cv, &sp->mutex);
		} else if (cv_timedwait(&sp->cv, &sp->mutex, deadline) < 0) {
			rv = AE_TIME;
			break;
		}
	} while (sp->available < count);

	/* if we dropped out of the wait with AE_OK, we got the units */
	if (rv == AE_OK) {
		sp->available -= count;
	}

	mutex_exit(&sp->mutex);
	return (rv);
}

/*
 *
 */
void
acpi_sema_v(acpi_sema_t *sp, unsigned count)
{
	mutex_enter(&sp->mutex);
	sp->available += count;
	cv_broadcast(&sp->cv);
	mutex_exit(&sp->mutex);
}


ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
    ACPI_HANDLE *OutHandle)
{
	acpi_sema_t *sp;

	if ((OutHandle == NULL) || (InitialUnits > MaxUnits)) {
		return (AE_BAD_PARAMETER);
	}

	sp = (acpi_sema_t *)kmem_alloc(sizeof (acpi_sema_t), KM_SLEEP);
	acpi_sema_init(sp, MaxUnits, InitialUnits);
	*OutHandle = (ACPI_HANDLE)sp;
	return (AE_OK);
}


ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_HANDLE Handle)
{
	if (Handle == NULL) {
		return (AE_BAD_PARAMETER);
	}

	acpi_sema_destroy((acpi_sema_t *)Handle);
	kmem_free((void *)Handle, sizeof (acpi_sema_t));
	return (AE_OK);
}

ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_HANDLE Handle, UINT32 Units, UINT16 Timeout)
{
	if ((Handle == NULL) || (Units < 1)) {
		return (AE_BAD_PARAMETER);
	}

	return (acpi_sema_p((acpi_sema_t *)Handle, Units, Timeout));
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_HANDLE Handle, UINT32 Units)
{
	if ((Handle == NULL) || (Units < 1)) {
		return (AE_BAD_PARAMETER);
	}

	acpi_sema_v((acpi_sema_t *)Handle, Units);
	return (AE_OK);
}

ACPI_STATUS
AcpiOsCreateLock(ACPI_HANDLE *OutHandle)
{
	kmutex_t *mp;

	if (OutHandle == NULL) {
		return (AE_BAD_PARAMETER);
	}

	mp = (kmutex_t *)kmem_alloc(sizeof (kmutex_t), KM_SLEEP);
	mutex_init(mp, NULL, MUTEX_DRIVER, NULL);
	*OutHandle = (ACPI_HANDLE)mp;
	return (AE_OK);
}

void
AcpiOsDeleteLock(ACPI_HANDLE Handle)
{
	if (Handle == NULL) {
		return;
	}

	mutex_destroy((kmutex_t *)Handle);
	kmem_free((void *)Handle, sizeof (kmutex_t));
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock(ACPI_HANDLE Handle)
{
	if (Handle == NULL) {
		return (AE_BAD_PARAMETER);
	}

	if (curthread == CPU->cpu_idle_thread) {
		while (!mutex_tryenter((kmutex_t *)Handle)) {
			/* spin */;
		}
	} else {
		mutex_enter((kmutex_t *)Handle);
	}

	return (AE_OK);
}

void
AcpiOsReleaseLock(ACPI_HANDLE Handle, ACPI_CPU_FLAGS Flags __unused)
{
	mutex_exit((kmutex_t *)Handle);
}


void *
AcpiOsAllocate(ACPI_SIZE Size)
{
	ACPI_SIZE *tmp_ptr;

	Size += sizeof (Size);
	tmp_ptr = (ACPI_SIZE *)kmem_zalloc(Size, KM_SLEEP);
	*tmp_ptr++ = Size;
	return (tmp_ptr);
}

void
AcpiOsFree(void *Memory)
{
	ACPI_SIZE	size, *tmp_ptr;

	tmp_ptr = (ACPI_SIZE *)Memory;
	tmp_ptr -= 1;
	size = *tmp_ptr;
	kmem_free(tmp_ptr, size);
}

void *
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Size)
{
	return (psm_map_phys((paddr_t)PhysicalAddress,
	    (size_t)Size, PROT_READ|PROT_WRITE));
}

void
AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size)
{
	psm_unmap_phys((caddr_t)LogicalAddress, (size_t)Size);
}

ACPI_STATUS
AcpiOsGetPhysicalAddress(void *LogicalAddress __unused,
    ACPI_PHYSICAL_ADDRESS *PhysicalAddress __unused)
{
	/* UNIMPLEMENTED: not invoked by ACPI CA code */
	return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiOsInstallInterruptHandler(UINT32 InterruptNumber,
    ACPI_OSD_HANDLER ServiceRoutine __unused,
    void *Context __unused)
{
	cmn_err(CE_WARN, "!AcpiOsInstallInterruptHandler: called unexpectedly "
	    "for interrupt %u", InterruptNumber);
	return (AE_ERROR);
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber,
    ACPI_OSD_HANDLER ServiceRoutine __unused)
{
	cmn_err(CE_WARN, "!AcpiOsRemoveInterruptHandler: called unexpectedly "
	    "for interrupt %u", InterruptNumber);
	return (AE_ERROR);
}

ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{
	/*
	 * ACPI CA doesn't care what actual value is returned as long
	 * as it is non-zero and unique to each existing thread.
	 * ACPI CA assumes that thread ID is castable to a pointer,
	 * so we use the current thread pointer.
	 */
	return (ACPI_CAST_PTHREAD_T((uintptr_t)curthread));
}

/*
 *
 */
ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK  Function,
    void *Context)
{

	if (!acpica_eventq_init) {
		/*
		 * Create taskqs for event handling
		 */
		if (init_event_queues() != AE_OK) {
			return (AE_ERROR);
		}
	}

	if (ddi_taskq_dispatch(osl_eventq[Type], Function, Context,
	    DDI_NOSLEEP) == DDI_FAILURE) {
#ifdef	DEBUG
		cmn_err(CE_WARN, "!acpica: unable to dispatch event");
#endif
		return (AE_ERROR);
	}

	return (AE_OK);

}


void
AcpiOsWaitEventsComplete(void)
{
	int	i;

	/*
	 * Wait for event queues to be empty.
	 */
	for (i = OSL_GLOBAL_LOCK_HANDLER; i <= OSL_EC_BURST_HANDLER; i++) {
		if (osl_eventq[i] != NULL) {
			ddi_taskq_wait(osl_eventq[i]);
		}
	}
}

void
AcpiOsSleep(ACPI_INTEGER Milliseconds)
{
	/*
	 * During kernel startup, before the first tick interrupt
	 * has taken place, we can't call delay; very late in
	 * kernel shutdown or suspend/resume, clock interrupts
	 * are blocked, so delay doesn't work then either.
	 * So we busy wait if lbolt == 0 (kernel startup)
	 * or if acpica_use_safe_delay has been set to a
	 * non-zero value.
	 */
	if ((ddi_get_lbolt() == 0) || acpica_use_safe_delay) {
		drv_usecwait(Milliseconds * 1000);
	} else {
		delay(drv_usectohz(Milliseconds * 1000));
	}
}

void
AcpiOsStall(UINT32 Microseconds)
{
	drv_usecwait(Microseconds);
}

/*
 *
 */
ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width)
{
	cmn_err(CE_WARN, "!AcpiOsReadPort: %lx %u unavailable",
	    (long)Address, Width);
	return (AE_ERROR);
}

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
	cmn_err(CE_WARN, "!AcpiOsWritePort: %lx %u unavailable",
	    (long)Address, Width);
	return (AE_ERROR);
}

/*
 *
 */

#define	OSL_RW(ptr, val, type, rw) \
	{ if (rw) *((type *)(ptr)) = *((type *) val); \
	    else *((type *) val) = *((type *)(ptr)); }


static void
osl_rw_memory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value,
    UINT32 Width, int write)
{
	size_t	maplen = Width / 8;
	caddr_t	ptr;

	ptr = psm_map_phys((paddr_t)Address, maplen,
	    PROT_READ|PROT_WRITE);

	switch (maplen) {
	case 1:
		OSL_RW(ptr, Value, uint8_t, write);
		break;
	case 2:
		OSL_RW(ptr, Value, uint16_t, write);
		break;
	case 4:
		OSL_RW(ptr, Value, uint32_t, write);
		break;
	case 8:
		OSL_RW(ptr, Value, uint64_t, write);
		break;
	default:
		cmn_err(CE_WARN, "!osl_rw_memory: invalid size %d",
		    Width);
		break;
	}

	psm_unmap_phys(ptr, maplen);
}

ACPI_STATUS
AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address,
    UINT64 *Value, UINT32 Width)
{
	osl_rw_memory(Address, Value, Width, 0);
	return (AE_OK);
}

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address,
    UINT64 Value, UINT32 Width)
{
	osl_rw_memory(Address, &Value, Width, 1);
	return (AE_OK);
}

/*
 * Return bus/dev/fn for PCI dip (note: not the parent "pci" node).
 */
static int
acpica_get_bdf(dev_info_t *dip, int *bus, int *device, int *func)
{
	pci_regspec_t *pci_rp;
	int len;

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "reg", (int **)&pci_rp, (uint_t *)&len) != DDI_SUCCESS) {
		return (-1);
	}

	if (len < (sizeof (pci_regspec_t) / sizeof (int))) {
		ddi_prop_free(pci_rp);
		return (-1);
	}
	if (bus != NULL) {
		*bus = (int)PCI_REG_BUS_G(pci_rp->pci_phys_hi);
	}
	if (device != NULL) {
		*device = (int)PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	}
	if (func != NULL) {
		*func = (int)PCI_REG_FUNC_G(pci_rp->pci_phys_hi);
	}
	ddi_prop_free(pci_rp);
	return (0);
}

/*
 * Called with ACPI_HANDLEs for both a PCI Config Space
 * OpRegion and (what ACPI CA thinks is) the PCI device
 * to which this ConfigSpace OpRegion belongs.
 *
 * We get the parent of the OpRegion, which must be a PCI
 * node, fetch the associated devinfo node and snag the
 * b/d/f from it.
 */
void
AcpiOsDerivePciId(ACPI_HANDLE rhandle __unused, ACPI_HANDLE chandle,
    ACPI_PCI_ID **PciId)
{
	ACPI_HANDLE handle;
	dev_info_t *dip;
	int bus, device, func;

	/*
	 * Get the OpRegion's parent
	 */
	if (AcpiGetParent(chandle, &handle) != AE_OK) {
		return;
	}

	/*
	 * If we've mapped the ACPI node to the devinfo
	 * tree, use the devinfo reg property
	 */
	if (ACPI_SUCCESS(acpica_get_devinfo(handle, &dip)) &&
	    (acpica_get_bdf(dip, &bus, &device, &func) >= 0)) {
		(*PciId)->Bus = bus;
		(*PciId)->Device = device;
		(*PciId)->Function = func;
	}
}


BOOLEAN
AcpiOsReadable(void *Pointer __unused, ACPI_SIZE Length __unused)
{
	/* Always says yes; all mapped memory assumed readable */
	return (1);
}

BOOLEAN
AcpiOsWritable(void *Pointer __unused, ACPI_SIZE Length __unused)
{
	/* Always says yes; all mapped memory assumed writable */
	return (1);
}

UINT64
AcpiOsGetTimer(void)
{
	/* gethrtime() returns 1nS resolution; convert to 100nS granules */
	return ((gethrtime() + 50) / 100);
}

static struct AcpiOSIFeature_s {
	uint64_t	control_flag;
	const char	*feature_name;
} AcpiOSIFeatures[] = {
	{ ACPI_FEATURE_OSI_MODULE,	"Module Device" },
	{ 0,				"Processor Device" }
};

ACPI_STATUS
AcpiOsValidateInterface(char *feature)
{
	int i;

	ASSERT3P(feature, !=, NULL);
	for (i = 0; i < sizeof (AcpiOSIFeatures) / sizeof (AcpiOSIFeatures[0]);
	    i++) {
		if (strcmp(feature, AcpiOSIFeatures[i].feature_name) != 0) {
			continue;
		}
		/* Check whether required core features are available. */
		if (AcpiOSIFeatures[i].control_flag != 0 &&
		    acpica_get_core_feature(AcpiOSIFeatures[i].control_flag) !=
		    AcpiOSIFeatures[i].control_flag) {
			break;
		}
		/* Feature supported. */
		return (AE_OK);
	}

	return (AE_SUPPORT);
}

ACPI_STATUS
AcpiOsValidateAddress(UINT8 spaceid __unused,
    ACPI_PHYSICAL_ADDRESS addr __unused, ACPI_SIZE length __unused)
{
	return (AE_OK);
}

ACPI_STATUS
AcpiOsSignal(UINT32 Function __unused, void *Info __unused)
{
	/* FUTUREWORK: debugger support */
	cmn_err(CE_NOTE, "!OsSignal unimplemented");
	return (AE_OK);
}

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf(const char *Format, ...)
{
	va_list ap;

	va_start(ap, Format);
	AcpiOsVprintf(Format, ap);
	va_end(ap);
}

ACPI_STATUS
AcpiOsEnterSleep(UINT8 SleepState __unused,
    UINT32 Rega __unused, UINT32 Regb __unused)
{
	return (AE_OK);
}

/*
 * When != 0, sends output to console
 * Patchable with kmdb or /etc/system.
 */
int acpica_console_out = 0;

#define	ACPICA_OUTBUF_LEN	160
char	acpica_outbuf[ACPICA_OUTBUF_LEN];
int	acpica_outbuf_offset;

/*
 *
 */
static void
acpica_pr_buf(char *buf)
{
	char c, *bufp, *outp;
	int	out_remaining;

	/*
	 * copy the supplied buffer into the output buffer
	 * when we hit a '\n' or overflow the output buffer,
	 * output and reset the output buffer
	 */
	bufp = buf;
	outp = acpica_outbuf + acpica_outbuf_offset;
	out_remaining = ACPICA_OUTBUF_LEN - acpica_outbuf_offset - 1;
	while (c = *bufp++) {
		*outp++ = c;
		if (c == '\n' || --out_remaining == 0) {
			*outp = '\0';
			switch (acpica_console_out) {
			case 1:
				printf("%s", acpica_outbuf);
				break;
			case 2:
				prom_printf("%s", acpica_outbuf);
				break;
			case 0:
			default:
				(void) strlog(0, 0, 0,
				    SL_CONSOLE | SL_NOTE | SL_LOGONLY,
				    "%s", acpica_outbuf);
				break;
			}
			acpica_outbuf_offset = 0;
			outp = acpica_outbuf;
			out_remaining = ACPICA_OUTBUF_LEN - 1;
		}
	}

	acpica_outbuf_offset = outp - acpica_outbuf;
}

void
AcpiOsVprintf(const char *Format, va_list Args)
{

	/*
	 * If AcpiOsInitialize() failed to allocate a string buffer,
	 * resort to vprintf().
	 */
	if (acpi_osl_pr_buffer == NULL) {
		vprintf(Format, Args);
		return;
	}

	/*
	 * It is possible that a very long debug output statement will
	 * be truncated; this is silently ignored.
	 */
	(void) vsnprintf(acpi_osl_pr_buffer, acpi_osl_pr_buflen, Format, Args);
	acpica_pr_buf(acpi_osl_pr_buffer);
}

void
AcpiOsRedirectOutput(void *Destination __unused)
{
	/* FUTUREWORK: debugger support */
#ifdef	DEBUG
	cmn_err(CE_WARN, "!acpica: AcpiOsRedirectOutput called");
#endif
}


UINT32
AcpiOsGetLine(char *Buffer __unused, UINT32 len __unused,
    UINT32 *BytesRead __unused)
{
	/* FUTUREWORK: debugger support */
	return (0);
}

static ACPI_STATUS
acpica_crs_cb(ACPI_RESOURCE *rp, void *context)
{
	int	*busno = context;

	if (rp->Data.Address.ProducerConsumer == 1) {
		return (AE_OK);
	}

	switch (rp->Type) {
	case ACPI_RESOURCE_TYPE_ADDRESS16:
		if (rp->Data.Address16.Address.AddressLength == 0) {
			break;
		}
		if (rp->Data.Address16.ResourceType != ACPI_BUS_NUMBER_RANGE) {
			break;
		}

		*busno = rp->Data.Address16.Address.Minimum;
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS32:
		if (rp->Data.Address32.Address.AddressLength == 0) {
			break;
		}
		if (rp->Data.Address32.ResourceType != ACPI_BUS_NUMBER_RANGE) {
			break;
		}

		*busno = rp->Data.Address32.Address.Minimum;
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS64:
		if (rp->Data.Address64.Address.AddressLength == 0) {
			break;
		}
		if (rp->Data.Address64.ResourceType != ACPI_BUS_NUMBER_RANGE) {
			break;
		}

		*busno = (int)rp->Data.Address64.Address.Minimum;
		break;

	default:
		break;
	}

	return (AE_OK);
}

/*
 * Retrieve the bus number for a root bus.
 *
 * _CRS (Current Resource Setting) holds the bus number as set in
 * PCI configuration, this may differ from _BBN and is a more reliable
 * indicator of what the bus number is.
 */
ACPI_STATUS
acpica_get_busno(ACPI_HANDLE hdl, int *busno)
{
	ACPI_STATUS	rv;
	int		bus = -1;
	int		bbn;

	if (ACPI_FAILURE(rv = acpica_eval_int(hdl, "_BBN", &bbn))) {
		return (rv);
	}

	(void) AcpiWalkResources(hdl, "_CRS", acpica_crs_cb, &bus);

	*busno = bus == -1 ? bbn : bus;

	return (AE_OK);
}

ACPI_STATUS
acpica_eval_int(ACPI_HANDLE dev, char *method, int *rint)
{
	ACPI_STATUS status;
	ACPI_BUFFER rb;
	ACPI_OBJECT ro;

	rb.Pointer = &ro;
	rb.Length = sizeof (ro);
	if ((status = AcpiEvaluateObjectTyped(dev, method, NULL, &rb,
	    ACPI_TYPE_INTEGER)) == AE_OK) {
		*rint = ro.Integer.Value;
	}

	return (status);
}

/*
 * Create linkage between devinfo nodes and ACPI nodes
 */
ACPI_STATUS
acpica_tag_devinfo(dev_info_t *dip, ACPI_HANDLE acpiobj)
{
	ACPI_STATUS status;
	ACPI_BUFFER rb;

	/*
	 * Tag the devinfo node with the ACPI name
	 */
	rb.Pointer = NULL;
	rb.Length = ACPI_ALLOCATE_BUFFER;
	status = AcpiGetName(acpiobj, ACPI_FULL_PATHNAME, &rb);
	if (ACPI_FAILURE(status)) {
		cmn_err(CE_WARN, "acpica: could not get ACPI path!");
	} else {
		(void) ndi_prop_update_string(DDI_DEV_T_NONE, dip,
		    "acpi-namespace", (char *)rb.Pointer);
		AcpiOsFree(rb.Pointer);

		/*
		 * Tag the ACPI node with the dip
		 */
		status = acpica_set_devinfo(acpiobj, dip);
		ASSERT(ACPI_SUCCESS(status));
	}

	return (status);
}

/*
 * Destroy linkage between devinfo nodes and ACPI nodes
 */
ACPI_STATUS
acpica_untag_devinfo(dev_info_t *dip, ACPI_HANDLE acpiobj)
{
	(void) acpica_unset_devinfo(acpiobj);
	(void) ndi_prop_remove(DDI_DEV_T_NONE, dip, "acpi-namespace");

	return (AE_OK);
}

/*
 * Return the ACPI device node matching this dev_info node, if it
 * exists in the ACPI tree.
 */
ACPI_STATUS
acpica_get_handle(dev_info_t *dip, ACPI_HANDLE *rh)
{
	ACPI_STATUS status;
	char *acpiname;

	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "acpi-namespace", &acpiname) != DDI_PROP_SUCCESS) {
		return (AE_ERROR);
	}

	status = AcpiGetHandle(NULL, acpiname, rh);
	ddi_prop_free((void *)acpiname);
	return (status);
}



/*
 * Manage OS data attachment to ACPI nodes
 */

/*
 * Return the (dev_info_t *) associated with the ACPI node.
 */
ACPI_STATUS
acpica_get_devinfo(ACPI_HANDLE obj, dev_info_t **dipp)
{
	ACPI_STATUS status;
	void *ptr;

	status = AcpiGetData(obj, acpica_devinfo_handler, &ptr);
	if (status == AE_OK) {
		*dipp = (dev_info_t *)ptr;
	}

	return (status);
}

/*
 * Set the dev_info_t associated with the ACPI node.
 */
static ACPI_STATUS
acpica_set_devinfo(ACPI_HANDLE obj, dev_info_t *dip)
{
	ACPI_STATUS status;

	status = AcpiAttachData(obj, acpica_devinfo_handler, (void *)dip);
	return (status);
}

/*
 * Unset the dev_info_t associated with the ACPI node.
 */
static ACPI_STATUS
acpica_unset_devinfo(ACPI_HANDLE obj)
{
	return (AcpiDetachData(obj, acpica_devinfo_handler));
}

/*
 *
 */
void
acpica_devinfo_handler(ACPI_HANDLE obj, void *data)
{
	/* no-op */
}

/*
 * Grow cpu map table on demand.
 */
static void
acpica_grow_cpu_map(void)
{
	if (cpu_map_count == cpu_map_count_max) {
		size_t sz;
		struct cpu_map_item **new_map;

		ASSERT(cpu_map_count_max < INT_MAX / 2);
		cpu_map_count_max += max_ncpus;
		new_map = kmem_zalloc(sizeof (cpu_map[0]) * cpu_map_count_max,
		    KM_SLEEP);
		if (cpu_map_count != 0) {
			ASSERT(cpu_map != NULL);
			sz = sizeof (cpu_map[0]) * cpu_map_count;
			kcopy(cpu_map, new_map, sz);
			kmem_free(cpu_map, sz);
		}
		cpu_map = new_map;
	}
}

/*
 * Maintain mapping information among (cpu id, ACPI processor id, MPIDR,
 * ACPI handle).
 *
 * This is called from acpidev/acpidev_cpu.c, which is the only place
 * that processors are created in aarch64 systems.
 */
ACPI_STATUS
acpica_add_processor_to_map(UINT32 acpi_id, ACPI_HANDLE obj, UINT64 mpidr)
{
	int i;
	ACPI_STATUS rc = AE_OK;
	struct cpu_map_item *item = NULL;

	ASSERT(obj != NULL);
	if (obj == NULL) {
		return (AE_ERROR);
	}

	mutex_enter(&cpu_map_lock);

	if (cpu_map == NULL) {
		acpica_grow_cpu_map();
		ASSERT3P(cpu_map, !=, NULL);
		ASSERT3S(cpu_map_count, <, cpu_map_count_max);
	}

	for (i = 0; i < cpu_map_count; i++) {
		if (cpu_map[i]->obj == obj) {
			rc = AE_ALREADY_EXISTS;
			break;
		} else if (cpu_map[i]->proc_id == acpi_id) {
			ASSERT(item == NULL);
			item = cpu_map[i];
		}
	}

	if (rc == AE_OK) {
		if (item != NULL) {
			/*
			 * ACPI alias objects may cause more than one object
			 * with the same ACPI processor id, only remember
			 * the first object encountered.
			 */
			if (item->obj == NULL) {
				item->obj = obj;
				item->mpidr = mpidr;
			} else {
				rc = AE_ALREADY_EXISTS;
			}
		} else if (cpu_map_count >= INT_MAX / 2) {
			rc = AE_NO_MEMORY;
		} else {
			acpica_grow_cpu_map();
			ASSERT(cpu_map != NULL);
			ASSERT(cpu_map_count < cpu_map_count_max);
			item = kmem_zalloc(sizeof (*item), KM_SLEEP);
			item->cpu_id = -1;
			item->proc_id = acpi_id;
			item->mpidr = mpidr;
			item->obj = obj;
			cpu_map[cpu_map_count] = item;
			cpu_map_count++;
		}
	}

	mutex_exit(&cpu_map_lock);

	return (rc);
}

ACPI_STATUS
acpica_remove_processor_from_map(UINT32 acpi_id)
{
	int i;
	ACPI_STATUS rc = AE_NOT_EXIST;

	mutex_enter(&cpu_map_lock);
	for (i = 0; i < cpu_map_count; i++) {
		if (cpu_map[i]->proc_id != acpi_id) {
			continue;
		}
		cpu_map[i]->obj = NULL;
		/* Free item if no more reference to it. */
		if (cpu_map[i]->cpu_id == -1) {
			kmem_free(cpu_map[i], sizeof (struct cpu_map_item));
			cpu_map[i] = NULL;
			cpu_map_count--;
			if (i != cpu_map_count) {
				cpu_map[i] = cpu_map[cpu_map_count];
				cpu_map[cpu_map_count] = NULL;
			}
		}
		rc = AE_OK;
		break;
	}
	mutex_exit(&cpu_map_lock);

	return (rc);
}

ACPI_STATUS
acpica_map_cpu(processorid_t cpuid, UINT32 acpi_id)
{
	int i;
	ACPI_STATUS rc = AE_OK;
	struct cpu_map_item *item = NULL;

	ASSERT(cpuid != -1);
	if (cpuid == -1) {
		return (AE_ERROR);
	}

	mutex_enter(&cpu_map_lock);

	if (cpu_map == NULL) {
		acpica_grow_cpu_map();
		ASSERT3P(cpu_map, !=, NULL);
		ASSERT3S(cpu_map_count, <, cpu_map_count_max);
	}

	for (i = 0; i < cpu_map_count; i++) {
		if (cpu_map[i]->cpu_id == cpuid) {
			rc = AE_ALREADY_EXISTS;
			break;
		} else if (cpu_map[i]->proc_id == acpi_id) {
			ASSERT(item == NULL);
			item = cpu_map[i];
		}
	}
	if (rc == AE_OK) {
		if (item != NULL) {
			if (item->cpu_id == -1) {
				item->cpu_id = cpuid;
			} else {
				rc = AE_ALREADY_EXISTS;
			}
		} else if (cpu_map_count >= INT_MAX / 2) {
			rc = AE_NO_MEMORY;
		} else {
			acpica_grow_cpu_map();
			ASSERT(cpu_map != NULL);
			ASSERT(cpu_map_count < cpu_map_count_max);
			item = kmem_zalloc(sizeof (*item), KM_SLEEP);
			item->cpu_id = cpuid;
			item->proc_id = acpi_id;
			item->mpidr = UINT64_MAX;
			item->obj = NULL;
			cpu_map[cpu_map_count] = item;
			cpu_map_count++;
		}
	}
	mutex_exit(&cpu_map_lock);

	return (rc);
}

ACPI_STATUS
acpica_unmap_cpu(processorid_t cpuid)
{
	int i;
	ACPI_STATUS rc = AE_NOT_EXIST;

	ASSERT(cpuid != -1);
	if (cpuid == -1) {
		return (rc);
	}

	mutex_enter(&cpu_map_lock);
	for (i = 0; i < cpu_map_count; i++) {
		if (cpu_map[i]->cpu_id != cpuid) {
			continue;
		}
		cpu_map[i]->cpu_id = -1;
		/* Free item if no more reference. */
		if (cpu_map[i]->obj == NULL) {
			kmem_free(cpu_map[i], sizeof (struct cpu_map_item));
			cpu_map[i] = NULL;
			cpu_map_count--;
			if (i != cpu_map_count) {
				cpu_map[i] = cpu_map[cpu_map_count];
				cpu_map[cpu_map_count] = NULL;
			}
		}
		rc = AE_OK;
		break;
	}
	mutex_exit(&cpu_map_lock);

	return (rc);
}

ACPI_STATUS
acpica_get_cpu_object_by_cpuid(processorid_t cpuid, ACPI_HANDLE *hdlp)
{
	int i;
	ACPI_STATUS rc = AE_NOT_EXIST;

	ASSERT(cpuid != -1);
	if (cpuid == -1) {
		return (rc);
	}

	mutex_enter(&cpu_map_lock);
	for (i = 0; i < cpu_map_count; i++) {
		if (cpu_map[i]->cpu_id == cpuid && cpu_map[i]->obj != NULL) {
			*hdlp = cpu_map[i]->obj;
			rc = AE_OK;
			break;
		}
	}
	mutex_exit(&cpu_map_lock);

	return (rc);
}

ACPI_STATUS
acpica_get_cpu_object_by_mpidr(UINT64 mpidr, ACPI_HANDLE *hdlp)
{
	int i;
	ACPI_STATUS rc = AE_NOT_EXIST;

	ASSERT(mpidr != UINT64_MAX);
	if (mpidr == UINT64_MAX) {
		return (rc);
	}

	mutex_enter(&cpu_map_lock);
	for (i = 0; i < cpu_map_count; i++) {
		if (cpu_map[i]->mpidr == mpidr && cpu_map[i]->obj != NULL) {
			*hdlp = cpu_map[i]->obj;
			rc = AE_OK;
			break;
		}
	}
	mutex_exit(&cpu_map_lock);

	return (rc);
}

ACPI_STATUS
acpica_get_cpu_id_by_object(ACPI_HANDLE hdl, processorid_t *cpuidp)
{
	int i;
	ACPI_STATUS rc = AE_NOT_EXIST;

	ASSERT(cpuidp != NULL);
	if (hdl == NULL || cpuidp == NULL) {
		return (rc);
	}

	*cpuidp = -1;
	mutex_enter(&cpu_map_lock);
	for (i = 0; i < cpu_map_count; i++) {
		if (cpu_map[i]->obj == hdl && cpu_map[i]->cpu_id != -1) {
			*cpuidp = cpu_map[i]->cpu_id;
			rc = AE_OK;
			break;
		}
	}
	mutex_exit(&cpu_map_lock);

	return (rc);
}

ACPI_STATUS
acpica_get_procid_by_object(ACPI_HANDLE hdl, UINT32 *rp)
{
	int i;
	ACPI_STATUS rc = AE_NOT_EXIST;

	ASSERT(rp != NULL);
	if (hdl == NULL || rp == NULL) {
		return (rc);
	}

	*rp = UINT32_MAX;
	mutex_enter(&cpu_map_lock);
	for (i = 0; i < cpu_map_count; i++) {
		if (cpu_map[i]->obj == hdl) {
			*rp = cpu_map[i]->proc_id;
			rc = AE_OK;
			break;
		}
	}
	mutex_exit(&cpu_map_lock);

	return (rc);
}

void
acpica_set_core_feature(uint64_t features)
{
	atomic_or_64(&acpica_core_features, features);
}

void
acpica_clear_core_feature(uint64_t features)
{
	atomic_and_64(&acpica_core_features, ~features);
}

uint64_t
acpica_get_core_feature(uint64_t features)
{
	return (acpica_core_features & features);
}

void
acpica_set_devcfg_feature(uint64_t features)
{
	atomic_or_64(&acpica_devcfg_features, features);
}

void
acpica_clear_devcfg_feature(uint64_t features)
{
	atomic_and_64(&acpica_devcfg_features, ~features);
}

uint64_t
acpica_get_devcfg_feature(uint64_t features)
{
	return (acpica_devcfg_features & features);
}

void
acpica_set_plat_osc(uint64_t caps)
{
	atomic_or_64(&acpica_plat_osc_caps, caps);
}

void
acpica_clear_plat_osc(uint64_t caps)
{
	atomic_and_64(&acpica_plat_osc_caps, ~caps);
}

uint64_t
acpica_get_plat_osc(uint64_t caps)
{
	return (acpica_plat_osc_caps & caps);
}

void
acpica_write_cpupm_capabilities(boolean_t pstates __unused,
    boolean_t cstates __unused)
{
	/* nothing to do for hardware-reduced platforms */
}

uint32_t
acpi_strtoul(const char *str, char **ep, int base)
{
	ulong_t v;

	if (ddi_strtoul(str, ep, base, &v) != 0 || v > ACPI_UINT32_MAX) {
		return (ACPI_UINT32_MAX);
	}

	return ((uint32_t)v);
}

/*
 * In prior versions of ACPI, the AcpiGetObjectInfo() function would provide
 * information about the status of the object via the _STA method. This has been
 * removed and this function is used to replace.
 *
 * Not every ACPI object has a _STA method. In cases where it is not found, then
 * the OSPM (aka us) is supposed to interpret that as though it indicates that
 * the device is present, enabled, shown in the UI, and functioning. This is the
 * value 0xF.
 */
ACPI_STATUS
acpica_get_object_status(ACPI_HANDLE obj, int *statusp)
{
	ACPI_STATUS status;
	int ival;

	status = acpica_eval_int(obj, METHOD_NAME__STA, &ival);
	if (ACPI_FAILURE(status)) {
		if (status == AE_NOT_FOUND) {
			*statusp = 0xf;
			return (AE_OK);
		}

		return (status);
	}

	/*
	 * This should not be a negative value. However, firmware is often the
	 * enemy. If it does, complain and treat that as a hard failure.
	 */
	if (ival < 0) {
		cmn_err(CE_WARN, "!acpica_get_object_status: encountered "
		    "negative _STA value on obj %p", obj);
		return (AE_ERROR);
	}

	*statusp = ival;
	return (AE_OK);
}
