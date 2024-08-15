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

#include <sys/promif.h>
#include <sys/miiregs.h>
#include <sys/ethernet.h>
#include <sys/byteorder.h>
#include <sys/controlregs.h>
#include <sys/debug.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/vlan.h>
#include <sys/mac.h>
#include <sys/mac_ether.h>
#include <sys/strsun.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/ddi_impldefs.h>
#include <sys/gxbb_smc.h>
#include <sys/crc32.h>
#include <sys/sysmacros.h>
#include <sys/platmod.h>
#include <sys/machparam.h>
#include "dwmac.h"
#include "dwmacreg.h"

#define	EMAC_DMA_BUFFER_SIZE	1536

static ddi_device_acc_attr_t mem_acc_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_NEVERSWAP_ACC,
	DDI_STRICTORDER_ACC,
};

static ddi_device_acc_attr_t reg_acc_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_NEVERSWAP_ACC,
	DDI_STRICTORDER_ACC,
};

static ddi_dma_attr_t dma_attr = {
	DMA_ATTR_V0,			/* dma_attr_version	*/
	0x0000000000000000ull,		/* dma_attr_addr_lo	*/
	0x00000000FFFFFFFFull,		/* dma_attr_addr_hi	*/
	0x00000000FFFFFFFFull,		/* dma_attr_count_max	*/
	0x0000000000000001ull,		/* dma_attr_align	*/
	0x00000FFF,			/* dma_attr_burstsizes	*/
	0x00000001,			/* dma_attr_minxfer	*/
	0x00000000FFFFFFFFull,		/* dma_attr_maxxfer	*/
	0x00000000FFFFFFFFull,		/* dma_attr_seg		*/
	1,				/* dma_attr_sgllen	*/
	0x00000001,			/* dma_attr_granular	*/
	DDI_DMA_FLAGERR			/* dma_attr_flags	*/
};

static void emac_destroy(struct emac_sc *sc);
static void emac_m_stop(void *arg);

static void
emac_reg_write(struct emac_sc *sc, uint32_t offset, uint32_t val)
{
	void *addr = sc->reg.addr + offset;
	ddi_put32(sc->reg.handle, addr, val);
}

static uint32_t
emac_reg_read(struct emac_sc *sc, uint32_t offset)
{
	void *addr = sc->reg.addr + offset;
	return ddi_get32(sc->reg.handle, addr);
}

static void
emac_usecwait(int usec)
{
	drv_usecwait(usec);
}

static pnode_t
emac_get_node(struct emac_sc *sc)
{
	return ddi_get_nodeid(sc->dip);
}
static void
emac_mutex_enter(struct emac_sc *sc)
{
	mutex_enter(&sc->intrlock);
}
static void
emac_mutex_exit(struct emac_sc *sc)
{
	mutex_exit(&sc->intrlock);
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

static void
emac_gmac_reset(struct emac_sc *sc)
{
	pnode_t node = emac_get_node(sc);

	emac_pinmux(node, "eth_pins");

	struct prom_hwreset hwreset;
	if (prom_get_reset(node, 0, &hwreset) == 0) {
		plat_hwreset_assert(&hwreset);
		plat_hwreset_deassert(&hwreset);
	}

	ddi_acc_handle_t handle;
	caddr_t addr;
	if (ddi_regs_map_setup(sc->dip, 1, &addr, 0, 0, &reg_acc_attr, &handle) == DDI_SUCCESS) {
		if (prom_getproplen(node, "mc_val") == sizeof(uint32_t)) {
			uint32_t mc_val;
			prom_getprop(node, "mc_val", (caddr_t)&mc_val);
			mc_val = ntohl(mc_val);
			ddi_put32(handle, (uint32_t *)addr, mc_val);
		}
		ddi_regs_map_free(&handle);
	}

	emac_reg_write(sc, EMAC_MAC_CONF, 0);

	// stop
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, 0);
	union emac_dma_operation_mode operation_mode;
	operation_mode.dw = emac_reg_read(sc, EMAC_DMA_OPERATION_MODE);
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
}

static void
emac_gmac_init(struct emac_sc *sc)
{
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

	union emac_mac_conf mac_conf = {0};
	mac_conf.be = 1;
	mac_conf.acs= 1;
	mac_conf.dcrs= 1;
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
	memcpy(mac_addr.dev_addr, sc->dev_addr, sizeof(sc->dev_addr));

	emac_reg_write(sc, EMAC_MAC_ADDRESS_HIGH(0), mac_addr.dw[1]);
	emac_reg_write(sc, EMAC_MAC_ADDRESS_LOW(0),  mac_addr.dw[0]);

	union emac_dma_bus_mode busmode = {0};
	busmode.eightxpbl = 1;
	busmode.pbl = 8;
	busmode.rpbl = 8;
	emac_reg_write(sc, EMAC_DMA_BUS_MODE, busmode.dw);

	union emac_dma_int_enable int_enable = {0};
	int_enable.tie = 1;
	int_enable.rie = 1;
	int_enable.nie = 1;
	int_enable.aie = 1;
	int_enable.fbe = 1;
	int_enable.une = 1;
	emac_reg_write(sc, EMAC_DMA_INT_ENABLE, int_enable.dw);
}

