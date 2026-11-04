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
 * Copyright 2021 Hayashi Naoyuki
 * Copyright 2026 OmniOS Community Edition (OmniOSce) Association.
 * Copyright 2026 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/machclock.h>
#include <sys/platform.h>
#include <sys/modctl.h>
#include <sys/platmod.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/errno.h>
#include <sys/byteorder.h>
#include <sys/cmn_err.h>
#include <sys/bootsvcs.h>
#include <sys/sunddi.h>
#include <sys/sysmacros.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_implfuncs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <vm/hat.h>
#include <sys/bcm2835_mbox.h>
#include <sys/bcm2835_vcprop.h>
#include <sys/bcm2835_vcio.h>
#include <sys/gpio.h>
#include <sys/smp_impldefs.h>
#include <sys/stdbool.h>

/*
 * Clock IDs from DT bindings
 *
 * These are the devicetree clock IDs, which will be internally mapped to
 * mailbox properties interface clock IDs.
 *
 * The comments after each define show the decimal value, the symbolic
 * name of the mailbox properties interface clock ID and the decimal
 * value of that clock ID.
 */
#define	DTPROP_CLK_UART		0x13	/* 19: VCPROP_CLK_UART (2) */
#define	DTPROP_CLK_VPU		0x14	/* 20: VCPROP_CLK_CORE (4) */
#define	DTPROP_CLK_EMMC		0x1C	/* 28: VCPROP_CLK_EMMC (1) */
#define	DTPROP_CLK_EMMC2	0x33	/* 51: VCPROP_CLK_EMMC2 (12) */

/*
 * Platform power management drivers list - empty by default
 */
char *platform_module_list[] = {
	NULL,
};

typedef enum {
	VCCLOCKID = 0,
	DTCLOCKID = 1,
} clockid_type_t;

void
plat_tod_fault(enum tod_fault_type tod_bad __unused)
{
}

static void
find_cprman(pnode_t node, void *arg)
{
	if (!prom_fdt_is_compatible(node, "brcm,bcm2711-cprman"))
		return;
	*(pnode_t *)arg = node;
}

static pnode_t
get_cprman_node(void)
{
	static kmutex_t cprman_node_lock;
	static volatile pnode_t cached_node = 0;

	if (cached_node == 0) {
		mutex_enter(&cprman_node_lock);
		if (cached_node == 0) {
			pnode_t node = 0;
			prom_fdt_walk(find_cprman, &node);
			cached_node = node;
		}
		mutex_exit(&cprman_node_lock);
	}

	return (cached_node);
}

static inline int
translate_clk_id_domain(clockid_type_t fromclkidtype,
    clockid_type_t toclkidtype, int clkid)
{
	if (fromclkidtype == toclkidtype)
		return (clkid);

	if (fromclkidtype == DTCLOCKID && toclkidtype == VCCLOCKID) {
		switch (clkid) {
		case DTPROP_CLK_UART: return (VCPROP_CLK_UART);
		case DTPROP_CLK_VPU: return (VCPROP_CLK_CORE);
		case DTPROP_CLK_EMMC: return (VCPROP_CLK_EMMC);
		case DTPROP_CLK_EMMC2: return (VCPROP_CLK_EMMC2);
		default: return (-1);
		}
	}

	cmn_err(CE_WARN, "unknown clock ID domain translation from ID type %d "
	    "to ID type %d", fromclkidtype, toclkidtype);

	return (-1);
}

static int
plat_vc_hwclock_rate(struct prom_hwclock *clk, clockid_type_t clkidtype,
    int vcproptag, int rate)
{
	if (!prom_fdt_is_compatible(clk->node, "brcm,bcm2711-cprman"))
		return (-1);

	int id = translate_clk_id_domain(clkidtype, VCCLOCKID, clk->id);
	if (id == -1)
		cmn_err(CE_PANIC, "unknown clock ID type");

	struct {
		struct vcprop_buffer_hdr	vb_hdr;
		struct vcprop_tag_clockrate	vbt_clockrate;
		struct vcprop_tag end;
	} vb = {
		.vb_hdr = {
			.vpb_len = sizeof (vb),
			.vpb_rcode = VCPROP_PROCESS_REQUEST,
		},
		.vbt_clockrate = {
			.tag = {
				.vpt_tag = vcproptag,
				.vpt_len = VCPROPTAG_LEN(vb.vbt_clockrate),
				.vpt_rcode = VCPROPTAG_REQUEST,
			},
			.id = id,
			.rate = rate,
		},
		.end = {
			.vpt_tag = VCPROPTAG_NULL,
		},
	};

	bcm2835_mbox_prop_send(&vb, sizeof (vb));

	if (!vcprop_buffer_success_p(&vb.vb_hdr))
		return (-1);
	if (!vcprop_tag_success_p(&vb.vbt_clockrate.tag))
		return (-1);

	return (vb.vbt_clockrate.rate);
}

