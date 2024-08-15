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
 * Copyright 2017 Hayashi Naoyuki
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/promif.h>
#include <sys/salib.h>
#include <sys/byteorder.h>
#include <sys/sysmacros.h>
#include <sys/controlregs.h>
#include <sys/arch_timer.h>
#include <sys/platmod.h>

#include <sys/boot.h>
#include <sys/gxbb_smc.h>
#include <sys/miiregs.h>
#include <sys/ethernet.h>
#include "prom_dev.h"
#include "dwmac.h"
#include "dwmacreg.h"
#include "boot_plat.h"

#define TX_DESC_NUM 32
#define RX_DESC_NUM 48

#define ENET_ALIGN  DCACHE_LINE
#define BUFFER_SIZE 1536

struct emac_sc
{
	uint64_t base;
	uint8_t mac_addr[6];
	int phy_id;
	int phy_speed;
	int phy_fullduplex;


	struct emac_desc *tx_desc;
	struct emac_desc *rx_desc;
	int tx_index;
	int rx_head;
	caddr_t tx_buffer;
	caddr_t rx_buffer;
	paddr_t tx_buffer_phys;
	paddr_t tx_desc_phys;
	paddr_t rx_desc_phys;
	paddr_t rx_buffer_phys;
};

static struct emac_sc *emac_dev[3];

static void
emac_usecwait(int usec)
{
	arch_timer_udelay(usec);
}

static void
emac_cache_flush(void *addr, size_t len)
{
	for (uintptr_t v = P2ALIGN((uintptr_t)addr, DCACHE_LINE); v < (uintptr_t)addr + len; v += DCACHE_LINE) {
		flush_data_cache(v);
	}
	dsb(sy);
}

static int
emac_alloc_buffer(struct emac_sc *sc)
{
	size_t size = 0;
	size += sizeof(struct emac_desc) * TX_DESC_NUM;
	size = roundup(size, ENET_ALIGN);
	size += sizeof(struct emac_desc) * RX_DESC_NUM;
	size = roundup(size, ENET_ALIGN);
	size += BUFFER_SIZE * TX_DESC_NUM;
	size = roundup(size, ENET_ALIGN);
	size += BUFFER_SIZE * RX_DESC_NUM;

	size_t alloc_size = size + 2 * MMU_PAGESIZE;
	uintptr_t orig_addr = (uintptr_t)kmem_alloc(alloc_size, 0);
	uintptr_t buf_addr = roundup(orig_addr, MMU_PAGESIZE);
	size_t buf_size = roundup(size, MMU_PAGESIZE);
	uintptr_t buf_vaddr = memlist_get(buf_size, MMU_PAGESIZE, &ptmplistp);

	map_phys(PTE_UXN | PTE_PXN | PTE_AF | PTE_SH_INNER | PTE_AP_KRWUNA | PTE_ATTR_STRONG, (caddr_t)buf_vaddr, buf_addr, buf_size);
	emac_cache_flush((caddr_t)orig_addr, alloc_size);
	size_t offset = 0;
	sc->tx_desc_phys = (paddr_t)(buf_addr + offset);
	sc->tx_desc = (struct emac_desc *)(buf_vaddr + offset);
	offset += sizeof(struct emac_desc) * TX_DESC_NUM;

	offset = roundup(offset, ENET_ALIGN);
	sc->rx_desc_phys = (paddr_t)(buf_addr + offset);
	sc->rx_desc = (struct emac_desc *)(buf_vaddr + offset);
	offset += sizeof(struct emac_desc) * RX_DESC_NUM;

	offset = roundup(offset, ENET_ALIGN);
	sc->tx_buffer_phys = (paddr_t)(buf_addr + offset);
	sc->tx_buffer = (caddr_t)(buf_vaddr + offset);
	offset += BUFFER_SIZE * TX_DESC_NUM;

	offset = roundup(offset, ENET_ALIGN);
	sc->rx_buffer_phys = (paddr_t)(buf_addr + offset);
	sc->rx_buffer = (caddr_t)(buf_vaddr + offset);
	offset += BUFFER_SIZE * RX_DESC_NUM;

	memset(sc->tx_desc, 0, sizeof(struct emac_desc) * TX_DESC_NUM);
	memset(sc->rx_desc, 0, sizeof(struct emac_desc) * RX_DESC_NUM);
	memset(sc->tx_buffer, 0, BUFFER_SIZE * TX_DESC_NUM);
	memset(sc->rx_buffer, 0, BUFFER_SIZE * RX_DESC_NUM);

	return 0;
}