static void
emac_gmac_update(struct emac_sc *sc)
{
	union emac_mac_conf mac_conf;
	mac_conf.dw = emac_reg_read(sc, EMAC_MAC_CONF);

	mac_conf.dm  = ((sc->phy_duplex == LINK_DUPLEX_FULL)? 1: 0);
	mac_conf.fes = ((sc->phy_speed == 100)? 1: 0);
	mac_conf.ps  = ((sc->phy_speed != 1000)? 1: 0);

	emac_reg_write(sc, EMAC_MAC_CONF, mac_conf.dw);
}


static void
emac_gmac_enable(struct emac_sc *sc)
{
	emac_reg_write(sc, EMAC_DMA_TX_DESC_ADDRESS, sc->tx_ring.desc.dmac_addr);
	emac_reg_write(sc, EMAC_DMA_RX_DESC_ADDRESS, sc->rx_ring.desc.dmac_addr);

	union emac_mac_conf mac_conf;
	mac_conf.dw = emac_reg_read(sc, EMAC_MAC_CONF);
	mac_conf.te = 1;
	mac_conf.re = 1;
	emac_reg_write(sc, EMAC_MAC_CONF, mac_conf.dw);

	union emac_dma_operation_mode operation_mode = {0};
	operation_mode.sr = 1;
	operation_mode.osf = 1;
	operation_mode.st = 1;
	operation_mode.tsf = 1;
	operation_mode.rsf = 1;
	emac_reg_write(sc, EMAC_DMA_OPERATION_MODE, operation_mode.dw);
}

static void
emac_gmac_disable(struct emac_sc *sc)
{
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
}

static void
emac_free_tx(struct emac_sc *sc, int idx)
{
	if (sc->tx_ring.mp[idx]) {
		freemsg(sc->tx_ring.mp[idx]);
		sc->tx_ring.mp[idx] = NULL;
	}
}

static void
emac_free_packet(struct emac_packet *pkt)
{
	struct emac_sc *sc = pkt->sc;
	if (sc->running && sc->rx_pkt_num < RX_PKT_NUM_MAX) {
		pkt->mp = desballoc((unsigned char *)pkt->dma.addr, EMAC_DMA_BUFFER_SIZE, BPRI_MED, &pkt->free_rtn);
	} else {
		pkt->mp = NULL;
	}
	if (pkt->mp == NULL) {
		ddi_dma_unbind_handle(pkt->dma.dma_handle);
		ddi_dma_mem_free(&pkt->dma.mem_handle);
		ddi_dma_free_handle(&pkt->dma.dma_handle);
		kmem_free(pkt, sizeof(struct emac_packet));
	} else {
		ddi_dma_sync(pkt->dma.dma_handle, 0, pkt->dma.size, DDI_DMA_SYNC_FORDEV);
		mutex_enter(&sc->rx_pkt_lock);
		pkt->next = sc->rx_pkt_free;
		sc->rx_pkt_free = pkt;
		sc->rx_pkt_num++;
		mutex_exit(&sc->rx_pkt_lock);
	}
}

static struct emac_packet *
emac_alloc_packet(struct emac_sc *sc)
{
	struct emac_packet *pkt;
	ddi_dma_attr_t desc_dma_attr = dma_attr;
	desc_dma_attr.dma_attr_align = DCACHE_LINE;

	mutex_enter(&sc->rx_pkt_lock);
	pkt = sc->rx_pkt_free;
	if (pkt) {
		sc->rx_pkt_free = pkt->next;
		sc->rx_pkt_num--;
	}
	mutex_exit(&sc->rx_pkt_lock);

	if (pkt == NULL) {
		pkt = (struct emac_packet *)kmem_zalloc(sizeof(struct emac_packet), KM_NOSLEEP);
		if (pkt) {
			if (ddi_dma_alloc_handle(sc->dip, &desc_dma_attr, DDI_DMA_SLEEP, 0, &pkt->dma.dma_handle) != DDI_SUCCESS) {
				kmem_free(pkt, sizeof(struct emac_packet));
				pkt= NULL;
			}
		}

		if (pkt) {
			if (ddi_dma_mem_alloc(pkt->dma.dma_handle, EMAC_DMA_BUFFER_SIZE, &mem_acc_attr, DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, 0,
				    &pkt->dma.addr, &pkt->dma.size, &pkt->dma.mem_handle)) {
				ddi_dma_free_handle(&pkt->dma.dma_handle);
				kmem_free(pkt, sizeof(struct emac_packet));
				pkt= NULL;
			} else {
				ASSERT(pkt->dma.size >= EMAC_DMA_BUFFER_SIZE);
	 		}
		}

		if (pkt) {
			ddi_dma_cookie_t cookie;
			uint_t ccount;
			int result = ddi_dma_addr_bind_handle(pkt->dma.dma_handle, NULL, pkt->dma.addr, pkt->dma.size, DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
			    DDI_DMA_SLEEP, NULL, &cookie, &ccount);
			if (result == DDI_DMA_MAPPED) {
				ASSERT(ccount == 1);
				pkt->dma.dmac_addr = cookie.dmac_laddress;
				ASSERT((cookie.dmac_laddress & (DCACHE_LINE - 1)) == 0);
				ASSERT(cookie.dmac_size <= EMAC_DMA_BUFFER_SIZE);
				pkt->sc = sc;
				pkt->free_rtn.free_func = emac_free_packet;
				pkt->free_rtn.free_arg = (char *)pkt;

				pkt->mp = desballoc((unsigned char *)pkt->dma.addr, EMAC_DMA_BUFFER_SIZE, BPRI_MED, &pkt->free_rtn);
				if (pkt->mp == NULL) {
					ddi_dma_unbind_handle(pkt->dma.dma_handle);
					ddi_dma_mem_free(&pkt->dma.mem_handle);
					ddi_dma_free_handle(&pkt->dma.dma_handle);
					kmem_free(pkt, sizeof(struct emac_packet));
					pkt= NULL;
				}
			} else {
				ddi_dma_mem_free(&pkt->dma.mem_handle);
				ddi_dma_free_handle(&pkt->dma.dma_handle);
				kmem_free(pkt, sizeof(struct emac_packet));
				pkt= NULL;
			}
		}
	}

	return pkt;
}