uint64_t
plat_cpu_get_speed(cpu_t *cp __unused)
{
	pnode_t node;
	int clkhz;

	if ((node = get_cprman_node()) == 0) {
		cmn_err(CE_PANIC, "cprman is not found");
	}

	struct prom_hwclock clk = { node, VCPROP_CLK_ARM };
	clkhz = plat_vc_hwclock_rate(&clk, VCCLOCKID,
	    VCPROPTAG_GET_CLOCKRATE, 0);
	if (clkhz == -1)
		cmn_err(CE_PANIC, "unable to read CPU clock rate");

	return (clkhz);
}

void
plat_set_max_cpu_clock(int cpu_no)
{
	pnode_t node;
	int clkhz;

	if ((node = get_cprman_node()) == 0) {
		cmn_err(CE_PANIC, "cprman is not found");
	}

	struct prom_hwclock clk = { node, VCPROP_CLK_ARM };
	clkhz = plat_vc_hwclock_rate(&clk, VCCLOCKID,
	    VCPROPTAG_GET_MAX_CLOCKRATE, 0);
	if (clkhz == -1)
		cmn_err(CE_PANIC, "unable to read maximum CPU clock rate");
	clkhz = plat_vc_hwclock_rate(&clk, VCCLOCKID,
	    VCPROPTAG_SET_CLOCKRATE, clkhz);
	if (clkhz == -1)
		cmn_err(CE_PANIC, "unable to set CPU clock rate");
}

int
plat_clk_get_rate(dev_info_t *dip, uint_t clkno)
{
	struct prom_hwclock hwclk;

	if (prom_fdt_get_clock_by_index((pnode_t)ddi_get_nodeid(dip),
	    clkno, &hwclk) != 0) {
		return (-1);
	}

	if (prom_fdt_is_compatible(hwclk.node, "fixed-clock")) {
		uint_t clock;
		if (prom_getproplen(hwclk.node, "clock-frequency") ==
		    sizeof (uint_t)) {
			prom_getprop(hwclk.node, "clock-frequency",
			    (caddr_t)&clock);
			return (ntohl(clock));
		}
	}

	return (plat_vc_hwclock_rate(&hwclk,
	    DTCLOCKID, VCPROPTAG_GET_CLOCKRATE, 0));
}

int
plat_clk_get_rate_by_name(dev_info_t *dip, const char *clkname)
{
	struct prom_hwclock hwclk;

	if (prom_fdt_get_clock_by_name((pnode_t)ddi_get_nodeid(dip),
	    clkname, &hwclk) != 0) {
		return (-1);
	}

	if (prom_fdt_is_compatible(hwclk.node, "fixed-clock")) {
		uint_t clock;
		if (prom_getproplen(hwclk.node, "clock-frequency") ==
		    sizeof (uint_t)) {
			prom_getprop(hwclk.node, "clock-frequency",
			    (caddr_t)&clock);
			return (ntohl(clock));
		}
	}

	return (plat_vc_hwclock_rate(&hwclk,
	    DTCLOCKID, VCPROPTAG_GET_CLOCKRATE, 0));
}

