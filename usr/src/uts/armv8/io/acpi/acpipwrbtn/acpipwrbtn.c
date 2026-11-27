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
 * ACPI Power Button Driver
 *
 * This driver binds to PNP0C0C (ACPI control method power button) devices
 * enumerated via acpidev.  It provides the /dev/power_button character
 * device interface, compatible with the i86pc power(4D) driver's ioctl
 * and poll interface (PB_BEGIN_MONITOR, PB_END_MONITOR, PB_GET_EVENTS,
 * PB_CREATE_BUTTON_EVENT as defined in <sys/pbio.h>).
 *
 * When an ACPI Notify(0x80) is delivered to the PNP0C0C device (typically
 * from a GED event method), the driver either:
 * - Wakes a monitoring userspace daemon (powerd) via pollwakeup, or
 * - Initiates a direct kernel shutdown via psignal(init, SIGPWR) if
 *   no monitor is attached.
 *
 * The triple-press abort sequence (configurable via /etc/system) calls
 * abort_sequence_enter for kernel debugging.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/note.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/thread.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/reboot.h>
#include <sys/policy.h>
#include <sys/sysevent/pwrctl.h>
#include <sys/sysevent/eventdefs.h>

#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/pbio.h>

#define	ACPIPWRBTN_NOTIFY_PRESSED	0x80

#define	ACPIPWRBTN_MAX_CLONE	256
#define	ACPIPWRBTN_MINOR_TO_CLONE(minor)	\
	((minor) & (ACPIPWRBTN_MAX_CLONE - 1))
#define	ACPIPWRBTN_MINOR_TO_INST(minor)		((minor) >> 8)

/*
 * Tunables: patchable via /etc/system.
 */
hrtime_t	acpipwrbtn_abort_interval = 1500000000LL;	/* 1.5 sec */
int		acpipwrbtn_abort_presses = 3;
int		acpipwrbtn_abort_enable = 1;
int		acpipwrbtn_enable = 1;

typedef struct acpipwrbtn_softstate {
	dev_info_t	*dip;
	ACPI_HANDLE	acpi_hdl;
	kmutex_t	mutex;
	pollhead_t	pollhd;
	int		events;
	int		monitor_on;		/* clone # or 0 */
	int		shutdown_pending;
	hrtime_t	last_press;
	int		press_count;
	timeout_id_t	abort_tid;
	uchar_t		clones[ACPIPWRBTN_MAX_CLONE];
} acpipwrbtn_softstate_t;

static void *acpipwrbtn_statep;
static int acpipwrbtn_inst = -1;

/*
 * Forward declarations.
 */
static int acpipwrbtn_attach(dev_info_t *, ddi_attach_cmd_t);
static int acpipwrbtn_detach(dev_info_t *, ddi_detach_cmd_t);
static int acpipwrbtn_open(dev_t *, int, int, cred_t *);
static int acpipwrbtn_close(dev_t, int, int, cred_t *);
static int acpipwrbtn_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int acpipwrbtn_chpoll(dev_t, short, int, short *, struct pollhead **);
static int acpipwrbtn_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);

static struct cb_ops acpipwrbtn_cb_ops = {
	.cb_open =	acpipwrbtn_open,
	.cb_close =	acpipwrbtn_close,
	.cb_strategy =	nodev,
	.cb_print =	nodev,
	.cb_dump =	nodev,
	.cb_read =	nodev,
	.cb_write =	nodev,
	.cb_ioctl =	acpipwrbtn_ioctl,
	.cb_devmap =	nodev,
	.cb_mmap =	nodev,
	.cb_segmap =	nodev,
	.cb_chpoll =	acpipwrbtn_chpoll,
	.cb_prop_op =	ddi_prop_op,
	.cb_str =	NULL,
	.cb_flag =	D_NEW | D_MP,
	.cb_rev =	CB_REV,
	.cb_aread =	nodev,
	.cb_awrite =	nodev,
};

static struct dev_ops acpipwrbtn_dev_ops = {
	.devo_rev =		DEVO_REV,
	.devo_refcnt =		0,
	.devo_getinfo =		acpipwrbtn_getinfo,
	.devo_identify =	nulldev,
	.devo_probe =		nulldev,
	.devo_attach =		acpipwrbtn_attach,
	.devo_detach =		acpipwrbtn_detach,
	.devo_reset =		nodev,
	.devo_cb_ops =		&acpipwrbtn_cb_ops,
	.devo_bus_ops =		NULL,
	.devo_power =		NULL,
	.devo_quiesce =		ddi_quiesce_not_needed,
};

static struct modldrv acpipwrbtn_modldrv = {
	&mod_driverops,
	"ACPI Power Button",
	&acpipwrbtn_dev_ops
};