static int
gxbb_get_macaddr(struct emac_sc *sc)
{
	uint64_t ret = gxbb_efuse_read(52, 6);
	if (ret != 6)
		return -1;

	emac_cache_flush((void*)gxbb_share_mem_out_base(), 6);
	volatile const uint8_t *src = (volatile const uint8_t *)gxbb_share_mem_out_base();
	uint8_t *dst = sc->mac_addr;
	for (int i = 0; i < 6; i++)
		*dst++ = *src++;

	return 0;
}

static int
emac_match(const char *name)
{
	pnode_t node = prom_finddevice(name);
	if (node <= 0)
		return 0;
	if (prom_is_compatible(node, "amlogic,gxbb-rgmii-dwmac"))
		return 1;
	return 0;
}

static void
emac_reg_write(struct emac_sc *sc, size_t offset, uint32_t val)
{
	*(volatile uint32_t *)(sc->base + offset) = val;
}

static uint32_t
emac_reg_read(struct emac_sc *sc, size_t offset)
{
	return *(volatile uint32_t *)(sc->base + offset);
}

static void
emac_mii_write(struct emac_sc *sc, int offset, uint16_t val)
{
	union emac_gmii_address gmii_address;
	gmii_address.dw = emac_reg_read(sc, EMAC_GMII_ADDRESS);
	if (gmii_address.gb)
		return;

	union emac_gmii_data gmii_data = {0};
	gmii_data.gd = val;
	emac_reg_write(sc, EMAC_GMII_DATA, gmii_data.dw);
	gmii_address.dw = 0;
	gmii_address.gb = 1;
	gmii_address.gw = 1;
	gmii_address.cr = 1;
	gmii_address.gr = offset;
	gmii_address.pa = sc->phy_id;
	emac_reg_write(sc, EMAC_GMII_ADDRESS, gmii_address.dw);

	for (int i = 0; i < 1000; i++) {
		emac_usecwait(100);
		gmii_address.dw = emac_reg_read(sc, EMAC_GMII_ADDRESS);
		if (gmii_address.gb == 0) {
			break;
		}
	}
}

static uint16_t
emac_mii_read(struct emac_sc *sc, int offset)
{
	uint16_t data = 0xffff;

	union emac_gmii_address gmii_address;
	gmii_address.dw = emac_reg_read(sc, EMAC_GMII_ADDRESS);

	if (gmii_address.gb == 0) {
		gmii_address.dw = 0;
		gmii_address.gb = 1;
		gmii_address.gw = 0;
		gmii_address.cr = 1;
		gmii_address.gr = offset;
		gmii_address.pa = sc->phy_id;
		emac_reg_write(sc, EMAC_GMII_ADDRESS, gmii_address.dw);

		for (int i = 0; i < 1000; i++) {
			emac_usecwait(100);
			gmii_address.dw = emac_reg_read(sc, EMAC_GMII_ADDRESS);
			if (gmii_address.gb == 0) {
				break;
			}
		}

		if (gmii_address.gb == 0) {
			union emac_gmii_data gmii_data;
			gmii_data.dw = emac_reg_read(sc, EMAC_GMII_DATA);
			data = gmii_data.gd;
		}
	}

	return data;
}