static boolean_t
emac_alloc_desc_ring(struct emac_sc *sc, struct emac_dma *desc_dma, size_t align, size_t size)
{
	ddi_dma_attr_t desc_dma_attr = dma_attr;
	desc_dma_attr.dma_attr_align = align;

	if (ddi_dma_alloc_handle(sc->dip, &desc_dma_attr, DDI_DMA_SLEEP, 0, &desc_dma->dma_handle) != DDI_SUCCESS) {
		return (B_FALSE);
	}

	if (ddi_dma_mem_alloc(desc_dma->dma_handle, size, &mem_acc_attr, DDI_DMA_CONSISTENT | IOMEM_DATA_UC_WR_COMBINE, DDI_DMA_SLEEP, 0,
		    &desc_dma->addr, &desc_dma->size, &desc_dma->mem_handle)) {
		return (B_FALSE);
	}

	ddi_dma_cookie_t cookie;
	uint_t ccount;
	int result = ddi_dma_addr_bind_handle(
	    desc_dma->dma_handle, NULL, desc_dma->addr, desc_dma->size, DDI_DMA_RDWR | DDI_DMA_CONSISTENT,
	    DDI_DMA_SLEEP, NULL, &cookie, &ccount);
	if (result == DDI_DMA_MAPPED) {
		ASSERT(ccount == 1);
	} else {
		return (B_FALSE);
	}
	ASSERT(desc_dma->size >= size);
	desc_dma->dmac_addr = cookie.dmac_laddress;

	return (B_TRUE);
}

static void
emac_free_desc_ring(struct emac_dma *desc_dma)
{
	if (desc_dma->dmac_addr)
		ddi_dma_unbind_handle(desc_dma->dma_handle);
	desc_dma->dmac_addr = 0;

	if (desc_dma->mem_handle)
		ddi_dma_mem_free(&desc_dma->mem_handle);
	desc_dma->mem_handle = 0;

	if (desc_dma->dma_handle)
		ddi_dma_free_handle(&desc_dma->dma_handle);
	desc_dma->dma_handle = 0;
}

static boolean_t
emac_alloc_buffer(struct emac_sc *sc)
{
	int len;

	for (int index = 0; index < RX_DESC_NUM; index++) {
		struct emac_packet *pkt = emac_alloc_packet(sc);
		if (!pkt)
			return (B_FALSE);
		sc->rx_ring.pkt[index] = pkt;
		struct emac_desc *desc_p = (struct emac_desc *)(sc->rx_ring.desc.addr + sizeof(struct emac_desc) * index);
		desc_p->status = EMAC_DESC_STATUS_OWN;
		desc_p->cntl = EMAC_DESC_CNTL_CHAIN | pkt->dma.size;
		desc_p->addr = pkt->dma.dmac_addr;
		desc_p->next = sc->rx_ring.desc.dmac_addr + sizeof(struct emac_desc) * ((index + 1) % RX_DESC_NUM);
	}
	sc->rx_ring.index = 0;

	for (int index = 0; index < TX_DESC_NUM; index++) {
		struct emac_packet *pkt = emac_alloc_packet(sc);
		if (!pkt)
			return (B_FALSE);
		sc->tx_ring.pkt[index] = pkt;
		struct emac_desc *desc_p = (struct emac_desc *)(sc->tx_ring.desc.addr + sizeof(struct emac_desc) * index);
		desc_p->status = 0;
		desc_p->cntl = EMAC_DESC_CNTL_CHAIN;
		desc_p->addr = pkt->dma.dmac_addr;
		desc_p->next = sc->tx_ring.desc.dmac_addr + sizeof(struct emac_desc) * ((index + 1) % TX_DESC_NUM);
	}
	sc->tx_ring.head = 0;
	sc->tx_ring.tail = 0;

	return (B_TRUE);
}