static struct modlinkage acpipwrbtn_modlinkage = {
	MODREV_1,
	{ &acpipwrbtn_modldrv, NULL }
};

static void
acpipwrbtn_gen_sysevent(acpipwrbtn_softstate_t *sp)
{
	nvlist_t *attr_list = NULL;
	char pathname[MAXPATHLEN];
	int err;

	err = nvlist_alloc(&attr_list, NV_UNIQUE_NAME_TYPE, KM_SLEEP);
	if (err != 0) {
		dev_err(sp->dip, CE_NOTE,
		    "!gen_sysevent: nvlist_alloc failed: 0x%x", err);
		return;
	}

	err = nvlist_add_string(attr_list, PWRCTL_DEV_HID, "PNP0C0C");
	if (err != 0) {
		dev_err(sp->dip, CE_NOTE,
		    "!gen_sysevent: nvlist_add_string failed: 0x%x", err);
		nvlist_free(attr_list);
		return;
	}

	(void) ddi_pathname(sp->dip, pathname);
	err = nvlist_add_string(attr_list, PWRCTL_DEV_PHYS_PATH, pathname);
	if (err != 0) {
		dev_err(sp->dip, CE_NOTE,
		    "!gen_sysevent: nvlist_add_string failed: 0x%x", err);
		nvlist_free(attr_list);
		return;
	}

	if ((err = ddi_log_sysevent(sp->dip, DDI_VENDOR_SUNW, EC_PWRCTL,
	    ESC_PWRCTL_POWER_BUTTON, attr_list, NULL, DDI_SLEEP))
	    != DDI_SUCCESS) {
		dev_err(sp->dip, CE_NOTE,
		    "!gen_sysevent: ddi_log_sysevent failed: 0x%x", err);
	}

	nvlist_free(attr_list);
}

/*
 * Initiate shutdown.  If a userspace monitor is attached, wake it via
 * pollwakeup and let it handle the shutdown gracefully.  Otherwise,
 * signal init directly.
 */
static void
acpipwrbtn_shutdown(acpipwrbtn_softstate_t *sp)
{
	acpipwrbtn_gen_sysevent(sp);

	mutex_enter(&sp->mutex);
	sp->events |= PB_BUTTON_PRESS;

	if (sp->monitor_on != 0) {
		mutex_exit(&sp->mutex);
		pollwakeup(&sp->pollhd, POLLRDNORM);
		pollwakeup(&sp->pollhd, POLLIN);
		return;
	}

	if (!sp->shutdown_pending) {
		proc_t *initpp;

		cmn_err(CE_WARN, "Power off requested via power button, "
		    "powering down the system!");
		sp->shutdown_pending = 1;
		mutex_exit(&sp->mutex);

		mutex_enter(&pidlock);
		initpp = prfind(P_INITPID);
		mutex_exit(&pidlock);

		if (initpp == NULL) {
			halt("Power off the System");
		}

		psignal(initpp, SIGPWR);
		return;
	}

	mutex_exit(&sp->mutex);
}

static void
acpipwrbtn_abort_timeout(void *arg)
{
	acpipwrbtn_softstate_t *sp = (acpipwrbtn_softstate_t *)arg;

	mutex_enter(&sp->mutex);
	sp->abort_tid = 0;
	sp->press_count = 0;
	sp->last_press = 0;
	mutex_exit(&sp->mutex);

	acpipwrbtn_shutdown(sp);
}

/*
 * ACPI notify handler.
 *
 * Called by ACPICA when Notify(PNP0C0C, 0x80) is evaluated in AML.
 *
 * Also called on wakeup from S1-S4 with val as 0x02 (ACPI_NOTIFY_DEVICE_WAKE).
 */
