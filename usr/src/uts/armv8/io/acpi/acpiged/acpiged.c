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
 * ACPI Generic Event Device (GED) Driver
 *
 * This driver supports the ACPI Generic Event Device (ACPI0013), which is
 * the HW-reduced ACPI replacement for GPE-based event delivery.  On
 * platforms without the legacy x86 fixed hardware (GPE register blocks,
 * SCI), GED devices deliver platform events - hotplug notifications,
 * power button presses, thermal events - via standard interrupts routed
 * through AML.
 *
 * The driver is intentionally simple:
 * 1. Match ACPI0013 devices.
 * 2. Walk _CRS to collect GSIVs from ACPI interrupt resource descriptors.
 * 3. Allocate one DDI interrupt per GSIV.
 * 4. Each ISR dispatches to a taskq that evaluates the appropriate event
 *    method, where the AML performs device-specific dispatch (e.g.
 *    Notify(PWRB, 0x80)).
 *
 * For GSIVs 0x00-0xFF, the driver checks at attach time whether an
 * _Exx (edge-triggered) or _Lxx (level-triggered) method exists in the
 * GED scope, using the trigger mode from the _CRS interrupt resource
 * descriptor.  If such a method exists, it takes precedence over _EVT
 * per ACPI 6.6 §5.6.9.  For GSIVs > 0xFF, or when no _Exx/_Lxx method
 * exists, _EVT(gsiv) is used.
 *
 * GSIVs are recovered directly from the ACPI resource descriptors via
 * AcpiWalkResources, keeping the driver independent of the interrupt
 * controller's devinfo property encoding.
 *
 * _INI methods on GED devices (which configure GPIO registers, etc.) are
 * evaluated by ACPICA during AcpiInitializeObjects() before this driver
 * attaches - no hardware initialisation is required here.
 *
 * See ACPI Specification 6.6 §5.6.9 "Interrupt-signaled ACPI events".
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/note.h>
#include <sys/taskq.h>
#include <sys/class.h>

#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/stdbool.h>

/*
 * Per-interrupt state.  One of these exists for each GSIV listed in the
 * GED device's _CRS.
 */
typedef struct acpiged_intr {
	struct acpiged_softstate	*agi_sp;
	ddi_intr_handle_t		agi_hdl;
	uint32_t			agi_gsiv;
	uint8_t				agi_triggering;
	bool				agi_use_evt;
	char				agi_method[5];
} acpiged_intr_t;

/*
 * Per-instance soft state.
 */
typedef struct acpiged_softstate {
	dev_info_t		*agd_dip;
	ACPI_HANDLE		agd_hdl;
	taskq_t			*agd_taskq;
	int			agd_nintr;
	acpiged_intr_t		*agd_intrs;
} acpiged_softstate_t;

/*
 * Temporary state used during _CRS walk to collect GSIVs.
 */
typedef struct acpiged_crs_ctx {
	uint32_t	*gsivs;		/* NULL = counting pass */
	uint8_t		*triggers;	/* NULL = counting pass */
	int		count;
	int		limit;
} acpiged_crs_ctx_t;

static void *acpiged_softstates;

/*
 * _CRS walk callback - collect GSIVs from interrupt resource descriptors.
 */
static ACPI_STATUS
acpiged_crs_cb(ACPI_RESOURCE *rsc, void *ctx)
{
	acpiged_crs_ctx_t *cp = (acpiged_crs_ctx_t *)ctx;
	uint_t i;

	switch (rsc->Type) {
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		for (i = 0; i < rsc->Data.ExtendedIrq.InterruptCount; i++) {
			if (cp->gsivs != NULL) {
				if (cp->count >= cp->limit) {
					return (AE_LIMIT);
				}
				cp->gsivs[cp->count] =
				    rsc->Data.ExtendedIrq.Interrupts[i];
				cp->triggers[cp->count] =
				    rsc->Data.ExtendedIrq.Triggering;
			}
			cp->count++;
		}
		break;
	case ACPI_RESOURCE_TYPE_IRQ:
		for (i = 0; i < rsc->Data.Irq.InterruptCount; i++) {
			if (cp->gsivs != NULL) {
				if (cp->count >= cp->limit) {
					return (AE_LIMIT);
				}
				cp->gsivs[cp->count] =
				    rsc->Data.Irq.Interrupts[i];
				cp->triggers[cp->count] =
				    rsc->Data.Irq.Triggering;
			}
			cp->count++;
		}
		break;
	default:
		break;
	}

	return (AE_OK);
}