static void
emac_free_buffer(struct emac_sc *sc)
{
	for (int i = 0; i < TX_DESC_NUM; i++) {
		struct emac_packet *pkt = sc->tx_ring.pkt[i];
		if (pkt) {
			freemsg(pkt->mp);
			sc->tx_ring.pkt[i] = NULL;
		}
		emac_free_tx(sc, i);

		struct emac_desc desc = {0};
		*(struct emac_desc *)(sc->tx_ring.desc.addr + sizeof(struct emac_desc) * i) = desc;
	}

	for (int i = 0; i < RX_DESC_NUM; i++) {
		struct emac_packet *pkt = sc->rx_ring.pkt[i];
		if (pkt) {
			freemsg(pkt->mp);
			sc->rx_ring.pkt[i] = NULL;
		}
		struct emac_desc desc = {0};
		*(struct emac_desc *)(sc->rx_ring.desc.addr + sizeof(struct emac_desc) * i) = desc;
	}

	mutex_enter(&sc->rx_pkt_lock);
	for (;;) {
		struct emac_packet *pkt = sc->rx_pkt_free;
		if (pkt == NULL)
			break;
		sc->rx_pkt_free = pkt->next;
		sc->rx_pkt_num--;
		mutex_exit(&sc->rx_pkt_lock);
		freemsg(pkt->mp);
		mutex_enter(&sc->rx_pkt_lock);
	}
	mutex_exit(&sc->rx_pkt_lock);
}

static boolean_t
emac_get_macaddr(struct emac_sc *sc)
{
	uint64_t ret = gxbb_efuse_read(52, 6);
	if (ret != 6)
		return (B_FALSE);

	memcpy(sc->dev_addr, (void *)(gxbb_share_mem_out_base() + SEGKPM_BASE), 6);
	return (B_TRUE);
}

static void
emac_destroy(struct emac_sc *sc)
{
	if (sc->intr_handle) {
		ddi_intr_disable(sc->intr_handle);
		ddi_intr_remove_handler(sc->intr_handle);
		ddi_intr_free(sc->intr_handle);
	}
	sc->intr_handle = 0;

	if (sc->mii_handle)
		mii_free(sc->mii_handle);
	sc->mii_handle = 0;

	if (sc->mac_handle) {
		mac_unregister(sc->mac_handle);
		mac_free(sc->macp);
	}
	sc->mac_handle = 0;

	emac_free_buffer(sc);

	emac_free_desc_ring(&sc->tx_ring.desc);
	emac_free_desc_ring(&sc->rx_ring.desc);

	if (sc->reg.handle)
		ddi_regs_map_free(&sc->reg.handle);
	sc->reg.handle = 0;

	ddi_set_driver_private(sc->dip, NULL);
	struct emac_mcast *mc;
	while ((mc = list_head(&sc->mcast)) != NULL) {
		list_remove(&sc->mcast, mc);
		kmem_free(mc, sizeof (*mc));
	}
	list_destroy(&sc->mcast);
	mutex_destroy(&sc->intrlock);
	mutex_destroy(&sc->rx_pkt_lock);
	kmem_free(sc, sizeof (*sc));
}

static boolean_t
emac_init(struct emac_sc *sc)
{
	if (!emac_get_macaddr(sc))
		return (B_FALSE);

	if (!emac_alloc_desc_ring(sc, &sc->rx_ring.desc, sizeof(struct emac_desc), sizeof(struct emac_desc) * RX_DESC_NUM))
		return (B_FALSE);

	if (!emac_alloc_desc_ring(sc, &sc->tx_ring.desc, sizeof(struct emac_desc), sizeof(struct emac_desc) * TX_DESC_NUM))
		return (B_FALSE);

	emac_gmac_reset(sc);

	return (B_TRUE);
}

static void
emac_mii_write(void *arg, uint8_t phy, uint8_t reg, uint16_t value)
{
	struct emac_sc *sc = arg;

	emac_mutex_enter(sc);

	union emac_gmii_address gmii_address;
	gmii_address.dw = emac_reg_read(sc, EMAC_GMII_ADDRESS);
	if (gmii_address.gb == 0) {
		union emac_gmii_data gmii_data = {0};
		gmii_data.gd = value;
		emac_reg_write(sc, EMAC_GMII_DATA, gmii_data.dw);
		gmii_address.dw = 0;
		gmii_address.gb = 1;
		gmii_address.gw = 1;
		gmii_address.cr = 1;
		gmii_address.gr = reg;
		gmii_address.pa = phy;
		emac_reg_write(sc, EMAC_GMII_ADDRESS, gmii_address.dw);

		for (int i = 0; i < 1000; i++) {
			emac_usecwait(100);
			gmii_address.dw = emac_reg_read(sc, EMAC_GMII_ADDRESS);
			if (gmii_address.gb == 0) {
				break;
			}
		}

		if (gmii_address.gb != 0) {
			cmn_err(CE_WARN, "%s%d: MII write failed",
			    ddi_driver_name(sc->dip), ddi_get_instance(sc->dip));
		}
	} else {
		cmn_err(CE_WARN, "%s%d: MII busy",
		    ddi_driver_name(sc->dip), ddi_get_instance(sc->dip));
	}

	emac_mutex_exit(sc);
}

