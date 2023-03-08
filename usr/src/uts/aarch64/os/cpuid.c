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

/* Copyright 2023 Richard Lowe */

/*
 * CPU Identification
 *
 * Determining the naming, etc, of processors, their features, and potentially
 * any workarounds they require.
 *
 * This is much simpler than on x86, but much less regular than on SPARC.
 */

#include <sys/cpu.h>
#include <sys/cpuid.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

struct cpu_partno {
	uint16_t part_id;
	const char *part_name;
};

struct cpu_impl {
	uint8_t impl_id;
	const char *impl_name;
	const struct cpu_partno *impl_part;
};

static const struct cpu_partno cpu_parts_arm[] = {
	{ MIDR_PART_ARM_FOUNDATION,	"Foundation" },
	{ MIDR_PART_ARM_CORTEX_A34,	"Cortex-A34" },
	{ MIDR_PART_ARM_CORTEX_A53,	"Cortex-A53" },
	{ MIDR_PART_ARM_CORTEX_A35,	"Cortex-A35" },
	{ MIDR_PART_ARM_CORTEX_A55,	"Cortex-A55" },
	{ MIDR_PART_ARM_CORTEX_A65,	"Cortex-A65" },
	{ MIDR_PART_ARM_CORTEX_A57,	"Cortex-A57" },
	{ MIDR_PART_ARM_CORTEX_A72,	"Cortex-A72" },
	{ MIDR_PART_ARM_CORTEX_A73,	"Cortex-A73" },
	{ MIDR_PART_ARM_CORTEX_A75,	"Cortex-A75" },
	{ MIDR_PART_ARM_CORTEX_A76,	"Cortex-A76" },
	{ MIDR_PART_ARM_NEOVERSE_N1,	"Neoverse-N1" },
	{ MIDR_PART_ARM_CORTEX_A77,	"Cortex-A77" },
	{ MIDR_PART_ARM_CORTEX_A76AE,	"Cortex-A76AE" },
	{ MIDR_PART_ARM_AEM_V8,		"AEMv8" },
	{ MIDR_PART_ARM_NEOVERSE_V1,	"Neoverse-V1" },
	{ MIDR_PART_ARM_CORTEX_A78,	"Cortex-A78" },
	{ MIDR_PART_ARM_CORTEX_A65AE,	"Cortex-A65AE" },
	{ MIDR_PART_ARM_CORTEX_X1,	"Cortex-X1" },
	{ MIDR_PART_ARM_CORTEX_A510,	"Cortex-A510" },
	{ MIDR_PART_ARM_CORTEX_A710,	"Cortex-A710" },
	{ MIDR_PART_ARM_CORTEX_X2,	"Cortex-X2" },
	{ MIDR_PART_ARM_NEOVERSE_N2,	"Neoverse-N2" },
	{ MIDR_PART_ARM_NEOVERSE_E1,	"Neoverse-E1" },
	{ MIDR_PART_ARM_CORTEX_A78C,	"Cortex-A78C" },
	{ MIDR_PART_ARM_CORTEX_X1C,	"Cortex-X1C" },
	{ MIDR_PART_ARM_CORTEX_A715,	"Cortex-A715" },
	{ MIDR_PART_ARM_CORTEX_X3,	"Cortex-X3" },
	{ MIDR_PART_ARM_NEOVERSE_V2,	"Neoverse-V2" },
	{ 0x0, NULL }
};

static const struct cpu_partno cpu_parts_cavium[] = {
	{ MIDR_PART_CAVIUM_THUNDERX,		"ThunderX" },
	{ MIDR_PART_CAVIUM_THUNDERX_81XX,	"ThunderX 81xx" },
	{ MIDR_PART_CAVIUM_THUNDERX_83XX,	"ThunderX 83xx" },
	{ MIDR_PART_CAVIUM_THUNDERX2,		"ThunderX2" },
	{ MIDR_PART_CAVIUM_OCTX2_98XX,		"OcteonX2 98xx" },
	{ MIDR_PART_CAVIUM_OCTX2_96XX,		"OcteonX2 96xx" },
	{ MIDR_PART_CAVIUM_OCTX2_95XX,		"OcteonX2 95xx" },
	{ MIDR_PART_CAVIUM_OCTX2_95XXN,		"OcteonX2 95xxN" },
	{ MIDR_PART_CAVIUM_OCTX2_95XXMM,	"OcteonX2 95xxMM" },
	{ MIDR_PART_CAVIUM_OCTX2_95XXO,		"OcteonX2 95xxO" },
	{ 0x0, NULL }
};

static const struct cpu_partno cpu_parts_amc[] = {
	{ MIDR_PART_AMC_POTENZA,	"Potenza" },
	{ 0x0, NULL }
};

static const struct cpu_partno cpu_parts_qualcomm[] = {
	{ MIDR_PART_QUALCOMM_KRYO,		"Kryo" },
	{ MIDR_PART_QUALCOMM_KRYO2xx_GOLD,	"Kryo 2xx Gold" },
	{ MIDR_PART_QUALCOMM_KRYO2xx_SILVER,	"Kryo 2xx Silver" },
	{ MIDR_PART_QUALCOMM_FALKOR,		"Falkor" },
	{ MIDR_PART_QUALCOMM_KRYO3xx_SILVER,	"Kryo 3xx Silver" },
	{ MIDR_PART_QUALCOMM_KRYO4xx_SILVER,	"Kryo 4xx Silver" },
	{ MIDR_PART_QUALCOMM_KRYO4xx_GOLD,	"Kryo 4xx Gold" },
	{ 0x0, NULL }
};