/*
 * Resolve the event method for a single interrupt.
 *
 * For GSIVs 0x00-0xFF, check whether an _Exx (edge) or _Lxx (level)
 * method exists in the GED scope.  If it does, use it in preference
 * to _EVT per ACPI 6.6 §5.6.9.  For GSIVs > 0xFF, or when no
 * _Exx/_Lxx method exists, fall back to _EVT.
 */
static void
acpiged_resolve_method(acpiged_softstate_t *sp, acpiged_intr_t *ip)
{
	ACPI_HANDLE mhdl;

	if (ip->agi_gsiv <= 0xFF) {
		(void) snprintf(ip->agi_method, sizeof (ip->agi_method),
		    "_%c%02X",
		    ip->agi_triggering == ACPI_EDGE_SENSITIVE ? 'E' : 'L',
		    ip->agi_gsiv);

		if (ACPI_SUCCESS(AcpiGetHandle(sp->agd_hdl,
		    ip->agi_method, &mhdl))) {
			ip->agi_use_evt = false;
			return;
		}
	}

	(void) strlcpy(ip->agi_method, "_EVT", sizeof (ip->agi_method));
	ip->agi_use_evt = true;
}

/*
 * Taskq callback - evaluate the resolved event method under the ACPICA
 * interpreter.  _EVT takes one argument (the GSIV) - _Exx/_Lxx take none.
 */
static void
acpiged_evt_task(void *arg)
{
	acpiged_intr_t *ip = (acpiged_intr_t *)arg;
	acpiged_softstate_t *sp = ip->agi_sp;
	ACPI_OBJECT_LIST args;
	ACPI_OBJECT obj;
	ACPI_OBJECT_LIST *argsp;

	if (ip->agi_use_evt) {
		obj.Type = ACPI_TYPE_INTEGER;
		obj.Integer.Value = ip->agi_gsiv;
		args.Count = 1;
		args.Pointer = &obj;
		argsp = &args;
	} else {
		argsp = NULL;
	}

	if (ACPI_FAILURE(AcpiEvaluateObject(sp->agd_hdl, ip->agi_method,
	    argsp, NULL))) {
		dev_err(sp->agd_dip, CE_WARN,
		    "!acpiged: %s(0x%x) evaluation failed",
		    ip->agi_method, ip->agi_gsiv);
	}
}

/*
 * Interrupt handler - dispatch to taskq for AML evaluation.
 */
static uint_t
acpiged_intr(caddr_t arg1, caddr_t arg2 __unused)
{
	acpiged_intr_t *ip = (acpiged_intr_t *)arg1;

	if (taskq_dispatch(ip->agi_sp->agd_taskq, acpiged_evt_task,
	    ip, TQ_NOSLEEP) == TASKQID_INVALID) {
		dev_err(ip->agi_sp->agd_dip, CE_WARN,
		    "!acpiged: failed to dispatch %s for GSIV %u, "
		    "evaluating inline", ip->agi_method, ip->agi_gsiv);
		/*
		 * We might magically become an interrupt thread.  This is fine.
		 */
		acpiged_evt_task(ip);
	}

	return (DDI_INTR_CLAIMED);
}

/*
 * Tear down all interrupt state.
 */
static void
acpiged_teardown_intrs(acpiged_softstate_t *sp)
{
	int i;

	for (i = 0; i < sp->agd_nintr; i++) {
		acpiged_intr_t *ip = &sp->agd_intrs[i];

		if (ip->agi_hdl != NULL) {
			(void) ddi_intr_disable(ip->agi_hdl);
			(void) ddi_intr_remove_handler(ip->agi_hdl);
			(void) ddi_intr_free(ip->agi_hdl);
			ip->agi_hdl = NULL;
		}
	}
}