static uint16_t
emac_mii_read(void *arg, uint8_t phy, uint8_t reg)
{
	struct emac_sc *sc = arg;

	uint16_t data = 0xffff;

	emac_mutex_enter(sc);

	union emac_gmii_address gmii_address;
	gmii_address.dw = emac_reg_read(sc, EMAC_GMII_ADDRESS);
	if (gmii_address.gb == 0) {
		gmii_address.dw = 0;
		gmii_address.gb = 1;
		gmii_address.gw = 0;
		gmii_address.cr = 1;
		gmii_address.gr = reg;
		gmii_address.pa = phy;
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
		} else {
			cmn_err(CE_WARN, "%s%d: MII read failed",
			    ddi_driver_name(sc->dip), ddi_get_instance(sc->dip));
		}
	} else {
		cmn_err(CE_WARN, "%s%d: MII busy",
		    ddi_driver_name(sc->dip), ddi_get_instance(sc->dip));
	}

	emac_mutex_exit(sc);

	return data;
}

static int
emac_probe(dev_info_t *dip)
{
	int len;
	char buf[80];
	pnode_t node = ddi_get_nodeid(dip);
	if (node < 0)
		return (DDI_PROBE_FAILURE);

	len = prom_getproplen(node, "status");
	if (len <= 0 || len >= sizeof(buf))
		return (DDI_PROBE_FAILURE);

	prom_getprop(node, "status", (caddr_t)buf);
	if (strcmp(buf, "ok") != 0 && (strcmp(buf, "okay") != 0))
		return (DDI_PROBE_FAILURE);

	return (DDI_PROBE_SUCCESS);
}

static void
emac_mii_notify(void *arg, link_state_t link)
{
	struct emac_sc *sc = arg;
	uint32_t gmac;
	uint32_t gpcr;
	link_flowctrl_t fc __unused; /* XXXARM */
	link_duplex_t duplex;
	int speed;

	fc = mii_get_flowctrl(sc->mii_handle);
	duplex = mii_get_duplex(sc->mii_handle);
	speed = mii_get_speed(sc->mii_handle);

	emac_mutex_enter(sc);

	if (link == LINK_STATE_UP) {
		sc->phy_speed = speed;
		sc->phy_duplex = duplex;
		emac_gmac_update(sc);
	} else {
		sc->phy_speed = -1;
		sc->phy_duplex = LINK_DUPLEX_UNKNOWN;
	}

	emac_mutex_exit(sc);

	mac_link_update(sc->mac_handle, link);
}

static void
emac_mii_reset(void *arg)
{
	struct emac_sc *sc = arg;
	int phy = mii_get_addr(sc->mii_handle);

	emac_mii_write(sc, phy, 0x0d, 0x7);
	emac_mii_write(sc, phy, 0x0e, 0x3c);
	emac_mii_write(sc, phy, 0x0d, 0x4007);
	emac_mii_write(sc, phy, 0x0e, 0);

	uint16_t v = emac_mii_read(sc, phy, 9);
	emac_mii_write(sc, phy, 9, v & ~(1u << 9));
}

static mii_ops_t emac_mii_ops = {
	MII_OPS_VERSION,
	emac_mii_read,
	emac_mii_write,
	emac_mii_notify,
	emac_mii_reset	/* reset */
};

static int
emac_phy_install(struct emac_sc *sc)
{
	sc->mii_handle = mii_alloc(sc, sc->dip, &emac_mii_ops);
	if (sc->mii_handle == NULL) {
		return (DDI_FAILURE);
	}
	//mii_set_pauseable(sc->mii_handle, B_FALSE, B_FALSE);

	return DDI_SUCCESS;
}

static mblk_t *
emac_send(struct emac_sc *sc, mblk_t *mp)
{
	if (((sc->tx_ring.head - sc->tx_ring.tail + TX_DESC_NUM) % TX_DESC_NUM) == (TX_DESC_NUM - 8)) {
		return mp;
	}

	int index = sc->tx_ring.head;
	size_t mblen = 0;
	sc->tx_ring.mp[index] = mp;
	struct emac_packet *pkt = sc->tx_ring.pkt[index];

	caddr_t addr = pkt->dma.addr;
	for (mblk_t *bp = mp; bp != NULL; bp = bp->b_cont) {
		size_t frag_len = MBLKL(bp);
		if (frag_len == 0)
			continue;
		memcpy(addr, bp->b_rptr, frag_len);
		addr += frag_len;
		mblen += frag_len;
	}

	ddi_dma_sync(pkt->dma.dma_handle, 0, mblen, DDI_DMA_SYNC_FORDEV);

	volatile struct emac_desc *desc_p = (volatile struct emac_desc *)(sc->tx_ring.desc.addr + sizeof(struct emac_desc) * index);
	desc_p->cntl = EMAC_TXDESC_CNTL_IC | EMAC_TXDESC_CNTL_LS | EMAC_TXDESC_CNTL_FS | EMAC_DESC_CNTL_CHAIN | ((mblen < ETHERMIN) ? ETHERMIN: mblen);
	dmb(sy);
	desc_p->status = EMAC_DESC_STATUS_OWN;
	sc->tx_ring.head = (index + 1) % TX_DESC_NUM;

	return (NULL);
}