int
plat_gpio_get(struct gpio_ctrl *gpio)
{
	int offset;
	if (prom_fdt_is_compatible(gpio->node, "raspberrypi,firmware-gpio")) {
		offset = 128;
	} else if (prom_fdt_is_compatible(gpio->node, "brcm,bcm2711-gpio")) {
		offset = 0;
	} else {
		return (-1);
	}

	struct {
		struct vcprop_buffer_hdr	vb_hdr;
		struct vcprop_tag_gpiostate	vbt_gpio;
		struct vcprop_tag end;
	} vb = {
		.vb_hdr = {
			.vpb_len = sizeof (vb),
			.vpb_rcode = VCPROP_PROCESS_REQUEST,
		},
		.vbt_gpio = {
			.tag = {
				.vpt_tag = VCPROPTAG_GET_GPIO_STATE,
				.vpt_len = VCPROPTAG_LEN(vb.vbt_gpio),
				.vpt_rcode = VCPROPTAG_REQUEST,
			},
			.gpio = gpio->pin + offset,
		},
		.end = {
			.vpt_tag = VCPROPTAG_NULL
		},
	};

	bcm2835_mbox_prop_send(&vb, sizeof (vb));

	if (!vcprop_buffer_success_p(&vb.vb_hdr))
		return (-1);
	if (!vcprop_tag_success_p(&vb.vbt_gpio.tag))
		return (-1);

	return (vb.vbt_gpio.state);
}

int
plat_gpio_set(struct gpio_ctrl *gpio, int value)
{
	int offset;
	if (prom_fdt_is_compatible(gpio->node, "raspberrypi,firmware-gpio")) {
		offset = VCPROP_EXP_GPIO_BASE;
	} else if (prom_fdt_is_compatible(gpio->node, "brcm,bcm2711-gpio")) {
		offset = 0;
	} else {
		return (-1);
	}

	struct {
		struct vcprop_buffer_hdr	vb_hdr;
		struct vcprop_tag_gpiostate	vbt_gpio;
		struct vcprop_tag end;
	} vb = {
		.vb_hdr = {
			.vpb_len = sizeof (vb),
			.vpb_rcode = VCPROP_PROCESS_REQUEST,
		},
		.vbt_gpio = {
			.tag = {
				.vpt_tag = VCPROPTAG_SET_GPIO_STATE,
				.vpt_len = VCPROPTAG_LEN(vb.vbt_gpio),
				.vpt_rcode = VCPROPTAG_REQUEST,
			},
			.gpio = gpio->pin + offset,
			.state = value,
		},
		.end = {
			.vpt_tag = VCPROPTAG_NULL
		},
	};

	bcm2835_mbox_prop_send(&vb, sizeof (vb));

	if (!vcprop_buffer_success_p(&vb.vb_hdr))
		return (-1);

	return (0);
}

/*
 * The BCM2711 GPIO block has 58 pins. Each pin's function is selected by a
 * 3-bit field in one of the GPFSEL registers, ten pins to a register.
 */
#define	BCM2711_GPIO_NPINS		58
#define	BCM2711_GPIO_GPFSEL(pin)	(((pin) / 10) * sizeof (uint32_t))
#define	BCM2711_GPIO_FSEL_SHIFT(pin)	(((pin) % 10) * 3)
#define	BCM2711_GPIO_FSEL_MASK		0x7
#define	BCM2711_GPIO_REGSIZE		\
	(BCM2711_GPIO_GPFSEL(BCM2711_GPIO_NPINS - 1) + sizeof (uint32_t))

/*
 * Serialises read-modify-write cycles on the GPFSEL registers.
 */
static kmutex_t rpi4_gpio_lock;

static int
rpi4_prop_u32_array(pnode_t node, const char *name, uint32_t **valp,
    uint_t *nvalp)
{
	int len = prom_getproplen(node, (caddr_t)name);

	if (len <= 0 || len % sizeof (uint32_t) != 0)
		return (ENOENT);

	uint32_t *val = kmem_alloc(len, KM_SLEEP);
	(void) prom_getprop(node, (caddr_t)name, (caddr_t)val);

	*nvalp = len / sizeof (uint32_t);
	for (uint_t i = 0; i < *nvalp; i++)
		val[i] = ntohl(val[i]);

	*valp = val;
	return (0);
}

/*
 * Find the index of the named pinctrl state for a device. Device tree nodes
 * list their state names in the optional "pinctrl-names" property. Where that
 * property is absent, the only state is the default, which is pinctrl-0.
 */