static int
emac_phy_reset(struct emac_sc *sc)
{
	uint16_t advert = emac_mii_read(sc, MII_AN_ADVERT) & 0x1F;
	advert |= MII_ABILITY_100BASE_TX_FD;
	advert |= MII_ABILITY_100BASE_TX;
	advert |= MII_ABILITY_10BASE_T_FD;
	advert |= MII_ABILITY_10BASE_T;
	uint16_t gigctrl =  MII_MSCONTROL_1000T_FD | MII_MSCONTROL_1000T;

	emac_mii_write(sc, MII_AN_ADVERT, advert);
	emac_mii_write(sc, MII_MSCONTROL, gigctrl);

	uint16_t bmcr = MII_CONTROL_ANE | MII_CONTROL_RSAN | MII_CONTROL_1GB | MII_CONTROL_FDUPLEX;
	emac_mii_write(sc, MII_CONTROL, bmcr);

	int i;
	uint16_t bmsr = 0;
	for (i = 0; i < 10000; i++) {
		emac_usecwait(1000);
		bmsr = emac_mii_read(sc, MII_STATUS);
		if (bmsr == 0xffff)
			continue;
		if (bmsr & MII_STATUS_LINKUP)
			break;
	}
	if (i == 10000 || !(bmsr & MII_STATUS_LINKUP))
		return -1;

	uint16_t lpar = emac_mii_read(sc, MII_AN_LPABLE);
	uint16_t msstat = emac_mii_read(sc, MII_MSSTATUS);
	if (msstat & MII_MSSTATUS_LP1000T_FD) {
		sc->phy_speed = 1000;
		sc->phy_fullduplex = 1;
	} else if (msstat & MII_MSSTATUS_LP1000T) {
		sc->phy_speed = 1000;
		sc->phy_fullduplex = 0;
	} else if (lpar & MII_ABILITY_100BASE_TX_FD) {
		sc->phy_speed = 100;
		sc->phy_fullduplex = 1;
	} else if (lpar & MII_ABILITY_100BASE_TX) {
		sc->phy_speed = 100;
		sc->phy_fullduplex = 0;
	} else if (lpar & MII_ABILITY_10BASE_T_FD) {
		sc->phy_speed = 10;
		sc->phy_fullduplex = 1;
	} else if (lpar & MII_ABILITY_10BASE_T) {
		sc->phy_speed = 10;
		sc->phy_fullduplex = 0;
	} else {
		sc->phy_speed = 0;
		sc->phy_fullduplex = 0;
	}

	return 0;
}

static void
emac_setup_rx_buffer(struct emac_sc *sc, int i)
{
	volatile struct emac_desc *rx_desc = &sc->rx_desc[i];
	dsb(sy);
	rx_desc->addr = (uint32_t)(sc->rx_buffer_phys + BUFFER_SIZE * i);
	rx_desc->next = (uint32_t)(sc->rx_desc_phys + sizeof (struct emac_desc) * ((i + 1) % RX_DESC_NUM));
	rx_desc->cntl = EMAC_DESC_CNTL_CHAIN | BUFFER_SIZE;
	dsb(sy);
	rx_desc->status = EMAC_DESC_STATUS_OWN;
}

static void
emac_setup_tx_buffer(struct emac_sc *sc, int i)
{
	volatile struct emac_desc *tx_desc = &sc->tx_desc[i];
	tx_desc->addr = (uint32_t)(sc->tx_buffer_phys + BUFFER_SIZE * i);
	tx_desc->next = (uint32_t)(sc->tx_desc_phys + sizeof (struct emac_desc) * ((i + 1) % TX_DESC_NUM));
	tx_desc->cntl = 0;
	tx_desc->status = 0;
}
static int
emac_pinmux(pnode_t node, const char *pinname)
{
	int len;
	int pinctrl_index = prom_get_prop_index(node, "pinctrl-names", pinname);
	if (pinctrl_index < 0)
		return -1;
	char buf[80];
	sprintf(buf, "pinctrl-%d", pinctrl_index);

	len = prom_getproplen(node, buf);
	if (len != sizeof(uint32_t))
		return -1;
	uint32_t pinctrl;
	prom_getprop(node, buf, (caddr_t)&pinctrl);
	pnode_t pinctrl_node;
	pinctrl_node = prom_findnode_by_phandle(ntohl(pinctrl));
	if (pinctrl_node < 0)
		return -1;

	return plat_pinmux_set(pinctrl_node);
}