static int
acpiged_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	acpiged_softstate_t *sp;
	acpiged_crs_ctx_t crs;
	ddi_intr_handle_t *hdls;
	int instance;
	int nintr, nactual;
	int i, ret;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(dip);

	if (ddi_soft_state_zalloc(acpiged_softstates, instance) !=
	    DDI_SUCCESS) {
		dev_err(dip, CE_WARN,
		    "!acpiged: failed to allocate soft state");
		return (DDI_FAILURE);
	}

	sp = ddi_get_soft_state(acpiged_softstates, instance);
	sp->agd_dip = dip;

	if (ACPI_FAILURE(acpica_get_handle(dip, &sp->agd_hdl))) {
		dev_err(dip, CE_WARN,
		    "!acpiged: failed to get ACPI handle");
		goto fail_softstate;
	}

	/*
	 * If firmware did not grant GED support in \_SB._OSC, attach
	 * the device (it exists in the namespace) but do not wire up
	 * interrupts.  GED events will not be delivered.
	 */
	if (acpica_get_plat_osc(PLAT_OSC_GED) == 0) {
		dev_err(dip, CE_NOTE,
		    "!acpiged: GED not granted by \\_SB._OSC, "
		    "events will not be delivered");
		ddi_report_dev(dip);
		return (DDI_SUCCESS);
	}

	/*
	 * Walk _CRS to count the GSIVs from the ACPI interrupt resource
	 * descriptors, then walk again to collect them.  This avoids
	 * encoding knowledge of the interrupt controller's devinfo
	 * property format and removes any fixed limit on interrupt count.
	 */
	crs.gsivs = NULL;
	crs.triggers = NULL;
	crs.count = 0;
	crs.limit = 0;
	if (ACPI_FAILURE(AcpiWalkResources(sp->agd_hdl, METHOD_NAME__CRS,
	    acpiged_crs_cb, &crs)) || crs.count < 1) {
		dev_err(dip, CE_WARN,
		    "!acpiged: failed to walk %s for GSIVs", METHOD_NAME__CRS);
		goto fail_softstate;
	}

	nintr = crs.count;

	if (ddi_intr_get_nintrs(dip, DDI_INTR_TYPE_FIXED, &ret) !=
	    DDI_SUCCESS || ret != nintr) {
		dev_err(dip, CE_WARN,
		    "!acpiged: _CRS GSIV count (%d) != interrupt count (%d)",
		    nintr, ret);
		goto fail_softstate;
	}

	crs.gsivs = kmem_alloc(nintr * sizeof (uint32_t), KM_SLEEP);
	crs.triggers = kmem_alloc(nintr * sizeof (uint8_t), KM_SLEEP);
	crs.count = 0;
	crs.limit = nintr;
	if (ACPI_FAILURE(AcpiWalkResources(sp->agd_hdl, METHOD_NAME__CRS,
	    acpiged_crs_cb, &crs)) || crs.count != nintr) {
		dev_err(dip, CE_WARN,
		    "!acpiged: failed to collect GSIVs from %s",
		    METHOD_NAME__CRS);
		kmem_free(crs.gsivs, nintr * sizeof (uint32_t));
		kmem_free(crs.triggers, nintr * sizeof (uint8_t));
		goto fail_softstate;
	}

	/*
	 * Create a taskq for AML evaluation.  A single thread is sufficient,
	 * as event methods take the ACPICA interpreter lock and events are
	 * serialised.
	 */
	sp->agd_taskq = taskq_create("acpiged", 1, minclsyspri, 1, 4,
	    TASKQ_PREPOPULATE);
	if (sp->agd_taskq == NULL) {
		dev_err(dip, CE_WARN,
		    "!acpiged: failed to create taskq");
		kmem_free(crs.gsivs, nintr * sizeof (uint32_t));
		kmem_free(crs.triggers, nintr * sizeof (uint8_t));
		goto fail_softstate;
	}

	sp->agd_intrs = kmem_zalloc(nintr * sizeof (acpiged_intr_t), KM_SLEEP);
	sp->agd_nintr = nintr;

	/*
	 * Allocate all FIXED interrupts in a single call.
	 */
	hdls = kmem_zalloc(nintr * sizeof (ddi_intr_handle_t), KM_SLEEP);
	ret = ddi_intr_alloc(dip, hdls, DDI_INTR_TYPE_FIXED,
	    0, nintr, &nactual, DDI_INTR_ALLOC_STRICT);
	if (ret != DDI_SUCCESS || nactual != nintr) {
		dev_err(dip, CE_WARN,
		    "!acpiged: failed to allocate interrupts "
		    "(%d of %d)", nactual, nintr);
		kmem_free(hdls, nintr * sizeof (ddi_intr_handle_t));
		goto fail_intrs;
	}
	for (i = 0; i < nintr; i++) {
		sp->agd_intrs[i].agi_hdl = hdls[i];
	}
	kmem_free(hdls, nintr * sizeof (ddi_intr_handle_t));

	for (i = 0; i < nintr; i++) {
		acpiged_intr_t *ip = &sp->agd_intrs[i];

		ip->agi_sp = sp;
		ip->agi_gsiv = crs.gsivs[i];
		ip->agi_triggering = crs.triggers[i];
		acpiged_resolve_method(sp, ip);

		if (ddi_intr_add_handler(ip->agi_hdl, acpiged_intr,
		    (caddr_t)ip, NULL) != DDI_SUCCESS) {
			dev_err(dip, CE_WARN,
			    "!acpiged: failed to add handler for interrupt %d",
			    i);
			goto fail_intrs;
		}

		if (ddi_intr_enable(ip->agi_hdl) != DDI_SUCCESS) {
			dev_err(dip, CE_WARN,
			    "!acpiged: failed to enable interrupt %d", i);
			(void) ddi_intr_remove_handler(ip->agi_hdl);
			goto fail_intrs;
		}
	}

	kmem_free(crs.gsivs, nintr * sizeof (uint32_t));
	kmem_free(crs.triggers, nintr * sizeof (uint8_t));

	ddi_report_dev(dip);
	return (DDI_SUCCESS);