static int
rpi4_pinctrl_index(pnode_t node, const char *state)
{
	int len = prom_getproplen(node, "pinctrl-names");

	if (len <= 0)
		return (strcmp(state, "default") == 0 ? 0 : -1);

	char *names = kmem_alloc(len, KM_SLEEP);
	(void) prom_getprop(node, "pinctrl-names", names);

	int idx = -1;
	char *name = NULL;
	for (int i = 0;
	    (name = prom_decode_composite_string(names, len, name)) != NULL;
	    i++) {
		if (strcmp(name, state) == 0) {
			idx = i;
			break;
		}
	}

	kmem_free(names, len);
	return (idx);
}

static bool
rpi4_pinctrl_is_child(pnode_t parent, pnode_t node)
{
	for (pnode_t child = prom_childnode(parent); child > 0;
	    child = prom_nextnode(child)) {
		if (child == node)
			return (true);
	}

	return (false);
}

/*
 * Apply a single pin configuration node. These are children of the GPIO
 * controller and use the Broadcom binding, in which "brcm,pins" lists the
 * pins and "brcm,function" gives either one function for all of them or one
 * per pin.
 */
static int
rpi4_pinctrl_configure(pnode_t cfg)
{
	uint32_t *pins = NULL, *funcs = NULL;
	uint_t npins = 0, nfuncs = 0;
	uint64_t base, size;
	int ret;

	pnode_t gpio = prom_fdt_find_compatible(prom_rootnode(),
	    "brcm,bcm2711-gpio");
	if (gpio <= 0 || !rpi4_pinctrl_is_child(gpio, cfg))
		return (ENOTSUP);

	if (prom_getproplen(cfg, "brcm,pull") > 0) {
		cmn_err(CE_NOTE, "!pin configuration node %d has pull "
		    "settings, which are not supported and will be ignored",
		    cfg);
	}

	if ((ret = rpi4_prop_u32_array(cfg, "brcm,pins", &pins,
	    &npins)) != 0) {
		goto out;
	}

	if ((ret = rpi4_prop_u32_array(cfg, "brcm,function", &funcs,
	    &nfuncs)) != 0) {
		goto out;
	}

	if (nfuncs != 1 && nfuncs != npins) {
		ret = EINVAL;
		goto out;
	}

	for (uint_t i = 0; i < npins; i++) {
		if (pins[i] >= BCM2711_GPIO_NPINS ||
		    funcs[nfuncs == 1 ? 0 : i] > BCM2711_GPIO_FSEL_MASK) {
			ret = EINVAL;
			goto out;
		}
	}

	if (prom_fdt_get_reg_address(gpio, 0, &base) != 0 ||
	    prom_fdt_get_reg_size(gpio, 0, &size) != 0 ||
	    size < BCM2711_GPIO_REGSIZE) {
		ret = ENXIO;
		goto out;
	}

	caddr_t regs = psm_map_phys(base, BCM2711_GPIO_REGSIZE,
	    PROT_READ | PROT_WRITE);
	if (regs == NULL) {
		ret = ENOMEM;
		goto out;
	}

	mutex_enter(&rpi4_gpio_lock);
	for (uint_t i = 0; i < npins; i++) {
		volatile uint32_t *reg = (volatile uint32_t *)(regs +
		    BCM2711_GPIO_GPFSEL(pins[i]));
		uint_t shift = BCM2711_GPIO_FSEL_SHIFT(pins[i]);
		uint32_t val = *reg;

		val &= ~(BCM2711_GPIO_FSEL_MASK << shift);
		val |= funcs[nfuncs == 1 ? 0 : i] << shift;
		*reg = val;
	}
	mutex_exit(&rpi4_gpio_lock);

	psm_unmap_phys(regs, BCM2711_GPIO_REGSIZE);
	ret = 0;

out:
	if (pins != NULL)
		kmem_free(pins, npins * sizeof (uint32_t));
	if (funcs != NULL)
		kmem_free(funcs, nfuncs * sizeof (uint32_t));
	return (ret);
}

