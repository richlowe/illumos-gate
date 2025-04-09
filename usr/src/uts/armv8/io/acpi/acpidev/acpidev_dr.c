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
 * ACPI-based system board Dynamic Reconfiguration (DR) stubs for aarch64.
 *
 * DR is not yet supported on aarch64.  Every public entry point returns
 * "not capable" or "not supported" so that callers (acpidev_drv,
 * acpidev_container, acpidev_memory, acpidev_pci, acpinex) can call
 * unconditionally without #ifdef guards.
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cpuvar.h>
#include <sys/memlist.h>
#include <sys/sunddi.h>
#include <sys/acpi/acpi.h>
#include <sys/acpica.h>
#include <sys/acpidev.h>
#include <sys/acpidev_rsc.h>
#include <sys/acpidev_dr.h>
#include <sys/acpidev_impl.h>

int acpidev_dr_enable = 0;

/*
 * SRAT and SLIT table pointers are cached here for use by acpidev_cpu.c
 * (proximity domain lookup) and other consumers.  The CPU probe path
 * populates acpidev_srat_tbl_ptr via AcpiGetTable(); the SLIT pointer
 * is unused on aarch64 today but is declared extern in acpidev_impl.h.
 */
ACPI_TABLE_SRAT *acpidev_srat_tbl_ptr;

int
acpidev_dr_capable(void)
{
	return (0);
}

uint32_t
acpidev_dr_max_boards(void)
{
	return (0);
}

uint32_t
acpidev_dr_max_mem_units_per_board(void)
{
	return (0);
}

uint32_t
acpidev_dr_max_io_units_per_board(void)
{
	return (0);
}

uint32_t
acpidev_dr_max_cmp_units_per_board(void)
{
	return (0);
}

uint32_t
acpidev_dr_max_cpu_units_per_cmp(void)
{
	return (0);
}

uint32_t
acpidev_dr_max_segments_per_mem_device(void)
{
	return (0);
}

uint32_t
acpidev_dr_max_memlists_per_segment(void)
{
	return (0);
}

ACPI_STATUS
acpidev_dr_get_mem_alignment(ACPI_HANDLE hdl __unused, uint64_t *ap __unused)
{
	return (AE_SUPPORT);
}

void
acpidev_dr_init(void)
{
}

void
acpidev_dr_check(acpidev_walk_info_t *infop __unused)
{
}

ACPI_STATUS
acpidev_dr_initialize(dev_info_t *pdip __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_get_board_handle(uint_t board __unused, ACPI_HANDLE *hdlp __unused)
{
	return (AE_SUPPORT);
}

acpidev_board_type_t
acpidev_dr_get_board_type(ACPI_HANDLE hdl __unused)
{
	return (ACPIDEV_INVALID_BOARD);
}

ACPI_STATUS
acpidev_dr_get_board_number(ACPI_HANDLE hdl __unused,
    uint32_t *bnump __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_get_board_name(ACPI_HANDLE hdl __unused, char *buf __unused,
    size_t len __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_get_attachment_point(ACPI_HANDLE hdl __unused, char *buf __unused,
    size_t len __unused)
{
	return (AE_SUPPORT);
}

acpidev_class_id_t
acpidev_dr_device_get_class(ACPI_HANDLE hdl __unused)
{
	return (ACPIDEV_CLASS_ID_INVALID);
}

ACPI_STATUS
acpidev_dr_device_get_memory_index(ACPI_HANDLE hdl __unused,
    uint32_t *idxp __unused)
{
	return (AE_SUPPORT);
}

int
acpidev_dr_device_is_board(ACPI_HANDLE hdl __unused)
{
	return (0);
}

int
acpidev_dr_device_is_present(ACPI_HANDLE hdl __unused)
{
	return (0);
}

int
acpidev_dr_device_is_powered(ACPI_HANDLE hdl __unused)
{
	return (0);
}

int
acpidev_dr_device_hotplug_capable(ACPI_HANDLE hdl __unused)
{
	return (0);
}

int
acpidev_dr_device_has_edl(ACPI_HANDLE hdl __unused)
{
	return (0);
}

int
acpidev_dr_device_getprop(ACPI_HANDLE hdl __unused, char *name __unused,
    caddr_t buf __unused, size_t len __unused)
{
	return (0);
}

ACPI_STATUS
acpidev_dr_device_get_regspec(ACPI_HANDLE hdl __unused,
    boolean_t assigned __unused, acpidev_regspec_t **regpp __unused,
    uint_t *cntp __unused)
{
	return (AE_SUPPORT);
}

void
acpidev_dr_device_free_regspec(acpidev_regspec_t *regp __unused,
    uint_t count __unused)
{
}

ACPI_STATUS
acpidev_dr_device_walk_edl(ACPI_HANDLE hdl __unused,
    ACPI_WALK_CALLBACK cb __unused, void *arg __unused,
    void **retval __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_device_walk_ejd(ACPI_HANDLE hdl __unused,
    ACPI_WALK_CALLBACK cb __unused, void *arg __unused,
    void **retval __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_device_walk_device(ACPI_HANDLE hdl __unused,
    uint_t max_lvl __unused, ACPI_WALK_CALLBACK cb __unused,
    void *arg __unused, void **retval __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_device_check_status(ACPI_HANDLE hdl __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_device_poweron(ACPI_HANDLE hdl __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_device_poweroff(ACPI_HANDLE hdl __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_device_insert(ACPI_HANDLE hdl __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_device_remove(ACPI_HANDLE hdl __unused)
{
	return (AE_SUPPORT);
}

void
acpidev_dr_lock_all(void)
{
}

void
acpidev_dr_unlock_all(void)
{
}

ACPI_STATUS
acpidev_dr_allocate_cpuid(ACPI_HANDLE hdl __unused,
    processorid_t *idp __unused)
{
	return (AE_SUPPORT);
}

ACPI_STATUS
acpidev_dr_free_cpuid(ACPI_HANDLE hdl __unused)
{
	return (AE_SUPPORT);
}

int
acpidev_dr_get_cpu_numa_info(cpu_t *cp __unused, void **hdlpp __unused,
    uint32_t *apicidp __unused, uint32_t *pxmidp __unused,
    uint32_t *slicntp __unused, uchar_t **slipp __unused)
{
	return (-1);
}

void
acpidev_dr_free_cpu_numa_info(void *hdlp __unused)
{
}

ACPI_STATUS
acpidev_dr_get_mem_numa_info(ACPI_HANDLE hdl __unused,
    struct memlist *ml __unused, void **hdlpp __unused,
    uint32_t *pxmidp __unused, uint32_t *slicntp __unused,
    uchar_t **slipp __unused)
{
	return (AE_SUPPORT);
}

void
acpidev_dr_free_mem_numa_info(void *hdlp __unused)
{
}