static int
get_reg_addr(pnode_t node, int index, uint64_t *reg)
{
	uint64_t addr;
	if (prom_get_reg(node, index, &addr) != 0)
		return -1;

	pnode_t parent = prom_parentnode(node);
	while (parent > 0) {
		if (prom_is_compatible(parent, "simple-bus")) {
			int len = prom_getproplen(parent, "ranges");
			if (len > 0) {
				int address_cells = prom_get_prop_int(parent, "#address-cells", 2);
				int size_cells = prom_get_prop_int(parent, "#size-cells", 2);
				int parent_address_cells  = prom_get_prop_int(prom_parentnode(parent), "#address-cells", 2);

				if ((len % (sizeof(uint32_t) * (address_cells + parent_address_cells + size_cells))) == 0) {
					uint32_t *ranges = __builtin_alloca(len);
					prom_getprop(parent, "ranges", (caddr_t)ranges);
					int ranges_cells = (address_cells + parent_address_cells + size_cells);

					for (int i = 0; i < len / (sizeof(uint32_t) * ranges_cells); i++) {
						uint64_t base = 0;
						uint64_t target = 0;
						uint64_t size = 0;
						for (int j = 0; j < address_cells; j++) {
							base <<= 32;
							base += ntohl(ranges[ranges_cells * i + j]);
						}
						for (int j = 0; j < parent_address_cells; j++) {
							target <<= 32;
							target += ntohl(ranges[ranges_cells * i + address_cells + j]);
						}
						for (int j = 0; j < size_cells; j++) {
							size <<= 32;
							size += ntohl(ranges[ranges_cells * i + address_cells + parent_address_cells + j]);
						}

						if (base <= addr && addr <= base + size - 1) {
							addr = (addr - base) + target;
							break;
						}
					}
				}
			}
		}
		parent = prom_parentnode(parent);
	}
	*reg = addr;
	return 0;
}

