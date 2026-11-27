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
 * Arm Architected timer device tree node creation from GTDT.
 */

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_subrdefs.h>
#include <sys/ddi_impldefs.h>
#include <sys/stdbool.h>
#include <sys/obpdefs.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev_arm_gtmr.h>
#include <sys/acpidev_gic.h>
#include <sys/gic_reg.h>

/*
 * See Documentation/devicetree/bindings/arm/arch_timer.txt in
 * the Linux kernel tree, or on https://www.kernel.org/doc/.
 */

/*
 * ACPI 6.2 adds two new fields to communicate the virtual EL2 timer.
 *
 * ACPICA does this hack to get visibility of the virtual EL2 timers, but not
 * in our version. Once we have an updated ACPICA we can replace references to
 * XACPI_GTDT_EL2 with ACPI_GTDT_EL2 and delete this struct.
 */
typedef struct {
	UINT32				VirtualEL2TimerGsiv;
	UINT32				VirtualEL2TimerFlags;
} XACPI_GTDT_EL2;

static int
acpidev_arm_gtmr_setup_device_node(void)
{
	int				rv;
	ACPI_STATUS			st;
	ACPI_TABLE_GTDT			*gtdt;
	XACPI_GTDT_EL2			*gtdt2;
	dev_info_t			*rdip;
	uint_t				nirqs;
	struct acpidev_gic_interrupt	irqs[5];
	char				*compatible[] = { "arm,armv8-timer" };
	bool				always_on;

	gtdt = NULL;
	rdip = NULL;

	if ((st = AcpiGetTable(ACPI_SIG_GTDT, 1,
	    (ACPI_TABLE_HEADER **)&gtdt)) != AE_OK) {
		cmn_err(CE_PANIC, "Unable to get the GTDT, which is mandatory "
		    "on Arm systems. Error 0x%x", st);
	}
	VERIFY3P(gtdt, !=, NULL);

	nirqs = 4;
	gtdt2 = NULL;
	if (gtdt->Header.Revision > 2 &&
	    gtdt->Header.Length >= sizeof (ACPI_TABLE_GTDT) +
	    sizeof (XACPI_GTDT_EL2)) {
		gtdt2 = (XACPI_GTDT_EL2 *)(((char *)gtdt) +
		    sizeof (ACPI_TABLE_GTDT));
	}

	irqs[0].gi_gsiv = gtdt->SecureEl1Interrupt;
	if (gtdt->SecureEl1Flags & ACPI_GTDT_INTERRUPT_MODE) {
		irqs[0].gi_flags = AGIF_EDGE;
		if (gtdt->SecureEl1Flags & ACPI_GTDT_INTERRUPT_POLARITY) {
			irqs[0].gi_flags |= AGIF_FALLING;
		}
	} else {
		irqs[0].gi_flags = AGIF_LEVEL;
		if (gtdt->SecureEl1Flags & ACPI_GTDT_INTERRUPT_POLARITY) {
			irqs[0].gi_flags |= AGIF_ACTIVE_LOW;
		}
	}

	irqs[1].gi_gsiv = gtdt->NonSecureEl1Interrupt;
	if (gtdt->NonSecureEl1Flags & ACPI_GTDT_INTERRUPT_MODE) {
		irqs[1].gi_flags = AGIF_EDGE;
		if (gtdt->NonSecureEl1Flags & ACPI_GTDT_INTERRUPT_POLARITY) {
			irqs[1].gi_flags |= AGIF_FALLING;
		}
	} else {
		irqs[1].gi_flags = AGIF_LEVEL;
		if (gtdt->NonSecureEl1Flags & ACPI_GTDT_INTERRUPT_POLARITY) {
			irqs[1].gi_flags |= AGIF_ACTIVE_LOW;
		}
	}

	irqs[2].gi_gsiv = gtdt->VirtualTimerInterrupt;
	if (gtdt->VirtualTimerFlags & ACPI_GTDT_INTERRUPT_MODE) {
		irqs[2].gi_flags = AGIF_EDGE;
		if (gtdt->VirtualTimerFlags & ACPI_GTDT_INTERRUPT_POLARITY) {
			irqs[2].gi_flags |= AGIF_FALLING;
		}
	} else {
		irqs[2].gi_flags = AGIF_LEVEL;
		if (gtdt->VirtualTimerFlags & ACPI_GTDT_INTERRUPT_POLARITY) {
			irqs[2].gi_flags |= AGIF_ACTIVE_LOW;
		}
	}

	irqs[3].gi_gsiv = gtdt->NonSecureEl2Interrupt;
	if (gtdt->NonSecureEl2Flags & ACPI_GTDT_INTERRUPT_MODE) {
		irqs[3].gi_flags = AGIF_EDGE;
		if (gtdt->NonSecureEl2Flags & ACPI_GTDT_INTERRUPT_POLARITY) {
			irqs[3].gi_flags |= AGIF_FALLING;
		}
	} else {
		irqs[3].gi_flags = AGIF_LEVEL;
		if (gtdt->NonSecureEl2Flags & ACPI_GTDT_INTERRUPT_POLARITY) {
			irqs[3].gi_flags |= AGIF_ACTIVE_LOW;
		}
	}

	always_on = (
	    (gtdt->SecureEl1Flags & ACPI_GTDT_ALWAYS_ON) &&
	    (gtdt->NonSecureEl1Flags & ACPI_GTDT_ALWAYS_ON) &&
	    (gtdt->VirtualTimerFlags & ACPI_GTDT_ALWAYS_ON) &&
	    (gtdt->NonSecureEl2Flags & ACPI_GTDT_ALWAYS_ON));

	/*
	 * Only set up the virtual EL2 interrupt if ACPI can communicate this
	 * to us and a value is set (indicating that VHE is supported).
	 */
	if (gtdt2 != NULL && gtdt2->VirtualEL2TimerGsiv != 0) {
		nirqs = 5;
		irqs[4].gi_gsiv = gtdt2->VirtualEL2TimerGsiv;
		if (gtdt2->VirtualEL2TimerFlags & ACPI_GTDT_INTERRUPT_MODE) {
			irqs[4].gi_flags = AGIF_EDGE;
			if (gtdt2->VirtualEL2TimerFlags &
			    ACPI_GTDT_INTERRUPT_POLARITY) {
				irqs[4].gi_flags |= AGIF_FALLING;
			}
		} else {
			irqs[4].gi_flags = AGIF_LEVEL;
			if (gtdt2->VirtualEL2TimerFlags &
			    ACPI_GTDT_INTERRUPT_POLARITY) {
				irqs[4].gi_flags |= AGIF_ACTIVE_LOW;
			}
		}

		if (always_on) {
			if (!(gtdt2->VirtualEL2TimerFlags &
			    ACPI_GTDT_ALWAYS_ON)) {
				always_on = false;
			}
		}
	}

	ndi_devi_alloc_sleep(ddi_root_node(), "timer",
	    (pnode_t)DEVI_SID_NODEID, &rdip);

	if ((rv = ndi_prop_update_string_array(DDI_DEV_T_NONE, rdip,
	    OBP_COMPATIBLE, compatible, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if ((rv = e_ddi_prop_update_int(DDI_DEV_T_NONE, rdip,
	    DDI_NO_AUTODETACH, 1)) != DDI_PROP_SUCCESS) {
		goto out;
	}

	if (ACPI_FAILURE(acpidev_set_gsiv_interrupts(rdip, irqs, nirqs))) {
		rv = DDI_FAILURE;
		goto out;
	}

	if (always_on) {
		if ((rv = ddi_prop_create(DDI_DEV_T_NONE, rdip,
		    DDI_PROP_CANSLEEP|DDI_PROP_HW_DEF,
		    "always-on", NULL, 0)) != DDI_PROP_SUCCESS) {
			goto out;
		}
	}

	if ((rv = ndi_devi_bind_driver(rdip, 0)) != NDI_SUCCESS) {
		goto out;
	}

out:
	if (rv != DDI_SUCCESS) {
		ndi_devi_offline(rdip, NDI_DEVI_REMOVE);
	}

	AcpiPutTable((ACPI_TABLE_HEADER *)gtdt);
	return (rv);
}


ACPI_STATUS
acpidev_create_arm_gtmr_node(void)
{
	if (acpidev_arm_gtmr_setup_device_node() == DDI_SUCCESS) {
		return (AE_OK);
	}

	return (AE_ERROR);
}