int
plat_pinctrl_select(dev_info_t *dip, const char *state)
{
	pnode_t node = ddi_get_nodeid(dip);
	char name[sizeof ("pinctrl-") + 10];
	uint32_t *cfgs;
	uint_t ncfgs;
	int idx, ret;

	if (node <= 0)
		return (ENOENT);

	if ((idx = rpi4_pinctrl_index(node, state)) < 0)
		return (ENOENT);

	if (snprintf(name, sizeof (name), "pinctrl-%d", idx) >= sizeof (name))
		return (ENOENT);
	if (rpi4_prop_u32_array(node, name, &cfgs, &ncfgs) != 0)
		return (ENOENT);

	ret = 0;
	for (uint_t i = 0; i < ncfgs; i++) {
		if ((ret = rpi4_pinctrl_configure((pnode_t)cfgs[i])) != 0)
			break;
	}

	kmem_free(cfgs, ncfgs * sizeof (uint32_t));
	return (ret);
}

int
plat_cpu_get_speeds(cpu_t *cp __unused, int **speedsp, int *nspeedsp)
{
	static int supp_speeds[] = {
		1500,
		1000,
		750,
		600,
	};
	int *speeds;
	int nspeeds = ARRAY_SIZE(supp_speeds);
	pnode_t node;

	if ((node = get_cprman_node()) == 0) {
		cmn_err(CE_PANIC, "cprman register is not found");
	}

	struct prom_hwclock clk = { node, VCPROP_CLK_ARM };
	int clkhz = plat_vc_hwclock_rate(&clk, VCCLOCKID,
	    VCPROPTAG_GET_MAX_CLOCKRATE, 0);

	speeds = kmem_alloc(nspeeds * sizeof (int), KM_SLEEP);
	memcpy(speeds, supp_speeds, nspeeds * sizeof (int));

	if (clkhz == -1) {
		cmn_err(CE_WARN, "unable to read maximum CPU clock rate; "
		    "assuming %d MHz", speeds[0]);
	} else {
		speeds[0] = (clkhz + 500000) / 1000000;
	}

	*speedsp = speeds;
	*nspeedsp = nspeeds;
	return (DDI_SUCCESS);
}

void
plat_cpu_free_speeds(int *speeds, int nspeeds)
{
	kmem_free(speeds, nspeeds * sizeof (int));
}

/*
 * Per-CPU requested speed tracking for DVFS.
 *
 * The RPi4 has a single clock domain for all CPUs, so we cannot set
 * per-CPU frequencies independently.  Instead, each CPU's governor
 * records its requested speed here, and the actual system-wide
 * frequency is set to the maximum across all CPUs so no busy CPU is
 * penalised by another's idle request.
 */
static int plat_cpu_req_speed[NCPU];
static kmutex_t plat_cpu_speed_lock;

int
plat_cpu_set_speed(cpu_t *cp, int speed)
{
	pnode_t node;
	int max_speed;
	int clkhz;
	int i;
	cpu_t *c;

	ASSERT3P(cp, !=, NULL);

	if ((node = get_cprman_node()) == 0) {
		return (DDI_FAILURE);
	}

	mutex_enter(&plat_cpu_speed_lock);

	plat_cpu_req_speed[cp->cpu_id] = speed;

	/* Find the maximum requested speed across all CPUs */
	max_speed = 0;
	for (i = 0; i < boot_max_ncpus; i++) {
		if (plat_cpu_req_speed[i] > max_speed) {
			max_speed = plat_cpu_req_speed[i];
		}
	}

	mutex_exit(&plat_cpu_speed_lock);

	if (max_speed <= 0) {
		return (DDI_FAILURE);
	}

	struct prom_hwclock clk = { node, VCPROP_CLK_ARM };
	clkhz = plat_vc_hwclock_rate(&clk, VCCLOCKID,
	    VCPROPTAG_SET_CLOCKRATE, max_speed * 1000000);
	if (clkhz == -1) {
		return (DDI_FAILURE);
	}

	/*
	 * The RPi4 has a single clock domain shared by all CPUs, so the
	 * actual hardware frequency is always the same for every core.
	 *
	 * Update cpu_curr_clock on all CPUs so kstat reflects the real
	 * hardware state rather than each governor's stale last readback.
	 */
	mutex_enter(&cpu_lock);
	c = cpu_list;
	do {
		c->cpu_curr_clock = (uint64_t)clkhz;
	} while ((c = c->cpu_next) != cpu_list);
	mutex_exit(&cpu_lock);

	return (DDI_SUCCESS);
}