static void
acpipwrbtn_acpi_notify(ACPI_HANDLE obj, UINT32 val, void *ctx)
{
	dev_info_t *dip;
	acpipwrbtn_softstate_t *sp = (acpipwrbtn_softstate_t *)ctx;

	switch (val) {
	case ACPI_NOTIFY_DEVICE_WAKE:
		return;	/* nothing to do here */
	case ACPIPWRBTN_NOTIFY_PRESSED:
		break;	/* we explicitly handle this */
	default:
		if (ACPI_SUCCESS(acpica_get_devinfo(obj, &dip))) {
			dev_err(dip, CE_NOTE,
			    "!unknown notify value 0x%x", val);
		} else {
			cmn_err(CE_NOTE,
			    "!acpipwrbtn: unknown notify value 0x%x", val);
		}

		return;
	}

	ASSERT3U(val, ==, ACPIPWRBTN_NOTIFY_PRESSED);

	if (!acpipwrbtn_enable) {
		return;
	}

	/*
	 * Abort sequence detection: N presses within the abort interval
	 * triggers abort_sequence_enter() for kernel debugging.
	 *
	 * Only arm the timer when the abort feature is enabled AND the
	 * kernel debugger is loaded - abort_sequence_enter() is a no-op
	 * without kmdb, so there's no point delaying the shutdown.
	 */
	if (acpipwrbtn_abort_enable && (boothowto & RB_DEBUG)) {
		hrtime_t now = gethrtime();

		mutex_enter(&sp->mutex);
		if (sp->last_press == 0 ||
		    (now - sp->last_press) > acpipwrbtn_abort_interval) {
			sp->press_count = 1;
		} else {
			sp->press_count++;
		}
		sp->last_press = now;

		if (sp->press_count >= acpipwrbtn_abort_presses) {
			timeout_id_t tid = sp->abort_tid;
			sp->abort_tid = 0;
			sp->press_count = 0;
			sp->last_press = 0;
			mutex_exit(&sp->mutex);
			if (tid != 0) {
				(void) untimeout(tid);
			}
			abort_sequence_enter("Power Button Abort");
			return;
		}

		if (sp->press_count == 1) {
			/* First press: start the window timer */
			sp->abort_tid = timeout(acpipwrbtn_abort_timeout, sp,
			    drv_usectohz(acpipwrbtn_abort_interval / 1000));
		}
		mutex_exit(&sp->mutex);
		return;
	}

	acpipwrbtn_shutdown(sp);
}

static int
acpipwrbtn_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	acpipwrbtn_softstate_t *sp;
	int instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	/* Only one power button instance */
	if (acpipwrbtn_inst != -1) {
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(acpipwrbtn_statep, instance) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	sp = ddi_get_soft_state(acpipwrbtn_statep, instance);
	sp->dip = dip;

	if (ACPI_FAILURE(acpica_get_handle(dip, &sp->acpi_hdl))) {
		dev_err(dip, CE_WARN,
		    "!acpipwrbtn: failed to get ACPI handle");
		goto fail;
	}

	mutex_init(&sp->mutex, NULL, MUTEX_DRIVER, NULL);

	if (ddi_create_minor_node(dip, "power_button", S_IFCHR,
	    (instance << 8) + 0, "ddi_power_button", 0) != DDI_SUCCESS) {
		dev_err(dip, CE_WARN,
		    "!acpipwrbtn: failed to create minor node");
		mutex_destroy(&sp->mutex);
		goto fail;
	}

	if (ACPI_FAILURE(AcpiInstallNotifyHandler(sp->acpi_hdl,
	    ACPI_DEVICE_NOTIFY, acpipwrbtn_acpi_notify, (void *)sp))) {
		dev_err(dip, CE_WARN,
		    "!acpipwrbtn: failed to install ACPI notify handler");
		ddi_remove_minor_node(dip, NULL);
		mutex_destroy(&sp->mutex);
		goto fail;
	}

	acpipwrbtn_inst = instance;
	ddi_report_dev(dip);
	return (DDI_SUCCESS);

fail:
	ddi_soft_state_free(acpipwrbtn_statep, instance);
	return (DDI_FAILURE);
}

static int
acpipwrbtn_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	acpipwrbtn_softstate_t *sp;
	int instance = ddi_get_instance(dip);

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	sp = ddi_get_soft_state(acpipwrbtn_statep, instance);
	if (sp == NULL) {
		return (DDI_FAILURE);
	}

	if (sp->abort_tid != 0) {
		(void) untimeout(sp->abort_tid);
	}

	(void) AcpiRemoveNotifyHandler(sp->acpi_hdl, ACPI_DEVICE_NOTIFY,
	    acpipwrbtn_acpi_notify);
	ddi_remove_minor_node(dip, NULL);
	mutex_destroy(&sp->mutex);
	ddi_soft_state_free(acpipwrbtn_statep, instance);
	acpipwrbtn_inst = -1;

	return (DDI_SUCCESS);
}

static int
acpipwrbtn_getinfo(dev_info_t *dip __unused, ddi_info_cmd_t infocmd, void *arg,
    void **result)
{
	acpipwrbtn_softstate_t *sp;
	int instance = ACPIPWRBTN_MINOR_TO_INST(getminor((dev_t)arg));

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		sp = ddi_get_soft_state(acpipwrbtn_statep, instance);
		if (sp == NULL) {
			return (DDI_FAILURE);
		}
		*result = (void *)sp->dip;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)(uintptr_t)instance;
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}