static int
emac_open(const char *name)
{
	pnode_t node = prom_finddevice(name);
	if (node <= 0)
		return -1;
	if (!prom_is_compatible(node, "amlogic,gxbb-rgmii-dwmac"))
		return -1;

	int fd;

	for (fd = 0; fd < sizeof(emac_dev) / sizeof(emac_dev[0]); fd++) {
		if (emac_dev[fd] == NULL)
			break;
	}
	if (fd == sizeof(emac_dev) / sizeof(emac_dev[0]))
		return -1;
	struct emac_sc *sc = kmem_alloc(sizeof(struct emac_sc), 0);

	if (emac_pinmux(node, "eth_pins") < 0)
		return -1;

	// power-on
	struct prom_hwreset hwreset;
	if (prom_get_reset(node, 0, &hwreset) == 0) {
		if (plat_hwreset_assert(&hwreset) != 0)
			return false;
		if (plat_hwreset_deassert(&hwreset) != 0)
			return false;
	}

	if (gxbb_get_macaddr(sc))
		return -1;

	if (get_reg_addr(node, 0, &sc->base) != 0)
		return -1;

	uint64_t preg_eth_addr0;
	if (get_reg_addr(node, 1, &preg_eth_addr0) != 0)
		return -1;

	if (prom_getproplen(node, "mc_val") != sizeof(uint32_t))
		return -1;

	uint32_t mc_val;
	prom_getprop(node, "mc_val", (caddr_t)&mc_val);
	mc_val = ntohl(mc_val);
	*(volatile uint32_t *)preg_eth_addr0 = mc_val;

	emac_reg_write(sc, EMAC_MAC_CONF, 0);

	// stop
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, 0);
	union emac_dma_operation_mode operation_mode = {0};
	operation_mode.ftf = 1;
	operation_mode.dff = 1;
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, operation_mode.dw);
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, 0);

	// reset
	union emac_dma_bus_mode bus_mode = {0};
	bus_mode.swr = 1;
	emac_reg_write(sc, EMAC_DMA_BUS_MODE, bus_mode.dw);
	dsb(sy);
	for (int i = 0; i < 1000; i++) {
		emac_usecwait(100);
		bus_mode.dw = emac_reg_read(sc, EMAC_DMA_BUS_MODE);
		if (bus_mode.swr == 0)
			break;
	}

	if (bus_mode.swr)
		return -1;

	// detect phy
	sc->phy_id = -1;
	for (int i = 0; i < 32; i++) {
		int phy_id = (i + 1) % 32;
		union emac_gmii_address gmii_address = {0};
		gmii_address.dw = 0;
		gmii_address.gb = 1;
		gmii_address.gw = 0;
		gmii_address.cr = 1;
		gmii_address.gr = 0;
		gmii_address.pa = phy_id;
		emac_reg_write(sc, EMAC_GMII_ADDRESS, gmii_address.dw);

		for (int i = 0; i < 1000; i++) {
			emac_usecwait(100);
			gmii_address.dw = emac_reg_read(sc, EMAC_GMII_ADDRESS);
			if (gmii_address.gb == 0) {
				break;
			}
		}

		if (gmii_address.gb)
			return -1;

		union emac_gmii_data gmii_data;
		gmii_data.dw = emac_reg_read(sc, EMAC_GMII_DATA);
		uint32_t mii_data = gmii_data.gd;

		if (mii_data != 0 && mii_data != 0xffff) {
			sc->phy_id = phy_id;
			break;
		}
	}
	if (sc->phy_id < 0)
		return -1;

	if (emac_phy_reset(sc))
		return -1;

	if (emac_alloc_buffer(sc))
		return -1;

	for (int i = 0; i < RX_DESC_NUM; i++)
		emac_setup_rx_buffer(sc, i);

	for (int i = 0; i < TX_DESC_NUM; i++)
		emac_setup_tx_buffer(sc, i);

	union emac_mac_conf mac_conf = {0};
	mac_conf.be = 1;
	mac_conf.acs= 1;
	mac_conf.dcrs= 1;
	mac_conf.dm  = (sc->phy_fullduplex? 1: 0);
	mac_conf.fes = ((sc->phy_speed == 100)? 1: 0);
	mac_conf.ps  = ((sc->phy_speed != 1000)? 1: 0);
	emac_reg_write(sc, EMAC_MAC_CONF, mac_conf.dw);

	union emac_mac_frame_filter mac_frame_filter = {0};
	mac_frame_filter.hmc = 1;
	emac_reg_write(sc, EMAC_MAC_FRAME_FILTER, mac_frame_filter.dw);

	emac_reg_write(sc, EMAC_MAC_HASHTABLE_HIGH, 0);
	emac_reg_write(sc, EMAC_MAC_HASHTABLE_LOW, 0);
	union {
		uint32_t dw[2];
		uint8_t dev_addr[6];
	} mac_addr = {0};
	memcpy(mac_addr.dev_addr, sc->mac_addr, sizeof(sc->mac_addr));

	emac_reg_write(sc, EMAC_MAC_ADDRESS_HIGH(0), mac_addr.dw[1]);
	emac_reg_write(sc, EMAC_MAC_ADDRESS_LOW(0),  mac_addr.dw[0]);

	bus_mode.dw = 0;
	bus_mode.eightxpbl = 1;
	bus_mode.pbl = 8;
	bus_mode.rpbl = 8;
	emac_reg_write(sc, EMAC_DMA_BUS_MODE, bus_mode.dw);

	// interrupt disable
	emac_reg_write(sc, EMAC_DMA_INT_ENABLE,		0);
	emac_reg_write(sc, EMAC_INT_MASK,		0xffffffff);
	emac_reg_write(sc, EMAC_MMC_TX_INT_MASK,	0xffffffff);
	emac_reg_write(sc, EMAC_MMC_RX_INT_MASK,	0xffffffff);
	emac_reg_write(sc, EMAC_MMC_IPC_RX_INT_MASK,	0xffffffff);

	// interrupt clear
	emac_reg_write(sc, EMAC_INT_STATUS,		0xffffffff);
	emac_reg_read(sc, EMAC_INT_STATUS);
	emac_reg_read(sc, EMAC_MII_CONTROL_STATUS);
	emac_reg_read(sc, EMAC_MMC_RX_INT);
	emac_reg_read(sc, EMAC_MMC_TX_INT);
	emac_reg_read(sc, EMAC_MMC_IPC_RX_INT);

	emac_reg_write(sc, EMAC_DMA_TX_DESC_ADDRESS, sc->tx_desc_phys);
	emac_reg_write(sc, EMAC_DMA_RX_DESC_ADDRESS, sc->rx_desc_phys);

	mac_conf.dw = emac_reg_read(sc, EMAC_MAC_CONF);
	mac_conf.te = 1;
	mac_conf.re = 1;
	emac_reg_write(sc, EMAC_MAC_CONF, mac_conf.dw);

	operation_mode.dw = 0;
	operation_mode.sr = 1;
	operation_mode.osf = 1;
	operation_mode.st = 1;
	operation_mode.tsf = 1;
	operation_mode.rsf = 1;
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, operation_mode.dw);

	char *str;
	str = "bootp";
	prom_setprop(prom_chosennode(), "net-config-strategy", (caddr_t)str, strlen(str) + 1);
	str = "ethernet,100,rj45,full";
	prom_setprop(prom_chosennode(), "network-interface-type", (caddr_t)str, strlen(str) + 1);

	emac_dev[fd] = sc;
	return fd;
}