static const struct cpu_partno cpu_parts_broadcom[] = {
	{ MIDR_PART_BROADCOM_BRAHMA_B53,	"Brahma B53" },
	{ MIDR_PART_BROADCOM_VULCAN,		"Vulcan" },
	{ 0x0, NULL }
};

static const struct cpu_partno cpu_parts_nvidia[] = {
	{ MIDR_PART_NVIDIA_DENVER,	"Denver" },
	{ MIDR_PART_NVIDIA_CARMEL,	"Carmel" },
	{ 0x0, NULL }
};

static const struct cpu_partno cpu_parts_fujitsu[] = {
	{ MIDR_PART_FUJITSU_A64FX,	"A64FX" },
	{ 0x0, NULL }
};

static const struct cpu_partno cpu_parts_apple[] = {
	{ MIDR_PART_APPLE_M1_ICESTORM,		"M1 (Icestorm)" },
	{ MIDR_PART_APPLE_M1_FIRESTORM,		"M1 (Firestorm)" },
	{ MIDR_PART_APPLE_M1_ICESTORM_PRO,	"M1 Pro (Icestorm)" },
	{ MIDR_PART_APPLE_M1_FIRESTORM_PRO,	"M1 Pro (Firestorm)" },
	{ MIDR_PART_APPLE_M1_ICESTORM_MAX,	"M1 Max (Icestorm)" },
	{ MIDR_PART_APPLE_M1_FIRESTORM_MAX,	"M1 Max (Firestorm)" },
	{ MIDR_PART_APPLE_M2_BLIZZARD,		"M2 (Blizzard)" },
	{ MIDR_PART_APPLE_M2_AVALANCHE,		"M2 (Avalanche)" },
	{ 0x0, NULL }
};

static const struct cpu_partno cpu_parts_ampere[] = {
	{ MIDR_PART_AMPERE_AMPERE1,	"Ampere1" },
	{ 0x0, NULL }
};

static const struct cpu_impl cpu_impls[] = {
	{ MIDR_IMPL_AMPERE,	"Ampere",	cpu_parts_ampere },
	{ MIDR_IMPL_ARM,	"ARM",		cpu_parts_arm },
	{ MIDR_IMPL_BROADCOM,	"Broadcom",	cpu_parts_broadcom },
	{ MIDR_IMPL_CAVIUM,	"Cavium",	cpu_parts_cavium },
	{ MIDR_IMPL_DEC,	"DEC",		NULL },
	{ MIDR_IMPL_FUJITSU,	"Fujitsu",	cpu_parts_fujitsu },
	{ MIDR_IMPL_INFINEON,	"Infineon",	NULL },
	{ MIDR_IMPL_MOTOROLA,	"Motorola",	NULL },
	{ MIDR_IMPL_NVIDIA,	"Nvidia",	cpu_parts_nvidia },
	{ MIDR_IMPL_AMC,	"AMC",		cpu_parts_amc },
	{ MIDR_IMPL_QUALCOMM,	"Qualcomm",	cpu_parts_qualcomm },
	{ MIDR_IMPL_MARVELL,	"Marvell",	NULL },
	{ MIDR_IMPL_APPLE,	"Apple",	cpu_parts_apple },
	{ MIDR_IMPL_INTEL,	"Intel",	NULL },
	{ 0x0,	NULL, NULL },
};

void
cpuid_implementer(const cpu_t *cpu, char *s, size_t n)
{
	uint8_t impl = MIDR_IMPL(cpu->cpu_m.mcpu_midr);
	char *vendor = NULL;

	for (const struct cpu_impl *ci = cpu_impls; ci->impl_id != 0x0; ci++) {
		if (ci->impl_id == impl) {
			strlcat(s, ci->impl_name, n);
			return;
		}
	}

	snprintf(s, n, "Unknown [%x]", impl);
}

void
cpuid_partname(const cpu_t *cpu, char *s, size_t n)
{
	uint8_t impl = MIDR_IMPL(cpu->cpu_m.mcpu_midr);
	uint16_t partid = MIDR_PART(cpu->cpu_m.mcpu_midr);
	const struct cpu_impl *ci;
	const struct cpu_partno *cp;
	char *part = NULL;

	for (ci = cpu_impls; ci->impl_id != 0x0; ci++) {
		if ((ci->impl_id != impl) || (ci->impl_part == NULL))
			continue;

		for (cp = ci->impl_part; cp->part_name != NULL; cp++) {
			if (cp->part_id == partid) {
				strlcat(s, cp->part_name, n);
				return;
			}
		}
	}

	snprintf(s, n, "Unknown [%x]", partid);
}

void
cpuid_brandstr(const cpu_t *cpu, char *s, size_t n)
{
	uint8_t impl = MIDR_IMPL(cpu->cpu_m.mcpu_midr);
	uint16_t partid = MIDR_PART(cpu->cpu_m.mcpu_midr);
	const struct cpu_impl *ci;
	const struct cpu_partno *cp;
	char *part = NULL;

	for (ci = cpu_impls; ci->impl_id != 0x0; ci++) {
		if ((ci->impl_id != impl) || (ci->impl_part == NULL))
			continue;

		for (cp = ci->impl_part; cp->part_name != NULL; cp++) {
			if (cp->part_id == partid) {
				snprintf(s, n, "%s %s @ %ld MHz", ci->impl_name,
				    cp->part_name,
				    cpu->cpu_curr_clock / 1000 / 1000);
				return;
			}
		}

		snprintf(s, n, "%s Unknown [%x] @ %ld MHz", ci->impl_name,
		    partid, cpu->cpu_curr_clock / 1000 / 1000);
	}
}