static int
acpipwrbtn_open(dev_t *devp, int openflags __unused,
    int otyp, cred_t *credp __unused)
{
	acpipwrbtn_softstate_t *sp;
	int clone;

	if (otyp != OTYP_CHR) {
		return (EINVAL);
	}

	if ((sp = ddi_get_soft_state(acpipwrbtn_statep,
	    acpipwrbtn_inst)) == NULL) {
		return (ENXIO);
	}

	mutex_enter(&sp->mutex);
	for (clone = 1; clone < ACPIPWRBTN_MAX_CLONE; clone++) {
		if (!sp->clones[clone]) {
			break;
		}
	}

	if (clone == ACPIPWRBTN_MAX_CLONE) {
		mutex_exit(&sp->mutex);
		return (ENXIO);
	}

	*devp = makedevice(getmajor(*devp), (acpipwrbtn_inst << 8) + clone);
	sp->clones[clone] = 1;
	mutex_exit(&sp->mutex);

	return (0);
}

static int
acpipwrbtn_close(dev_t dev, int openflags __unused,
    int otyp, cred_t *credp __unused)
{
	acpipwrbtn_softstate_t *sp;
	int clone;

	if (otyp != OTYP_CHR) {
		return (EINVAL);
	}

	if ((sp = ddi_get_soft_state(acpipwrbtn_statep,
	    acpipwrbtn_inst)) == NULL) {
		return (ENXIO);
	}

	clone = ACPIPWRBTN_MINOR_TO_CLONE(getminor(dev));
	mutex_enter(&sp->mutex);
	if (sp->monitor_on == clone) {
		sp->monitor_on = 0;
	}
	sp->clones[clone] = 0;
	mutex_exit(&sp->mutex);

	return (0);
}

static int
acpipwrbtn_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
    cred_t *cred_p, int *rval_p __unused)
{
	acpipwrbtn_softstate_t *sp;
	int clone;

	if ((sp = ddi_get_soft_state(acpipwrbtn_statep,
	    acpipwrbtn_inst)) == NULL) {
		return (ENXIO);
	}

	clone = ACPIPWRBTN_MINOR_TO_CLONE(getminor(dev));
	switch (cmd) {
	case PB_BEGIN_MONITOR:
		mutex_enter(&sp->mutex);
		if (sp->monitor_on) {
			mutex_exit(&sp->mutex);
			return (EBUSY);
		}
		sp->monitor_on = clone;
		mutex_exit(&sp->mutex);
		return (0);

	case PB_END_MONITOR:
		mutex_enter(&sp->mutex);
		if (!sp->monitor_on) {
			mutex_exit(&sp->mutex);
			return (ENXIO);
		}
		if (sp->monitor_on != clone) {
			mutex_exit(&sp->mutex);
			return (EINVAL);
		}
		sp->monitor_on = 0;
		mutex_exit(&sp->mutex);
		return (0);

	case PB_GET_EVENTS:
		mutex_enter(&sp->mutex);
		if (ddi_copyout((void *)&sp->events, (void *)arg,
		    sizeof (int), mode) != 0) {
			mutex_exit(&sp->mutex);
			return (EFAULT);
		}
		sp->events = 0;
		mutex_exit(&sp->mutex);
		return (0);

	case PB_CREATE_BUTTON_EVENT:
		if (secpolicy_sys_config(cred_p, B_FALSE) != 0) {
			return (EPERM);
		}
		acpipwrbtn_shutdown(sp);
		return (0);

	default:
		return (ENOTTY);
	}
}

static int
acpipwrbtn_chpoll(dev_t dev __unused, short events __unused, int anyyet,
    short *reventsp, struct pollhead **phpp)
{
	acpipwrbtn_softstate_t *sp;

	if ((sp = ddi_get_soft_state(acpipwrbtn_statep,
	    acpipwrbtn_inst)) == NULL) {
		return (ENXIO);
	}

	mutex_enter(&sp->mutex);
	*reventsp = 0;
	if (sp->events) {
		*reventsp = POLLRDNORM | POLLIN;
	} else if (!anyyet) {
		*phpp = &sp->pollhd;
	}
	mutex_exit(&sp->mutex);

	return (0);
}

int
_init(void)
{
	int ret;

	ret = ddi_soft_state_init(&acpipwrbtn_statep,
	    sizeof (acpipwrbtn_softstate_t), 1);
	if (ret != 0) {
		return (ret);
	}

	ret = mod_install(&acpipwrbtn_modlinkage);
	if (ret != 0) {
		ddi_soft_state_fini(&acpipwrbtn_statep);
	}

	return (ret);
}

int
_fini(void)
{
	int ret;

	ret = mod_remove(&acpipwrbtn_modlinkage);
	if (ret == 0) {
		ddi_soft_state_fini(&acpipwrbtn_statep);
	}

	return (ret);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&acpipwrbtn_modlinkage, modinfop));
}