static ssize_t
emac_send(int dev, caddr_t data, size_t packet_length, uint_t startblk)
{
	if (!(0 <= dev && dev < sizeof(emac_dev) / sizeof(emac_dev[0])))
		return -1;

	struct emac_sc *sc = emac_dev[dev];
	if (!sc)
		return -1;

	if (packet_length > BUFFER_SIZE)
		return -1;

	int index = sc->tx_index;
	volatile struct emac_desc *tx_desc = &sc->tx_desc[index];

	while (tx_desc->status & EMAC_DESC_STATUS_OWN) {}
	caddr_t buffer = sc->tx_buffer + BUFFER_SIZE * index;
	memcpy(buffer, data, packet_length);

	tx_desc->cntl = EMAC_TXDESC_CNTL_LS | EMAC_TXDESC_CNTL_FS | EMAC_DESC_CNTL_CHAIN | ((packet_length < ETHERMIN) ? ETHERMIN: packet_length);
	dsb(sy);
	tx_desc->status = EMAC_DESC_STATUS_OWN;
	emac_reg_write(sc, EMAC_DMA_TX_POLL_DEMAND, 0xffffffff);

	sc->tx_index = (sc->tx_index + 1) % TX_DESC_NUM;

	return packet_length;
}

static ssize_t
emac_recv(int dev, caddr_t buf, size_t buf_len, uint_t startblk)
{
	if (!(0 <= dev && dev < sizeof(emac_dev) / sizeof(emac_dev[0])))
		return -1;

	struct emac_sc *sc = emac_dev[dev];
	if (!sc)
		return -1;

	int index = sc->rx_head;
	size_t len = 0;

	volatile struct emac_desc *rx_desc = &sc->rx_desc[index];
	dsb(sy);
	uint32_t status = rx_desc->status;
	dsb(sy);
	if (status & EMAC_DESC_STATUS_OWN) {
		return 0;
	}

	if ((status & EMAC_RXDESC_STATUS_ES) == 0 &&
		    (status & (EMAC_RXDESC_STATUS_FS | EMAC_RXDESC_STATUS_LD)) == (EMAC_RXDESC_STATUS_FS | EMAC_RXDESC_STATUS_LD)) {
		len = EMAC_RXDESC_STATUS_FL(status);
		if (len >= 64) {
			len -= 4;
			caddr_t buffer = sc->rx_buffer + BUFFER_SIZE * index;
			memcpy(buf, buffer, len);
		}
	}

	emac_setup_rx_buffer(sc, index);
	index = (index + 1) % RX_DESC_NUM;
	sc->rx_head = index;

	return len;
}

static int
emac_getmacaddr(ihandle_t dev, caddr_t ea)
{
	if (!(0 <= dev && dev < sizeof(emac_dev) / sizeof(emac_dev[0])))
		return -1;

	struct emac_sc *sc = emac_dev[dev];
	if (!sc)
		return -1;
	memcpy(ea, sc->mac_addr, 6);
	return 0;
}

static int
emac_close(int dev)
{
	if (!(0 <= dev && dev < sizeof(emac_dev) / sizeof(emac_dev[0])))
		return -1;
	struct emac_sc *sc = emac_dev[dev];
	if (!sc)
		return -1;

	union emac_mac_conf mac_conf;
	mac_conf.dw = emac_reg_read(sc, EMAC_MAC_CONF);
	mac_conf.te = 0;
	mac_conf.re = 0;
	emac_reg_write(sc, EMAC_MAC_CONF, mac_conf.dw);

	// stop
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, 0);
	union emac_dma_operation_mode operation_mode;
	operation_mode.dw = emac_reg_read(sc, EMAC_DMA_OPERATION_MODE);
	operation_mode.ftf = 1;
	operation_mode.dff = 1;
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, operation_mode.dw);
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, 0);

	emac_dev[dev] = NULL;
	return 0;
}

static struct prom_dev emac_prom_dev =
{
	.match = emac_match,
	.open = emac_open,
	.write = emac_send,
	.read = emac_recv,
	.close = emac_close,
	.getmacaddr = emac_getmacaddr,
};

void init_dwmac(void)
{
	prom_register(&emac_prom_dev);
}