static mblk_t *
emac_m_tx(void *arg, mblk_t *mp)
{
	struct emac_sc *sc = arg;
	mblk_t *nmp;

	emac_mutex_enter(sc);

	int count = 0;
	while (mp != NULL) {
		nmp = mp->b_next;
		mp->b_next = NULL;
		if ((mp = emac_send(sc, mp)) != NULL) {
			mp->b_next = nmp;
			break;
		}
		mp = nmp;
		count++;
	}

	if (count != 0) {
		dmb(sy);
		emac_reg_write(sc, EMAC_DMA_TX_POLL_DEMAND, 0xffffffff);
	}

	emac_mutex_exit(sc);

	return (mp);
}


static mblk_t *
emac_rx_intr(struct emac_sc *sc)
{
	int index = sc->rx_ring.index;

	mblk_t *mblk_head = NULL;
	mblk_t **mblk_tail = &mblk_head;

	for (;;) {
		size_t len = 0;
		volatile struct emac_desc *desc_p = (struct emac_desc *)(sc->rx_ring.desc.addr + sizeof(struct emac_desc) * index);
		uint32_t status = desc_p->status;
		dmb(sy);
		if (status & EMAC_DESC_STATUS_OWN)
			break;

		if ((status & EMAC_RXDESC_STATUS_ES) == 0 &&
		    (status & (EMAC_RXDESC_STATUS_FS | EMAC_RXDESC_STATUS_LD)) == (EMAC_RXDESC_STATUS_FS | EMAC_RXDESC_STATUS_LD)) {
			len = EMAC_RXDESC_STATUS_FL(status);
			if (len >= 4)
				len -= 4;
			else
				len = 0;
		}

		if (len > 0) {
			struct emac_packet *pkt = emac_alloc_packet(sc);
			if (pkt) {
				mblk_t *mp = sc->rx_ring.pkt[index]->mp;
				*mblk_tail = mp;
				mblk_tail = &mp->b_next;
				ddi_dma_sync(sc->rx_ring.pkt[index]->dma.dma_handle, 0, len, DDI_DMA_SYNC_FORKERNEL);
				mp->b_wptr += len;
				sc->rx_ring.pkt[index] = pkt;
			}
		}

		{
			struct emac_packet *pkt = sc->rx_ring.pkt[index];
			desc_p->cntl = EMAC_DESC_CNTL_CHAIN | pkt->dma.size;
			desc_p->addr = pkt->dma.dmac_addr;
			dmb(sy);
			desc_p->status = EMAC_DESC_STATUS_OWN;
		}
		index = ((index + 1) % RX_DESC_NUM);
	}

	sc->rx_ring.index = index;

	return mblk_head;
}


static int
emac_tx_intr(struct emac_sc *sc)
{
	int index = sc->tx_ring.tail;
	int ret = 0;
	while (index != sc->tx_ring.head) {
		volatile struct emac_desc *desc_p = (volatile struct emac_desc *)(sc->tx_ring.desc.addr + sizeof(struct emac_desc) * index);
		uint32_t status = desc_p->status;
		dmb(sy);
		if (status & EMAC_DESC_STATUS_OWN)
			break;
		emac_free_tx(sc, index);
		index = (index + 1) % TX_DESC_NUM;
		ret++;
	}
	sc->tx_ring.tail = index;
	return ret;
}

static uint_t
emac_intr(caddr_t arg, caddr_t unused)
{
	struct emac_sc *sc = (struct emac_sc *)arg;

	emac_mutex_enter(sc);

	for (;;) {
		union emac_dma_status status;
		status.dw = emac_reg_read(sc, EMAC_DMA_STATUS);
		if (status.ti == 0 && status.ri == 0 && status.nis == 0 &&
		    status.ais == 0 && status.fbi == 0 && status.unf == 0)
			break;
		emac_reg_write(sc, EMAC_DMA_STATUS, status.dw);

		if (sc->running == 0)
			break;

		if (status.ri) {
			mblk_t *mp = emac_rx_intr(sc);
			if (mp) {
				emac_mutex_exit(sc);
				mac_rx(sc->mac_handle, NULL, mp);
				emac_mutex_enter(sc);
			}
		}

		if (sc->running == 0)
			break;

		if (status.ti) {
			int tx = 0;

			tx = emac_tx_intr(sc);

			if (tx) {
				emac_mutex_exit(sc);
				mac_tx_update(sc->mac_handle);
				emac_mutex_enter(sc);
			}
		}
	}

	emac_mutex_exit(sc);

	return (DDI_INTR_CLAIMED);
}


static int emac_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int
emac_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;
	default:
		return (DDI_FAILURE);
	}
	struct emac_sc *sc = ddi_get_driver_private(dip);

	emac_m_stop(sc);

	if (mac_disable(sc->mac_handle) != 0)
		return (DDI_FAILURE);

	emac_destroy(sc);

	return DDI_SUCCESS;
}