fail_intrs:
	kmem_free(crs.gsivs, nintr * sizeof (uint32_t));
	kmem_free(crs.triggers, nintr * sizeof (uint8_t));
	acpiged_teardown_intrs(sp);
	kmem_free(sp->agd_intrs, sp->agd_nintr * sizeof (acpiged_intr_t));
	taskq_destroy(sp->agd_taskq);
fail_softstate:
	ddi_soft_state_free(acpiged_softstates, instance);
	return (DDI_FAILURE);
}

static int
acpiged_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	acpiged_softstate_t *sp;
	int instance;

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	instance = ddi_get_instance(dip);
	sp = ddi_get_soft_state(acpiged_softstates, instance);
	if (sp == NULL) {
		return (DDI_FAILURE);
	}

	acpiged_teardown_intrs(sp);

	if (sp->agd_taskq != NULL) {
		taskq_destroy(sp->agd_taskq);
	}

	if (sp->agd_intrs) {
		kmem_free(sp->agd_intrs,
		    sp->agd_nintr * sizeof (acpiged_intr_t));
	}

	ddi_soft_state_free(acpiged_softstates, instance);
	return (DDI_SUCCESS);
}

static struct dev_ops acpiged_dev_ops = {
	.devo_rev =		DEVO_REV,
	.devo_refcnt =		0,
	.devo_getinfo =		ddi_no_info,
	.devo_identify =	nulldev,
	.devo_probe =		nulldev,
	.devo_attach =		acpiged_attach,
	.devo_detach =		acpiged_detach,
	.devo_reset =		nodev,
	.devo_cb_ops =		NULL,
	.devo_bus_ops =		NULL,
	.devo_power =		NULL,
	.devo_quiesce =		ddi_quiesce_not_needed,
};

static struct modldrv acpiged_modldrv = {
	&mod_driverops,
	"ACPI Generic Event Device",
	&acpiged_dev_ops
};

static struct modlinkage acpiged_modlinkage = {
	MODREV_1,
	{ &acpiged_modldrv, NULL }
};

int
_init(void)
{
	int ret;

	ret = ddi_soft_state_init(&acpiged_softstates,
	    sizeof (acpiged_softstate_t), 2);
	if (ret != 0) {
		return (ret);
	}

	ret = mod_install(&acpiged_modlinkage);
	if (ret != 0) {
		ddi_soft_state_fini(&acpiged_softstates);
	}

	return (ret);
}

int
_fini(void)
{
	int ret;

	ret = mod_remove(&acpiged_modlinkage);
	if (ret == 0) {
		ddi_soft_state_fini(&acpiged_softstates);
	}

	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&acpiged_modlinkage, modinfop));
}