static int
emac_quiesce(dev_info_t *dip)
{
	cmn_err(CE_WARN, "%s%d: emac_quiesce is not implemented",
	    ddi_driver_name(dip), ddi_get_instance(dip));
	return DDI_FAILURE;
}

static uint32_t
bitreverse(uint32_t x)
{
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));

	return (x >> 16) | (x << 16);
}

static void
emac_update_filter(struct emac_sc *sc)
{
	uint32_t hash[2] = {0};
	union emac_mac_frame_filter frame_filter;
	frame_filter.dw = emac_reg_read(sc, EMAC_MAC_FRAME_FILTER);
	if (frame_filter.pr) {
		hash[0] = 0xffffffff;
		hash[1] = 0xffffffff;
	} else {
		for (struct emac_mcast *mc = list_head(&sc->mcast); mc; mc = list_next(&sc->mcast, mc)) {
			uint32_t crc;
			CRC32(crc, mc->addr, sizeof(mc->addr), -1U, crc32_table);
			uint32_t val = (bitreverse(~crc) >> 26);
			hash[(val >> 5)] |= (1 << (val & 31));
		}
	}
	emac_reg_write(sc, EMAC_MAC_HASHTABLE_HIGH, hash[1]);
	emac_reg_write(sc, EMAC_MAC_HASHTABLE_LOW,  hash[0]);
}

static int
emac_m_setpromisc(void *a, boolean_t b)
{
	struct emac_sc *sc = a;
	emac_mutex_enter(sc);

	union emac_mac_frame_filter frame_filter;
	frame_filter.dw = emac_reg_read(sc, EMAC_MAC_FRAME_FILTER);
	frame_filter.pr = (b? 1: 0);
	emac_reg_write(sc, EMAC_MAC_FRAME_FILTER, frame_filter.dw);
	emac_update_filter(sc);

	emac_mutex_exit(sc);

	return 0;
}

static int
emac_m_multicst(void *a, boolean_t b, const uint8_t *c)
{
	struct emac_sc *sc = a;
	struct emac_mcast *mc;

	emac_mutex_enter(sc);

	if (b) {
		mc = kmem_alloc(sizeof (*mc), KM_NOSLEEP);
		if (!mc) {
			emac_mutex_exit(sc);
			return ENOMEM;
		}

		memcpy(mc->addr, c, sizeof(mc->addr));
		list_insert_head(&sc->mcast, mc);
	} else {
		for (mc = list_head(&sc->mcast); mc; mc = list_next(&sc->mcast, mc)) {
			if (memcmp(mc->addr, c, sizeof(mc->addr)) == 0) {
				list_remove(&sc->mcast, mc);
				kmem_free(mc, sizeof (*mc));
				break;
			}
		}
	}

	emac_update_filter(sc);

	emac_mutex_exit(sc);
	return 0;
}

static int
emac_m_unicst(void *arg, const uint8_t *dev_addr)
{
	struct emac_sc *sc = arg;

	emac_mutex_enter(sc);

	memcpy(sc->dev_addr, dev_addr, sizeof(sc->dev_addr));

	emac_gmac_disable(sc);

	union {
		uint32_t dw[2];
		uint8_t dev_addr[6];
	} mac_addr = {0};
	memcpy(mac_addr.dev_addr, sc->dev_addr, sizeof(sc->dev_addr));

	emac_reg_write(sc, EMAC_MAC_ADDRESS_HIGH(0), mac_addr.dw[1]);
	emac_reg_write(sc, EMAC_MAC_ADDRESS_LOW(0),  mac_addr.dw[0]);

	emac_gmac_enable(sc);

	emac_mutex_exit(sc);

	return 0;
}

static int
emac_m_start(void *arg)
{
	struct emac_sc *sc = arg;

	emac_mutex_enter(sc);

	if (!emac_alloc_buffer(sc)) {
		emac_mutex_exit(sc);
		return ENOMEM;
	}
	emac_gmac_init(sc);
	emac_gmac_enable(sc);

	sc->running = 1;

	if (ddi_intr_enable(sc->intr_handle) != DDI_SUCCESS) {
		sc->running = 0;
		emac_gmac_disable(sc);
		emac_free_buffer(sc);
		emac_mutex_exit(sc);
		return EIO;
	}

	emac_mutex_exit(sc);

	mii_start(sc->mii_handle);

	return 0;
}

static void
emac_m_stop(void *arg)
{
	struct emac_sc *sc = arg;

	mii_stop(sc->mii_handle);

	emac_mutex_enter(sc);

	ddi_intr_disable(sc->intr_handle);

	sc->running = 0;
	emac_gmac_disable(sc);
	emac_free_buffer(sc);

	emac_mutex_exit(sc);
}

static int
emac_m_getstat(void *arg, uint_t stat, uint64_t *val)
{
	struct emac_sc *sc = arg;
	return mii_m_getstat(sc->mii_handle, stat, val);
}

static int
emac_m_setprop(void *arg, const char *name, mac_prop_id_t num, uint_t sz, const void *val)
{
	struct emac_sc *sc = arg;
	return mii_m_setprop(sc->mii_handle, name, num, sz, val);
}

static int
emac_m_getprop(void *arg, const char *name, mac_prop_id_t num, uint_t sz, void *val)
{
	struct emac_sc *sc = arg;
	return mii_m_getprop(sc->mii_handle, name, num, sz, val);
}

static void
emac_m_propinfo(void *arg, const char *name, mac_prop_id_t num, mac_prop_info_handle_t prh)
{
	struct emac_sc *sc = arg;
	mii_m_propinfo(sc->mii_handle, name, num, prh);
}

static void
emac_m_ioctl(void *arg, queue_t *wq, mblk_t *mp)
{
	struct emac_sc *sc = arg;
	if (mii_m_loop_ioctl(sc->mii_handle, wq, mp))
		return;

	miocnak(wq, mp, 0, EINVAL);
}

extern struct mod_ops mod_driverops;

DDI_DEFINE_STREAM_OPS(emac_devops, nulldev, emac_probe, emac_attach,
    emac_detach, nodev, NULL, D_MP, NULL, emac_quiesce);

static struct modldrv emac_modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"Amlogic gmac",		/* short description */
	&emac_devops		/* driver specific ops */
};

static struct modlinkage emac_modlinkage = {
	MODREV_1,		/* ml_rev */
	{ &emac_modldrv, NULL }	/* ml_linkage */
};

static mac_callbacks_t emac_m_callbacks = {
	MC_SETPROP | MC_GETPROP | MC_PROPINFO,	/* mc_callbacks */
	emac_m_getstat,	/* mc_getstat */
	emac_m_start,		/* mc_start */
	emac_m_stop,		/* mc_stop */
	emac_m_setpromisc,	/* mc_setpromisc */
	emac_m_multicst,	/* mc_multicst */
	emac_m_unicst,		/* mc_unicst */
	emac_m_tx,		/* mc_tx */
	NULL,
	emac_m_ioctl,		/* mc_ioctl */
	NULL,			/* mc_getcapab */
	NULL,			/* mc_open */
	NULL,			/* mc_close */
	emac_m_setprop,
	emac_m_getprop,
	emac_m_propinfo
};

static int
emac_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	default:
		return (DDI_FAILURE);
	}

	struct emac_sc *sc = kmem_zalloc(sizeof(struct emac_sc), KM_SLEEP);
	ddi_set_driver_private(dip, sc);
	sc->dip = dip;

	mutex_init(&sc->intrlock, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&sc->rx_pkt_lock, NULL, MUTEX_DRIVER, NULL);
	list_create(&sc->mcast, sizeof (struct emac_mcast), offsetof(struct emac_mcast, node));

	if (ddi_regs_map_setup(sc->dip, 0, &sc->reg.addr, 0, 0, &reg_acc_attr, &sc->reg.handle) != DDI_SUCCESS) {
		goto err_exit;
	}

	emac_mutex_enter(sc);
	if (!emac_init(sc)) {
		emac_mutex_exit(sc);
		goto err_exit;
	}
	emac_mutex_exit(sc);

	mac_register_t *macp;
	if ((macp = mac_alloc(MAC_VERSION)) == NULL) {
		goto err_exit;
	}
	sc->macp = macp;

	macp->m_type_ident = MAC_PLUGIN_IDENT_ETHER;
	macp->m_driver = sc;
	macp->m_dip = dip;
	macp->m_src_addr = sc->dev_addr;
	macp->m_callbacks = &emac_m_callbacks;
	macp->m_min_sdu = 0;
	macp->m_max_sdu = ETHERMTU;
	macp->m_margin = VLAN_TAGSZ;

	if (mac_register(macp, &sc->mac_handle) != 0) {
		mac_free(sc->macp);
		sc->mac_handle = 0;
		goto err_exit;
	}

	if (emac_phy_install(sc) != DDI_SUCCESS) {
		goto err_exit;
	}

	int actual;
	if (ddi_intr_alloc(dip, &sc->intr_handle, DDI_INTR_TYPE_FIXED, 0, 1, &actual, DDI_INTR_ALLOC_STRICT) != DDI_SUCCESS) {
		goto err_exit;
	}

	if (ddi_intr_add_handler(sc->intr_handle, emac_intr, sc, NULL) != DDI_SUCCESS) {
		ddi_intr_free(sc->intr_handle);
		sc->intr_handle = 0;
		goto err_exit;
	}

	return DDI_SUCCESS;
err_exit:
	emac_destroy(sc);
	return (DDI_FAILURE);
}

int
_init(void)
{
	int i;

	mac_init_ops(&emac_devops, "dwmac");

	if ((i = mod_install(&emac_modlinkage)) != 0) {
		mac_fini_ops(&emac_devops);
	}
	return (i);
}

int
_fini(void)
{
	int i;

	if ((i = mod_remove(&emac_modlinkage)) == 0) {
		mac_fini_ops(&emac_devops);
	}
	return (i);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&emac_modlinkage, modinfop));
}
