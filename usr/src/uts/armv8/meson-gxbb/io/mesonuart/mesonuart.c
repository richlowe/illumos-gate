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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved					*/

/*
 * Copyright (c) 1992, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2012 Milan Jurik. All rights reserved.
 */
/*
 * Copyright 2017 Hayashi Naoyuki
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/stream.h>
#include <sys/termio.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/strtty.h>
#include <sys/debug.h>
#include <sys/kbio.h>
#include <sys/cred.h>
#include <sys/stat.h>
#include <sys/consdev.h>
#include <sys/mkdev.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/strsun.h>
#include <sys/promif.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/pci.h>
#include <sys/policy.h>
#include <sys/gic.h>

#include "mesonuart.h"

#define MESON_UART_WFIFO		0x00
#define MESON_UART_RFIFO		0x04
#define MESON_UART_CONTROL		0x08
#define MESON_UART_STATUS		0x0c
#define MESON_UART_MISC			0x10
#define MESON_UART_REG5			0x14

#define REG_READ(mesonuart, reg)		ddi_get32((mesonuart)->mesonuart_iohandle, (uint32_t *)((mesonuart)->mesonuart_ioaddr + (reg)))
#define REG_WRITE(mesonuart, reg, val)	ddi_put32((mesonuart)->mesonuart_iohandle, (uint32_t *)((mesonuart)->mesonuart_ioaddr + (reg)), (val))

union uart_control {
	uint32_t dw;
	struct {
		uint32_t baud_rate		: 12;
		uint32_t transmit_enable	: 1;
		uint32_t receive_enable		: 1;
		uint32_t 			: 1;
		uint32_t two_wire_mode		: 1;
		uint32_t stop_bit_length	: 2;
		uint32_t parity_type		: 1;
		uint32_t parity_enable		: 1;
		uint32_t character_length	: 2;
		uint32_t reset_transmit		: 1;
		uint32_t reset_receive		: 1;
		uint32_t clear_error		: 1;
		uint32_t invert_rx		: 1;
		uint32_t invert_tx		: 1;
		uint32_t receive_interrupt	: 1;
		uint32_t transmit_interrupt	: 1;
		uint32_t invert_cts		: 1;
		uint32_t mask_error		: 1;
		uint32_t invert_rts		: 1;
	};
};

union uart_status {
	uint32_t dw;
	struct {
		uint32_t rx_fifo_count		: 7;
		uint32_t 			: 1;
		uint32_t tx_fifo_count		: 7;
		uint32_t 			: 1;
		uint32_t parity_error		: 1;
		uint32_t frame_error		: 1;
		uint32_t tx_fifo_werr		: 1;
		uint32_t rx_fifo_full		: 1;
		uint32_t rx_fifo_empty		: 1;
		uint32_t tx_fifo_full		: 1;
		uint32_t tx_fifo_empty		: 1;
		uint32_t cts_level		: 1;
		uint32_t rx_fifo_overflow	: 1;
		uint32_t tx_busy		: 1;
		uint32_t rx_busy		: 1;
		uint32_t			: 5;
	};
};

union uart_misc {
	uint32_t dw;
	struct {
		uint32_t rx_irq_count		: 8;
		uint32_t tx_irq_count		: 8;
		uint32_t rx_filter_sel		: 3;
		uint32_t rx_filter_timebase	: 1;
		uint32_t baud_rate_ext		: 4;
		uint32_t cts_filter_sel		: 3;
		uint32_t cts_filter_timebase	: 1;
		uint32_t msasync_fifo_en		: 1;
		uint32_t msasync_fifo_purge	: 1;
		uint32_t old_rx_baud		: 1;
		uint32_t always_enable		: 1;
	};
};

union uart_reg5 {
	uint32_t dw;
	struct {
		uint32_t baud_rate		: 23;
		uint32_t use_new_baud_rate	: 1;
		uint32_t use_xtal_clk		: 1;
		uint32_t 			: 7;
	};
};

#define	MESONUART_REGISTER_FILE_NO 0
#define	MESONUART_REGOFFSET 0
#define	MESONUART_REGISTER_LEN 0
#define	MESONUART_DEFAULT_BAUD	B115200

/*
 * set the RX FIFO trigger_level to half the RX FIFO size for now
 * we may want to make this configurable later.
 */
static	int mesonuart_trig_level = FIFO_TRIG_8;

static int mesonuart_drain_check = 15000000;		/* tunable: exit drain check time */
static int mesonuart_min_dtr_low = 500000;		/* tunable: minimum DTR down time */
static int mesonuart_min_utbrk = 100000;		/* tunable: minumum untimed brk time */

/*
 * Just in case someone has a chip with broken loopback mode, we provide a
 * means to disable the loopback test. By default, we only loopback test
 * UARTs which look like they have FIFOs bigger than 16 bytes.
 * Set to 0 to suppress test, or to 2 to enable test on any size FIFO.
 */
static int mesonuart_fifo_test = 1;		/* tunable: set to 0, 1, or 2 */

/*
 * Allow ability to switch off testing of the scratch register.
 * Some UART emulators might not have it. This will also disable the test
 * for Exar/Startech ST16C650, as that requires use of the SCR register.
 */
static int mesonuart_scr_test = 1;		/* tunable: set to 0 to disable SCR reg test */

/*
 * As we don't yet support on-chip flow control, it's a bad idea to put a
 * large number of characters in the TX FIFO, since if other end tells us
 * to stop transmitting, we can only stop filling the TX FIFO, but it will
 * still carry on draining by itself, so remote end still gets what's left
 * in the FIFO.
 */
static int mesonuart_max_tx_fifo = 16;	/* tunable: max fill of TX FIFO */

#define	msasync_stopc	msasync_ttycommon.t_stopc
#define	msasync_startc	msasync_ttycommon.t_startc

#define	MESONUART_INIT	1
#define	MESONUART_NOINIT	0

/* enum value for sw and hw flow control action */
typedef enum {
	FLOW_CHECK,
	FLOW_STOP,
	FLOW_START
} msasync_flowc_action;

#ifdef DEBUG
#define	MESONUART_DEBUG_INIT	0x0001	/* Output msgs during driver initialization. */
#define	MESONUART_DEBUG_INPUT	0x0002	/* Report characters received during int. */
#define	MESONUART_DEBUG_EOT	0x0004	/* Output msgs when wait for xmit to finish. */
#define	MESONUART_DEBUG_CLOSE	0x0008	/* Output msgs when driver open/close called */
#define	MESONUART_DEBUG_HFLOW	0x0010	/* Output msgs when H/W flowcontrol is active */
#define	MESONUART_DEBUG_PROCS	0x0020	/* Output each proc name as it is entered. */
#define	MESONUART_DEBUG_STATE	0x0040	/* Output value of Interrupt Service Reg. */
#define	MESONUART_DEBUG_INTR	0x0080	/* Output value of Interrupt Service Reg. */
#define	MESONUART_DEBUG_OUT	0x0100	/* Output msgs about output events. */
#define	MESONUART_DEBUG_BUSY	0x0200	/* Output msgs when xmit is enabled/disabled */
#define	MESONUART_DEBUG_MODEM	0x0400	/* Output msgs about modem status & control. */
#define	MESONUART_DEBUG_MODM2	0x0800	/* Output msgs about modem status & control. */
#define	MESONUART_DEBUG_IOCTL	0x1000	/* Output msgs about ioctl messages. */
#define	MESONUART_DEBUG_CHIP	0x2000	/* Output msgs about chip identification. */
#define	MESONUART_DEBUG_SFLOW	0x4000	/* Output msgs when S/W flowcontrol is active */
#define	MESONUART_DEBUG(x) (debug & (x))
static	int debug  = 0;
#else
#define	MESONUART_DEBUG(x) B_FALSE
#endif

/* pnpISA compressed device ids */
#define	pnpMTS0219 0xb6930219	/* Multitech MT5634ZTX modem */

/*
 * PPS (Pulse Per Second) support.
 */
void ddi_hardpps(struct timeval *, int);
/*
 * This is protected by the mesonuart_excl_hi of the port on which PPS event
 * handling is enabled.  Note that only one port should have this enabled at
 * any one time.  Enabling PPS handling on multiple ports will result in
 * unpredictable (but benign) results.
 */
static struct ppsclockev mesonuart_ppsev;

#ifdef PPSCLOCKLED
/* XXX Use these to observe PPS latencies and jitter on a scope */
#define	LED_ON
#define	LED_OFF
#else
#define	LED_ON
#define	LED_OFF
#endif

static	int max_mesonuart_instance = -1;

static	uint_t	mesonuartsoftintr(caddr_t intarg);
static	uint_t	mesonuartintr(caddr_t argmesonuart);

static boolean_t abort_charseq_recognize(uchar_t ch);

/* The msasync interrupt entry points */
static void	msasync_txint(struct mesonuartcom *mesonuart);
static void	msasync_rxint(struct mesonuartcom *mesonuart);
static void	msasync_msint(struct mesonuartcom *mesonuart);
static void	msasync_softint(struct mesonuartcom *mesonuart);

static void	msasync_ioctl(struct msasyncline *msasync, queue_t *q, mblk_t *mp);
static void	msasync_reioctl(void *unit);
static void	msasync_iocdata(queue_t *q, mblk_t *mp);
static void	msasync_restart(void *arg);
static void	msasync_start(struct msasyncline *msasync);
static void	msasync_nstart(struct msasyncline *msasync, int mode);
static void	msasync_resume(struct msasyncline *msasync);
static void	mesonuart_program(struct mesonuartcom *mesonuart, int mode);
static void	mesonuartinit(struct mesonuartcom *mesonuart);
static void	mesonuart_waiteot(struct mesonuartcom *mesonuart);
static void	mesonuartputchar(cons_polledio_arg_t, uchar_t c);
static int	mesonuartgetchar(cons_polledio_arg_t);
static boolean_t	mesonuartischar(cons_polledio_arg_t);

static int	mesonuarttodm(int, int);
static int	dmtomesonuart(int);
/*PRINTFLIKE2*/
static void	mesonuarterror(int level, const char *fmt, ...) __KPRINTFLIKE(2);
static void	mesonuart_parse_mode(dev_info_t *devi, struct mesonuartcom *mesonuart);
static void	mesonuart_soft_state_free(struct mesonuartcom *);
static char	*mesonuart_name(struct mesonuartcom *mesonuart);
static void	msasync_hold_utbrk(void *arg);
static void	msasync_resume_utbrk(struct msasyncline *msasync);
static void	msasync_dtr_free(struct msasyncline *msasync);
static int	mesonuart_getproperty(dev_info_t *devi, struct mesonuartcom *mesonuart,
		    const char *property);
static boolean_t	msasync_flowcontrol_sw_input(struct mesonuartcom *mesonuart,
			    msasync_flowc_action onoff, int type);
static void	msasync_flowcontrol_sw_output(struct mesonuartcom *mesonuart,
		    msasync_flowc_action onoff);
static void	msasync_flowcontrol_hw_input(struct mesonuartcom *mesonuart,
		    msasync_flowc_action onoff, int type);
static void	msasync_flowcontrol_hw_output(struct mesonuartcom *mesonuart,
		    msasync_flowc_action onoff);


#define	GET_PROP(devi, pname, pflag, pval, plen) \
		(ddi_prop_op(DDI_DEV_T_ANY, (devi), PROP_LEN_AND_VAL_BUF, \
		(pflag), (pname), (caddr_t)(pval), (plen)))

static kmutex_t mesonuart_glob_lock; /* lock protecting global data manipulation */
static void *mesonuart_soft_state;

#ifdef	DEBUG
/*
 * Set this to true to make the driver pretend to do a suspend.  Useful
 * for debugging suspend/resume code with a serial debugger.
 */
static boolean_t	mesonuart_nosuspend = B_FALSE;
#endif


static int baudtable[] = {
	0,	/* 0 baud rate */
	50,	/* 50 baud rate */
	75,	/* 75 baud rate */
	110,	/* 110 baud rate */
	134,	/* 134 baud rate */
	150,	/* 150 baud rate */
	200,	/* 200 baud rate */
	300,	/* 300 baud rate */
	600,	/* 600 baud rate */
	1200,	/* 1200 baud rate */
	1800,	/* 1800 baud rate */
	2400,	/* 2400 baud rate */
	4800,	/* 4800 baud rate */
	9600,	/* 9600 baud rate */
	19200,	/* 19200 baud rate */
	38400,	/* 38400 baud rate */
	57600,	/* 57600 baud rate */
	76800,	/* 76800 baud rate */
	115200,	/* 115200 baud rate */
	153600,	/* 153600 baud rate */
	230400,	/* 230400 baud rate */
	307200,	/* 307200 baud rate */
	460800	/* 460800 baud rate */
};

#define	N_SU_SPEEDS	(sizeof (baudtable)/sizeof (baudtable[0]))

static void
meson_reset_fifo(struct mesonuartcom *mesonuart, uint8_t flush)
{
	union uart_control reg;
	reg.dw = REG_READ(mesonuart, MESON_UART_CONTROL);

	if (flush & FIFORXFLSH)
		reg.reset_receive = 1;
	if (flush & FIFOTXFLSH)
		reg.reset_transmit = 1;

	REG_WRITE(mesonuart, MESON_UART_CONTROL, reg.dw);
	reg.reset_receive = 0;
	reg.reset_transmit = 0;
	REG_READ(mesonuart, MESON_UART_CONTROL);
	REG_WRITE(mesonuart, MESON_UART_CONTROL, reg.dw);
}

static void
meson_put_char(struct mesonuartcom *mesonuart, uint8_t val)
{
	REG_WRITE(mesonuart, MESON_UART_WFIFO, val);
}

static bool
meson_is_busy(struct mesonuartcom *mesonuart)
{
	union uart_status reg;
	reg.dw = REG_READ(mesonuart, MESON_UART_STATUS);
	return !reg.tx_fifo_empty || reg.tx_busy;
}

static bool
meson_tx_is_ready(struct mesonuartcom *mesonuart)
{
	union uart_status reg;
	reg.dw = REG_READ(mesonuart, MESON_UART_STATUS);
	return reg.tx_fifo_full == 0;
}

static bool
meson_rx_is_ready(struct mesonuartcom *mesonuart)
{
	union uart_status reg;
	reg.dw = REG_READ(mesonuart, MESON_UART_STATUS);
	return reg.rx_fifo_empty != 0;
}

static uint8_t
meson_get_msr(struct mesonuartcom *mesonuart)
{
	return CTS | DCD;
}

static void
meson_set_icr(struct mesonuartcom *mesonuart, uint8_t icr, uint8_t mask)
{
	union uart_control reg;
	reg.dw = REG_READ(mesonuart, MESON_UART_CONTROL);
	if (mask & RIEN) {
		reg.receive_interrupt = ((icr & RIEN)? 1: 0);
	}
	if (mask & TIEN) {
		reg.transmit_interrupt = ((icr & TIEN)? 1: 0);
	}
	REG_WRITE(mesonuart, MESON_UART_CONTROL, reg.dw);
}

static uint8_t
meson_get_char(struct mesonuartcom *mesonuart)
{
	return REG_READ(mesonuart, MESON_UART_RFIFO);
}

static void
meson_set_control(struct mesonuartcom *mesonuart)
{
	uint8_t lcr = mesonuart->mesonuart_lcr;
	union uart_control reg;
	reg.dw = REG_READ(mesonuart, MESON_UART_CONTROL);

	if (lcr & PEN) {
		reg.parity_enable = 1;
		reg.parity_type = ((lcr & EPS)? 0: 1);
	} else {
		reg.parity_enable = 0;
		reg.parity_type = 0;
	}

	switch (lcr & (WLS0 | WLS1)) {
	case BITS5: reg.character_length = 3; break;
	case BITS6: reg.character_length = 2; break;
	case BITS7: reg.character_length = 1; break;
	case BITS8: reg.character_length = 0; break;
	}
	reg.stop_bit_length = ((lcr & STB)? 1: 0);
	REG_WRITE(mesonuart, MESON_UART_CONTROL, reg.dw);
}

static void
meson_reset(struct mesonuartcom *mesonuart)
{
	union uart_control reg;
	reg.dw = 0;

	reg.clear_error = 1;
	reg.reset_transmit = 1;
	reg.reset_receive = 1;
	REG_WRITE(mesonuart, MESON_UART_CONTROL, reg.dw);

	reg.clear_error = 0;
	reg.reset_transmit = 0;
	reg.reset_receive = 0;
	REG_READ(mesonuart, MESON_UART_CONTROL);
	REG_WRITE(mesonuart, MESON_UART_CONTROL, reg.dw);

	reg.transmit_enable = 1;
	reg.receive_enable = 1;
	reg.two_wire_mode = 1;
	REG_WRITE(mesonuart, MESON_UART_CONTROL, reg.dw);
}

static void
meson_set_baud(struct mesonuartcom *mesonuart, uint8_t bidx)
{
	ASSERT(bidx < N_SU_SPEEDS);
	int baudrate;
	if (bidx == 0)
		baudrate = 115200;
	else
		baudrate = baudtable[bidx];
	union uart_reg5 reg;
	reg.dw = REG_READ(mesonuart, MESON_UART_REG5);

	if (mesonuart->mesonuart_clock == 24000000) {
		reg.baud_rate = ((mesonuart->mesonuart_clock / 3) / baudrate) - 1;
		reg.use_xtal_clk = 1;
	} else {
		reg.baud_rate = ((mesonuart->mesonuart_clock * 10 / (baudrate * 4) + 5)  / 10) - 1;
	}
	reg.use_new_baud_rate = 1;
	REG_WRITE(mesonuart, MESON_UART_REG5, reg.dw);
}

static int mesonuartrsrv(queue_t *q);
static int mesonuartopen(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr);
static int mesonuartclose(queue_t *q, int flag, cred_t *credp);
static int mesonuartwputdo(queue_t *q, mblk_t *mp, boolean_t);
static int mesonuartwput(queue_t *q, mblk_t *mp);

struct module_info mesonuart_info = {
	0,
	"mesonuart",
	0,
	INFPSZ,
	4096,
	128
};

static struct qinit mesonuart_rint = {
	putq,
	mesonuartrsrv,
	mesonuartopen,
	mesonuartclose,
	NULL,
	&mesonuart_info,
	NULL
};

static struct qinit mesonuart_wint = {
	mesonuartwput,
	NULL,
	NULL,
	NULL,
	NULL,
	&mesonuart_info,
	NULL
};

struct streamtab mesonuart_str_info = {
	&mesonuart_rint,
	&mesonuart_wint,
	NULL,
	NULL
};

static int mesonuartinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int mesonuartprobe(dev_info_t *);
static int mesonuartattach(dev_info_t *, ddi_attach_cmd_t);
static int mesonuartdetach(dev_info_t *, ddi_detach_cmd_t);
static int mesonuartquiesce(dev_info_t *);

static 	struct cb_ops cb_mesonuart_ops = {
	nodev,			/* cb_open */
	nodev,			/* cb_close */
	nodev,			/* cb_strategy */
	nodev,			/* cb_print */
	nodev,			/* cb_dump */
	nodev,			/* cb_read */
	nodev,			/* cb_write */
	nodev,			/* cb_ioctl */
	nodev,			/* cb_devmap */
	nodev,			/* cb_mmap */
	nodev,			/* cb_segmap */
	nochpoll,		/* cb_chpoll */
	ddi_prop_op,		/* cb_prop_op */
	&mesonuart_str_info,		/* cb_stream */
	D_MP			/* cb_flag */
};

struct dev_ops mesonuart_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* devo_refcnt */
	mesonuartinfo,		/* devo_getinfo */
	nulldev,		/* devo_identify */
	mesonuartprobe,		/* devo_probe */
	mesonuartattach,		/* devo_attach */
	mesonuartdetach,		/* devo_detach */
	nodev,			/* devo_reset */
	&cb_mesonuart_ops,		/* devo_cb_ops */
	NULL,			/* devo_bus_ops */
	NULL,			/* power */
	mesonuartquiesce,		/* quiesce */
};

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a driver */
	"MESONUART driver",
	&mesonuart_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	int i;

	i = ddi_soft_state_init(&mesonuart_soft_state, sizeof (struct mesonuartcom), 2);
	if (i == 0) {
		mutex_init(&mesonuart_glob_lock, NULL, MUTEX_DRIVER, NULL);
		if ((i = mod_install(&modlinkage)) != 0) {
			mutex_destroy(&mesonuart_glob_lock);
			ddi_soft_state_fini(&mesonuart_soft_state);
		} else {
			DEBUGCONT2(MESONUART_DEBUG_INIT, "%s, debug = %x\n",
			    modldrv.drv_linkinfo, debug);
		}
	}
	return (i);
}

int
_fini(void)
{
	int i;

	if ((i = mod_remove(&modlinkage)) == 0) {
		DEBUGCONT1(MESONUART_DEBUG_INIT, "%s unloading\n",
		    modldrv.drv_linkinfo);
		ASSERT(max_mesonuart_instance == -1);
		mutex_destroy(&mesonuart_glob_lock);
		ddi_soft_state_fini(&mesonuart_soft_state);
	}
	return (i);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static void
msasync_put_suspq(struct mesonuartcom *mesonuart, mblk_t *mp)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl));

	if (msasync->msasync_suspqf == NULL)
		msasync->msasync_suspqf = mp;
	else
		msasync->msasync_suspqb->b_next = mp;

	msasync->msasync_suspqb = mp;
}

static mblk_t *
msasync_get_suspq(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	mblk_t *mp;

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl));

	if ((mp = msasync->msasync_suspqf) != NULL) {
		msasync->msasync_suspqf = mp->b_next;
		mp->b_next = NULL;
	} else {
		msasync->msasync_suspqb = NULL;
	}
	return (mp);
}

static void
msasync_process_suspq(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	mblk_t *mp;

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl));

	while ((mp = msasync_get_suspq(mesonuart)) != NULL) {
		queue_t *q;

		q = msasync->msasync_ttycommon.t_writeq;
		ASSERT(q != NULL);
		mutex_exit(&mesonuart->mesonuart_excl);
		(void) mesonuartwputdo(q, mp, B_FALSE);
		mutex_enter(&mesonuart->mesonuart_excl);
	}
	msasync->msasync_flags &= ~MSASYNC_DDI_SUSPENDED;
	cv_broadcast(&msasync->msasync_flags_cv);
}

static int
mesonuartdetach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance;
	struct mesonuartcom *mesonuart;
	struct msasyncline *msasync;

	instance = ddi_get_instance(devi);	/* find out which unit */

	mesonuart = ddi_get_soft_state(mesonuart_soft_state, instance);
	if (mesonuart == NULL)
		return (DDI_FAILURE);
	msasync = mesonuart->mesonuart_priv;

	switch (cmd) {
	case DDI_DETACH:
		DEBUGNOTE2(MESONUART_DEBUG_INIT, "mesonuart%d: %s shutdown.",
		    instance, mesonuart_name(mesonuart));

		/* cancel DTR hold timeout */
		if (msasync->msasync_dtrtid != 0) {
			(void) untimeout(msasync->msasync_dtrtid);
			msasync->msasync_dtrtid = 0;
		}

		/* remove all minor device node(s) for this device */
		ddi_remove_minor_node(devi, NULL);

		mutex_destroy(&mesonuart->mesonuart_excl);
		mutex_destroy(&mesonuart->mesonuart_excl_hi);
		cv_destroy(&msasync->msasync_flags_cv);
		ddi_remove_intr(devi, 0, mesonuart->mesonuart_iblock);
		ddi_regs_map_free(&mesonuart->mesonuart_iohandle);
		ddi_remove_softintr(mesonuart->mesonuart_softintr_id);
		mutex_destroy(&mesonuart->mesonuart_soft_lock);
		mesonuart_soft_state_free(mesonuart);
		DEBUGNOTE1(MESONUART_DEBUG_INIT, "mesonuart%d: shutdown complete",
		    instance);
		break;
	case DDI_SUSPEND:
	default:
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * mesonuartprobe
 * We don't bother probing for the hardware, as since Solaris 2.6, device
 * nodes are only created for auto-detected hardware or nodes explicitly
 * created by the user, e.g. via the DCA. However, we should check the
 * device node is at least vaguely usable, i.e. we have a block of 8 i/o
 * ports. This prevents attempting to attach to bogus serial ports which
 * some BIOSs still partially report when they are disabled in the BIOS.
 */
static int
mesonuartprobe(dev_info_t *dip)
{
	char buf[80];
	pnode_t node = ddi_get_nodeid(dip);
	if (node < 0)
		return (DDI_PROBE_FAILURE);

	int len = prom_getproplen(node, "status");
	if (len <= 0 || len >= sizeof(buf))
		return (DDI_PROBE_FAILURE);

	prom_getprop(node, "status", (caddr_t)buf);
	if (strcmp(buf, "ok") != 0 && strcmp(buf, "okay") != 0)
		return (DDI_PROBE_FAILURE);

	return (DDI_PROBE_SUCCESS);
}

static int
mesonuartattach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	int instance;
	int mcr __unused;	/* XXXARM */
	int ret;
	int i;
	struct mesonuartcom *mesonuart;
	char name[MESONUART_MINOR_LEN];
	int status;
	static ddi_device_acc_attr_t ioattr = {
		DDI_DEVICE_ATTR_V0,
		DDI_NEVERSWAP_ACC,
		DDI_STRICTORDER_ACC,
	};

	instance = ddi_get_instance(devi);	/* find out which unit */

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
	default:
		return (DDI_FAILURE);
	}

	uint_t uart_clock = 1843200;

	pnode_t clock_node = ddi_get_nodeid(devi);
	int len = prom_getproplen(ddi_get_nodeid(devi), "clocks");
	if (len > 0) {
		uint32_t *clocks = __builtin_alloca(len);
		prom_getprop(ddi_get_nodeid(devi), "clocks", (caddr_t)clocks);
		clock_node = prom_findnode_by_phandle(ntohl(clocks[0]));
	}

	if (prom_getproplen(clock_node, "clock-frequency") == sizeof(uint_t)) {
		prom_getprop(clock_node, "clock-frequency", (caddr_t)&uart_clock);
		uart_clock = ntohl(uart_clock);
	}

	ret = ddi_soft_state_zalloc(mesonuart_soft_state, instance);
	if (ret != DDI_SUCCESS)
		return (DDI_FAILURE);
	mesonuart = ddi_get_soft_state(mesonuart_soft_state, instance);
	ASSERT(mesonuart != NULL);	/* can't fail - we only just allocated it */
	mesonuart->mesonuart_unit = instance;
	mutex_enter(&mesonuart_glob_lock);
	if (instance > max_mesonuart_instance)
		max_mesonuart_instance = instance;
	mutex_exit(&mesonuart_glob_lock);

	if (prom_is_compatible(ddi_get_nodeid(devi), "amlogic,meson-uart")) {
		mesonuart->mesonuart_hwtype = MESONUARTMESON;
		mesonuart->mesonuart_fifo_buf = 64;
		mesonuart->mesonuart_use_fifo = FIFO_ON;
	} else {
		cmn_err(CE_WARN, "mesonuart%d: unknown hardware", instance);
		mesonuart_soft_state_free(mesonuart);
		return (DDI_FAILURE);
	}

	mesonuart->mesonuart_clock = uart_clock;

	if (ddi_regs_map_setup(devi, MESONUART_REGISTER_FILE_NO, (caddr_t *)&mesonuart->mesonuart_ioaddr,
	    MESONUART_REGOFFSET, MESONUART_REGISTER_LEN, &ioattr, &mesonuart->mesonuart_iohandle)
	    != DDI_SUCCESS) {
		cmn_err(CE_WARN, "mesonuart%d: could not map UART registers @ %p",
		    instance, (void *)mesonuart->mesonuart_ioaddr);

		mesonuart_soft_state_free(mesonuart);
		return (DDI_FAILURE);
	}

	DEBUGCONT2(MESONUART_DEBUG_INIT, "mesonuart%dattach: UART @ %p\n",
	    instance, (void *)mesonuart->mesonuart_ioaddr);

	mesonuart->mesonuart_com_port = instance + 1;

	/*
	 * It appears that there was msasync hardware that on reset
	 * did not clear ICR.  Hence when we get to
	 * ddi_get_iblock_cookie below, this hardware would cause
	 * the system to hang if there was input available.
	 */
	union uart_misc reg;
	reg.dw = 0;
	reg.tx_irq_count = mesonuart_max_tx_fifo / 2;
	reg.rx_irq_count = 1;
	REG_WRITE(mesonuart, MESON_UART_MISC, reg.dw);
	meson_reset(mesonuart);

	/* establish default usage */
	mesonuart->mesonuart_mcr |= RTS|DTR;		/* do use RTS/DTR after open */
	mesonuart->mesonuart_lcr = STOP1|BITS8;		/* default to 1 stop 8 bits */
	mesonuart->mesonuart_bidx = MESONUART_DEFAULT_BAUD;	/* default to 9600  */
#ifdef DEBUG
	mesonuart->mesonuart_msint_cnt = 0;			/* # of times in msasync_msint */
#endif

	switch (mesonuart_getproperty(devi, mesonuart, "ignore-cd")) {
	case 0:				/* *-ignore-cd=False */
		DEBUGCONT1(MESONUART_DEBUG_MODEM,
		    "mesonuart%dattach: clear MESONUART_IGNORE_CD\n", instance);
		mesonuart->mesonuart_flags &= ~MESONUART_IGNORE_CD; /* wait for cd */
		break;
	case 1:				/* *-ignore-cd=True */
		/*FALLTHRU*/
	default:			/* *-ignore-cd not defined */
		/*
		 * We set rather silly defaults of soft carrier on
		 * and DTR/RTS raised here because it might be that
		 * one of the motherboard ports is the system console.
		 */
		DEBUGCONT1(MESONUART_DEBUG_MODEM,
		    "mesonuart%dattach: set MESONUART_IGNORE_CD, set RTS & DTR\n",
		    instance);
		mcr = mesonuart->mesonuart_mcr;		/* rts/dtr on */
		mesonuart->mesonuart_flags |= MESONUART_IGNORE_CD;	/* ignore cd */
		break;
	}

	/* Parse property for tty modes */
	mesonuart_parse_mode(devi, mesonuart);

	/*
	 * Get icookie for mutexes initialization
	 */
	if ((ddi_get_iblock_cookie(devi, 0, &mesonuart->mesonuart_iblock) !=
	    DDI_SUCCESS) ||
	    (ddi_get_soft_iblock_cookie(devi, DDI_SOFTINT_MED,
	    &mesonuart->mesonuart_soft_iblock) != DDI_SUCCESS)) {
		ddi_regs_map_free(&mesonuart->mesonuart_iohandle);
		cmn_err(CE_CONT,
		    "mesonuart%d: could not hook interrupt for UART @ %p\n",
		    instance, (void *)mesonuart->mesonuart_ioaddr);
		mesonuart_soft_state_free(mesonuart);
		return (DDI_FAILURE);
	}

	/*
	 * Initialize mutexes before accessing the hardware
	 */
	mutex_init(&mesonuart->mesonuart_soft_lock, NULL, MUTEX_DRIVER,
	    (void *)mesonuart->mesonuart_soft_iblock);
	mutex_init(&mesonuart->mesonuart_excl, NULL, MUTEX_DRIVER, NULL);
	mutex_init(&mesonuart->mesonuart_excl_hi, NULL, MUTEX_DRIVER,
	    (void *)mesonuart->mesonuart_iblock);
	mutex_init(&mesonuart->mesonuart_soft_sr, NULL, MUTEX_DRIVER,
	    (void *)mesonuart->mesonuart_soft_iblock);
	mutex_enter(&mesonuart->mesonuart_excl);
	mutex_enter(&mesonuart->mesonuart_excl_hi);

	/* Make UART type visible in device tree for prtconf, etc */
	dev_t dev = makedevice(DDI_MAJOR_T_UNKNOWN, mesonuart->mesonuart_unit);
	ddi_prop_update_string(dev, devi, "uart", mesonuart_name(mesonuart));

	meson_set_baud(mesonuart, mesonuart->mesonuart_bidx);
	meson_set_control(mesonuart);

	mutex_exit(&mesonuart->mesonuart_excl_hi);
	mutex_exit(&mesonuart->mesonuart_excl);

	/*
	 * Set up the other components of the mesonuartcom structure for this port.
	 */
	mesonuart->mesonuart_dip = devi;

	/*
	 * Install per instance software interrupt handler.
	 */
	if (ddi_add_softintr(devi, DDI_SOFTINT_MED,
	    &(mesonuart->mesonuart_softintr_id), NULL, 0, mesonuartsoftintr,
	    (caddr_t)mesonuart) != DDI_SUCCESS) {
		mutex_destroy(&mesonuart->mesonuart_soft_lock);
		mutex_destroy(&mesonuart->mesonuart_excl);
		mutex_destroy(&mesonuart->mesonuart_excl_hi);
		ddi_regs_map_free(&mesonuart->mesonuart_iohandle);
		cmn_err(CE_CONT,
		    "Can not set soft interrupt for MESONUART driver\n");
		mesonuart_soft_state_free(mesonuart);
		return (DDI_FAILURE);
	}

	mutex_enter(&mesonuart->mesonuart_excl);
	mutex_enter(&mesonuart->mesonuart_excl_hi);

	/*
	 * Install interrupt handler for this device.
	 */
	if (ddi_add_intr(devi, 0, NULL, 0, mesonuartintr,
	    (caddr_t)mesonuart) != DDI_SUCCESS) {
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		mutex_exit(&mesonuart->mesonuart_excl);
		ddi_remove_softintr(mesonuart->mesonuart_softintr_id);
		mutex_destroy(&mesonuart->mesonuart_soft_lock);
		mutex_destroy(&mesonuart->mesonuart_excl);
		mutex_destroy(&mesonuart->mesonuart_excl_hi);
		ddi_regs_map_free(&mesonuart->mesonuart_iohandle);
		cmn_err(CE_CONT,
		    "Can not set device interrupt for MESONUART driver\n");
		mesonuart_soft_state_free(mesonuart);
		return (DDI_FAILURE);
	}

	mutex_exit(&mesonuart->mesonuart_excl_hi);
	mutex_exit(&mesonuart->mesonuart_excl);

	mesonuartinit(mesonuart);	/* initialize the msasyncline structure */

	/* create minor device nodes for this device */
	/*
	 * For DOS COM ports, add letter suffix so
	 * devfsadm can create correct link names.
	 */
	name[0] = mesonuart->mesonuart_com_port + 'a' - 1;
	name[1] = '\0';
	status = ddi_create_minor_node(devi, name, S_IFCHR, instance,
	    mesonuart->mesonuart_com_port != 0 ? DDI_NT_SERIAL_MB : DDI_NT_SERIAL, 0);
	if (status == DDI_SUCCESS) {
		(void) strcat(name, ",cu");
		status = ddi_create_minor_node(devi, name, S_IFCHR,
		    OUTLINE | instance,
		    mesonuart->mesonuart_com_port != 0 ? DDI_NT_SERIAL_MB_DO :
		    DDI_NT_SERIAL_DO, 0);
	}

	if (status != DDI_SUCCESS) {
		struct msasyncline *msasync = mesonuart->mesonuart_priv;

		ddi_remove_minor_node(devi, NULL);
		ddi_remove_intr(devi, 0, mesonuart->mesonuart_iblock);
		ddi_remove_softintr(mesonuart->mesonuart_softintr_id);
		mutex_destroy(&mesonuart->mesonuart_soft_lock);
		mutex_destroy(&mesonuart->mesonuart_excl);
		mutex_destroy(&mesonuart->mesonuart_excl_hi);
		cv_destroy(&msasync->msasync_flags_cv);
		ddi_regs_map_free(&mesonuart->mesonuart_iohandle);
		mesonuart_soft_state_free(mesonuart);
		return (DDI_FAILURE);
	}

	/*
	 * Fill in the polled I/O structure.
	 */
	mesonuart->polledio.cons_polledio_version = CONSPOLLEDIO_V0;
	mesonuart->polledio.cons_polledio_argument = (cons_polledio_arg_t)mesonuart;
	mesonuart->polledio.cons_polledio_putchar = mesonuartputchar;
	mesonuart->polledio.cons_polledio_getchar = mesonuartgetchar;
	mesonuart->polledio.cons_polledio_ischar = mesonuartischar;
	mesonuart->polledio.cons_polledio_enter = NULL;
	mesonuart->polledio.cons_polledio_exit = NULL;

	ddi_report_dev(devi);
	DEBUGCONT1(MESONUART_DEBUG_INIT, "mesonuart%dattach: done\n", instance);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
mesonuartinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
	void **result)
{
	dev_t dev = (dev_t)arg;
	int instance, error;
	struct mesonuartcom *mesonuart;

	instance = UNIT(dev);

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		mesonuart = ddi_get_soft_state(mesonuart_soft_state, instance);
		if ((mesonuart == NULL) || (mesonuart->mesonuart_dip == NULL))
			error = DDI_FAILURE;
		else {
			*result = (void *) mesonuart->mesonuart_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)(intptr_t)instance;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/* mesonuart_getproperty -- walk through all name variants until we find a match */

static int
mesonuart_getproperty(dev_info_t *devi, struct mesonuartcom *mesonuart, const char *property)
{
	int len;
	int ret;
	char letter = mesonuart->mesonuart_com_port + 'a' - 1;	/* for ttya */
	char number = mesonuart->mesonuart_com_port + '0';		/* for COM1 */
	char val[40];
	char name[40];

	/* Property for ignoring DCD */
	(void) sprintf(name, "tty%c-%s", letter, property);
	len = sizeof (val);
	ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "com%c-%s", number, property);
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	}
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "tty0%c-%s", number, property);
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	}
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "port-%c-%s", letter, property);
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	}
	if (ret != DDI_PROP_SUCCESS)
		return (-1);		/* property non-existant */
	if (val[0] == 'f' || val[0] == 'F' || val[0] == '0')
		return (0);		/* property false/0 */
	return (1);			/* property true/!0 */
}

/* mesonuart_soft_state_free - local wrapper for ddi_soft_state_free(9F) */

static void
mesonuart_soft_state_free(struct mesonuartcom *mesonuart)
{
	mutex_enter(&mesonuart_glob_lock);
	/* If we were the max_mesonuart_instance, work out new value */
	if (mesonuart->mesonuart_unit == max_mesonuart_instance) {
		while (--max_mesonuart_instance >= 0) {
			if (ddi_get_soft_state(mesonuart_soft_state,
			    max_mesonuart_instance) != NULL)
				break;
		}
	}
	mutex_exit(&mesonuart_glob_lock);

	if (mesonuart->mesonuart_priv != NULL) {
		kmem_free(mesonuart->mesonuart_priv, sizeof (struct msasyncline));
		mesonuart->mesonuart_priv = NULL;
	}
	ddi_soft_state_free(mesonuart_soft_state, mesonuart->mesonuart_unit);
}

static char *
mesonuart_name(struct mesonuartcom *mesonuart)
{
	switch (mesonuart->mesonuart_hwtype) {
	case MESONUART16550A:
		return ("16550A");
	case MESONUARTMESON:
		return ("meson");
	default:
		DEBUGNOTE2(MESONUART_DEBUG_INIT,
		    "mesonuart%d: mesonuart_name: unknown mesonuart_hwtype: %d",
		    mesonuart->mesonuart_unit, mesonuart->mesonuart_hwtype);
		return ("?");
	}
}

/*
 * mesonuartinit() initializes the TTY protocol-private data for this channel
 * before enabling the interrupts.
 */
static void
mesonuartinit(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync;

	mesonuart->mesonuart_priv = kmem_zalloc(sizeof (struct msasyncline), KM_SLEEP);
	msasync = mesonuart->mesonuart_priv;
	mutex_enter(&mesonuart->mesonuart_excl);
	msasync->msasync_common = mesonuart;
	cv_init(&msasync->msasync_flags_cv, NULL, CV_DRIVER, NULL);
	mutex_exit(&mesonuart->mesonuart_excl);
}

/*ARGSUSED3*/
static int
mesonuartopen(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr)
{
	struct mesonuartcom	*mesonuart;
	struct msasyncline *msasync;
	int		mcr;
	int		unit;
	int 		len;
	struct termios 	*termiosp;

	unit = UNIT(*dev);
	DEBUGCONT1(MESONUART_DEBUG_CLOSE, "mesonuart%dopen\n", unit);
	mesonuart = ddi_get_soft_state(mesonuart_soft_state, unit);
	if (mesonuart == NULL)
		return (ENXIO);		/* unit not configured */
	msasync = mesonuart->mesonuart_priv;
	mutex_enter(&mesonuart->mesonuart_excl);

again:
	mutex_enter(&mesonuart->mesonuart_excl_hi);

	/*
	 * Block waiting for carrier to come up, unless this is a no-delay open.
	 */
	if (!(msasync->msasync_flags & MSASYNC_ISOPEN)) {
		/*
		 * Set the default termios settings (cflag).
		 * Others are set in ldterm.
		 */
		mutex_exit(&mesonuart->mesonuart_excl_hi);

		if (ddi_getlongprop(DDI_DEV_T_ANY, ddi_root_node(),
		    0, "ttymodes",
		    (caddr_t)&termiosp, &len) == DDI_PROP_SUCCESS &&
		    len == sizeof (struct termios)) {
			msasync->msasync_ttycommon.t_cflag = termiosp->c_cflag;
			kmem_free(termiosp, len);
		} else
			cmn_err(CE_WARN,
			    "mesonuart: couldn't get ttymodes property!");
		mutex_enter(&mesonuart->mesonuart_excl_hi);

		/* eeprom mode support - respect properties */
		if (mesonuart->mesonuart_cflag)
			msasync->msasync_ttycommon.t_cflag = mesonuart->mesonuart_cflag;

		msasync->msasync_ttycommon.t_iflag = 0;
		msasync->msasync_ttycommon.t_iocpending = NULL;
		msasync->msasync_ttycommon.t_size.ws_row = 0;
		msasync->msasync_ttycommon.t_size.ws_col = 0;
		msasync->msasync_ttycommon.t_size.ws_xpixel = 0;
		msasync->msasync_ttycommon.t_size.ws_ypixel = 0;
		msasync->msasync_dev = *dev;
		msasync->msasync_wbufcid = 0;

		msasync->msasync_startc = CSTART;
		msasync->msasync_stopc = CSTOP;
		mesonuart_program(mesonuart, MESONUART_INIT);
	} else
		if ((msasync->msasync_ttycommon.t_flags & TS_XCLUDE) &&
		    secpolicy_excl_open(cr) != 0) {
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		mutex_exit(&mesonuart->mesonuart_excl);
		return (EBUSY);
	} else if ((*dev & OUTLINE) && !(msasync->msasync_flags & MSASYNC_OUT)) {
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		mutex_exit(&mesonuart->mesonuart_excl);
		return (EBUSY);
	}

	if (*dev & OUTLINE)
		msasync->msasync_flags |= MSASYNC_OUT;

	/* Raise DTR on every open, but delay if it was just lowered. */
	while (msasync->msasync_flags & MSASYNC_DTR_DELAY) {
		DEBUGCONT1(MESONUART_DEBUG_MODEM,
		    "mesonuart%dopen: waiting for the MSASYNC_DTR_DELAY to be clear\n",
		    unit);
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		if (cv_wait_sig(&msasync->msasync_flags_cv,
		    &mesonuart->mesonuart_excl) == 0) {
			DEBUGCONT1(MESONUART_DEBUG_MODEM,
			    "mesonuart%dopen: interrupted by signal, exiting\n",
			    unit);
			mutex_exit(&mesonuart->mesonuart_excl);
			return (EINTR);
		}
		mutex_enter(&mesonuart->mesonuart_excl_hi);
	}

	if (mesonuart->mesonuart_flags & MESONUART_IGNORE_CD) {
		DEBUGCONT1(MESONUART_DEBUG_MODEM,
		    "mesonuart%dopen: MESONUART_IGNORE_CD set, set TS_SOFTCAR\n",
		    unit);
		msasync->msasync_ttycommon.t_flags |= TS_SOFTCAR;
	}
	else
		msasync->msasync_ttycommon.t_flags &= ~TS_SOFTCAR;

	/*
	 * Check carrier.
	 */
	mesonuart->mesonuart_msr = meson_get_msr(mesonuart);

	if (mesonuart->mesonuart_msr & DCD)
		msasync->msasync_flags |= MSASYNC_CARR_ON;
	else
		msasync->msasync_flags &= ~MSASYNC_CARR_ON;
	mutex_exit(&mesonuart->mesonuart_excl_hi);

	/*
	 * If FNDELAY and FNONBLOCK are clear, block until carrier up.
	 * Quit on interrupt.
	 */
	if (!(flag & (FNDELAY|FNONBLOCK)) &&
	    !(msasync->msasync_ttycommon.t_cflag & CLOCAL)) {
		if ((!(msasync->msasync_flags & (MSASYNC_CARR_ON|MSASYNC_OUT)) &&
		    !(msasync->msasync_ttycommon.t_flags & TS_SOFTCAR)) ||
		    ((msasync->msasync_flags & MSASYNC_OUT) &&
		    !(*dev & OUTLINE))) {
			msasync->msasync_flags |= MSASYNC_WOPEN;
			if (cv_wait_sig(&msasync->msasync_flags_cv,
			    &mesonuart->mesonuart_excl) == B_FALSE) {
				msasync->msasync_flags &= ~MSASYNC_WOPEN;
				mutex_exit(&mesonuart->mesonuart_excl);
				return (EINTR);
			}
			msasync->msasync_flags &= ~MSASYNC_WOPEN;
			goto again;
		}
	} else if ((msasync->msasync_flags & MSASYNC_OUT) && !(*dev & OUTLINE)) {
		mutex_exit(&mesonuart->mesonuart_excl);
		return (EBUSY);
	}

	msasync->msasync_ttycommon.t_readq = rq;
	msasync->msasync_ttycommon.t_writeq = WR(rq);
	rq->q_ptr = WR(rq)->q_ptr = (caddr_t)msasync;
	mutex_exit(&mesonuart->mesonuart_excl);
	/*
	 * Caution here -- qprocson sets the pointers that are used by canput
	 * called by msasync_softint.  MSASYNC_ISOPEN must *not* be set until those
	 * pointers are valid.
	 */
	qprocson(rq);
	msasync->msasync_flags |= MSASYNC_ISOPEN;
	msasync->msasync_polltid = 0;
	DEBUGCONT1(MESONUART_DEBUG_INIT, "mesonuart%dopen: done\n", unit);
	return (0);
}

static void
msasync_progress_check(void *arg)
{
	struct msasyncline *msasync = arg;
	struct mesonuartcom	 *mesonuart = msasync->msasync_common;
	mblk_t *bp;

	/*
	 * We define "progress" as either waiting on a timed break or delay, or
	 * having had at least one transmitter interrupt.  If none of these are
	 * true, then just terminate the output and wake up that close thread.
	 */
	mutex_enter(&mesonuart->mesonuart_excl);
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	if (!(msasync->msasync_flags & (MSASYNC_BREAK|MSASYNC_DELAY|MSASYNC_PROGRESS))) {
		msasync->msasync_ocnt = 0;
		msasync->msasync_flags &= ~MSASYNC_BUSY;
		msasync->msasync_timer = 0;
		bp = msasync->msasync_xmitblk;
		msasync->msasync_xmitblk = NULL;
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		if (bp != NULL)
			freeb(bp);
		/*
		 * Since this timer is running, we know that we're in exit(2).
		 * That means that the user can't possibly be waiting on any
		 * valid ioctl(2) completion anymore, and we should just flush
		 * everything.
		 */
		flushq(msasync->msasync_ttycommon.t_writeq, FLUSHALL);
		cv_broadcast(&msasync->msasync_flags_cv);
	} else {
		msasync->msasync_flags &= ~MSASYNC_PROGRESS;
		msasync->msasync_timer = timeout(msasync_progress_check, msasync, drv_usectohz(mesonuart_drain_check));
		mutex_exit(&mesonuart->mesonuart_excl_hi);
	}
	mutex_exit(&mesonuart->mesonuart_excl);
}

/*
 * Release DTR so that mesonuartopen() can raise it.
 */
static void
msasync_dtr_free(struct msasyncline *msasync)
{
	struct mesonuartcom *mesonuart = msasync->msasync_common;

	DEBUGCONT0(MESONUART_DEBUG_MODEM,
	    "msasync_dtr_free, clearing MSASYNC_DTR_DELAY\n");
	mutex_enter(&mesonuart->mesonuart_excl);
	msasync->msasync_flags &= ~MSASYNC_DTR_DELAY;
	msasync->msasync_dtrtid = 0;
	cv_broadcast(&msasync->msasync_flags_cv);
	mutex_exit(&mesonuart->mesonuart_excl);
}

/*
 * Close routine.
 */
/*ARGSUSED2*/
static int
mesonuartclose(queue_t *q, int flag, cred_t *credp)
{
	struct msasyncline *msasync;
	struct mesonuartcom	 *mesonuart;
	int icr, lcr;
#ifdef DEBUG
	int instance;
#endif

	msasync = (struct msasyncline *)q->q_ptr;
	ASSERT(msasync != NULL);
#ifdef DEBUG
	instance = UNIT(msasync->msasync_dev);
	DEBUGCONT1(MESONUART_DEBUG_CLOSE, "mesonuart%dclose\n", instance);
#endif
	mesonuart = msasync->msasync_common;

	mutex_enter(&mesonuart->mesonuart_excl);
	msasync->msasync_flags |= MSASYNC_CLOSING;

	/*
	 * Turn off PPS handling early to avoid events occuring during
	 * close.  Also reset the DCD edge monitoring bit.
	 */
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	mesonuart->mesonuart_flags &= ~(MESONUART_PPS | MESONUART_PPS_EDGE);
	mutex_exit(&mesonuart->mesonuart_excl_hi);

	/*
	 * There are two flavors of break -- timed (M_BREAK or TCSBRK) and
	 * untimed (TIOCSBRK).  For the timed case, these are enqueued on our
	 * write queue and there's a timer running, so we don't have to worry
	 * about them.  For the untimed case, though, the user obviously made a
	 * mistake, because these are handled immediately.  We'll terminate the
	 * break now and honor his implicit request by discarding the rest of
	 * the data.
	 */
	if (msasync->msasync_flags & MSASYNC_OUT_SUSPEND) {
		if (msasync->msasync_utbrktid != 0) {
			(void) untimeout(msasync->msasync_utbrktid);
			msasync->msasync_utbrktid = 0;
		}
		msasync->msasync_flags &= ~MSASYNC_OUT_SUSPEND;
		goto nodrain;
	}

	/*
	 * If the user told us not to delay the close ("non-blocking"), then
	 * don't bother trying to drain.
	 *
	 * If the user did M_STOP (MSASYNC_STOPPED), there's no hope of ever
	 * getting an M_START (since these messages aren't enqueued), and the
	 * only other way to clear the stop condition is by loss of DCD, which
	 * would discard the queue data.  Thus, we drop the output data if
	 * MSASYNC_STOPPED is set.
	 */
	if ((flag & (FNDELAY|FNONBLOCK)) ||
	    (msasync->msasync_flags & MSASYNC_STOPPED)) {
		goto nodrain;
	}

	/*
	 * If there's any pending output, then we have to try to drain it.
	 * There are two main cases to be handled:
	 *	- called by close(2): need to drain until done or until
	 *	  a signal is received.  No timeout.
	 *	- called by exit(2): need to drain while making progress
	 *	  or until a timeout occurs.  No signals.
	 *
	 * If we can't rely on receiving a signal to get us out of a hung
	 * session, then we have to use a timer.  In this case, we set a timer
	 * to check for progress in sending the output data -- all that we ask
	 * (at each interval) is that there's been some progress made.  Since
	 * the interrupt routine grabs buffers from the write queue, we can't
	 * trust changes in msasync_ocnt.  Instead, we use a progress flag.
	 *
	 * Note that loss of carrier will cause the output queue to be flushed,
	 * and we'll wake up again and finish normally.
	 */
	if (!ddi_can_receive_sig() && mesonuart_drain_check != 0) {
		msasync->msasync_flags &= ~MSASYNC_PROGRESS;
		msasync->msasync_timer = timeout(msasync_progress_check, msasync, drv_usectohz(mesonuart_drain_check));
	}
	while (msasync->msasync_ocnt > 0 ||
	    msasync->msasync_ttycommon.t_writeq->q_first != NULL ||
	    (msasync->msasync_flags & (MSASYNC_BUSY|MSASYNC_BREAK|MSASYNC_DELAY))) {
		if (cv_wait_sig(&msasync->msasync_flags_cv, &mesonuart->mesonuart_excl) == 0)
			break;
	}
	if (msasync->msasync_timer != 0) {
		(void) untimeout(msasync->msasync_timer);
		msasync->msasync_timer = 0;
	}

nodrain:
	msasync->msasync_ocnt = 0;
	if (msasync->msasync_xmitblk != NULL)
		freeb(msasync->msasync_xmitblk);
	msasync->msasync_xmitblk = NULL;

	/*
	 * If line has HUPCL set or is incompletely opened fix up the modem
	 * lines.
	 */
	DEBUGCONT1(MESONUART_DEBUG_MODEM, "mesonuart%dclose: next check HUPCL flag\n",
	    instance);
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	if ((msasync->msasync_ttycommon.t_cflag & HUPCL) ||
	    (msasync->msasync_flags & MSASYNC_WOPEN)) {
		DEBUGCONT3(MESONUART_DEBUG_MODEM,
		    "mesonuart%dclose: HUPCL flag = %x, MSASYNC_WOPEN flag = %x\n",
		    instance,
		    msasync->msasync_ttycommon.t_cflag & HUPCL,
		    msasync->msasync_ttycommon.t_cflag & MSASYNC_WOPEN);
		msasync->msasync_flags |= MSASYNC_DTR_DELAY;

		/* turn off DTR, RTS but NOT interrupt to 386 */
		if (mesonuart->mesonuart_flags & (MESONUART_IGNORE_CD|MESONUART_RTS_DTR_OFF)) {
			DEBUGCONT3(MESONUART_DEBUG_MODEM,
			    "mesonuart%dclose: MESONUART_IGNORE_CD flag = %x, "
			    "MESONUART_RTS_DTR_OFF flag = %x\n",
			    instance,
			    mesonuart->mesonuart_flags & MESONUART_IGNORE_CD,
			    mesonuart->mesonuart_flags & MESONUART_RTS_DTR_OFF);
		} else {
			DEBUGCONT1(MESONUART_DEBUG_MODEM,
			    "mesonuart%dclose: Dropping DTR and RTS\n", instance);
		}
		msasync->msasync_dtrtid =
		    timeout((void (*)())msasync_dtr_free,
		    (caddr_t)msasync, drv_usectohz(mesonuart_min_dtr_low));
	}
	/*
	 * If nobody's using it now, turn off receiver interrupts.
	 */
	if ((msasync->msasync_flags & (MSASYNC_WOPEN|MSASYNC_ISOPEN)) == 0) {
		meson_set_icr(mesonuart, 0, RIEN);
	}
	mutex_exit(&mesonuart->mesonuart_excl_hi);
out:
	ttycommon_close(&msasync->msasync_ttycommon);

	/*
	 * Cancel outstanding "bufcall" request.
	 */
	if (msasync->msasync_wbufcid != 0) {
		unbufcall(msasync->msasync_wbufcid);
		msasync->msasync_wbufcid = 0;
	}

	/* Note that qprocsoff can't be done until after interrupts are off */
	qprocsoff(q);
	q->q_ptr = WR(q)->q_ptr = NULL;
	msasync->msasync_ttycommon.t_readq = NULL;
	msasync->msasync_ttycommon.t_writeq = NULL;

	/*
	 * Clear out device state, except persistant device property flags.
	 */
	msasync->msasync_flags &= (MSASYNC_DTR_DELAY|MESONUART_RTS_DTR_OFF);
	cv_broadcast(&msasync->msasync_flags_cv);
	mutex_exit(&mesonuart->mesonuart_excl);

	DEBUGCONT1(MESONUART_DEBUG_CLOSE, "mesonuart%dclose: done\n", instance);
	return (0);
}

static boolean_t
mesonuart_isbusy(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync;

	DEBUGCONT0(MESONUART_DEBUG_EOT, "mesonuart_isbusy\n");
	msasync = mesonuart->mesonuart_priv;
	ASSERT(mutex_owned(&mesonuart->mesonuart_excl));
	ASSERT(mutex_owned(&mesonuart->mesonuart_excl_hi));
/*
 * XXXX this should be recoded
 */
	return ((msasync->msasync_ocnt > 0) || meson_is_busy(mesonuart));
}

static void
mesonuart_waiteot(struct mesonuartcom *mesonuart)
{
	/*
	 * Wait for the current transmission block and the
	 * current fifo data to transmit. Once this is done
	 * we may go on.
	 */
	DEBUGCONT0(MESONUART_DEBUG_EOT, "mesonuart_waiteot\n");
	ASSERT(mutex_owned(&mesonuart->mesonuart_excl));
	ASSERT(mutex_owned(&mesonuart->mesonuart_excl_hi));
	while (mesonuart_isbusy(mesonuart)) {
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		mutex_exit(&mesonuart->mesonuart_excl);
		drv_usecwait(10000);		/* wait .01 */
		mutex_enter(&mesonuart->mesonuart_excl);
		mutex_enter(&mesonuart->mesonuart_excl_hi);
	}
}

/*
 * Program the MESONUART port. Most of the msasync operation is based on the values
 * of 'c_iflag' and 'c_cflag'.
 */

#define	BAUDINDEX(cflg)	(((cflg) & CBAUDEXT) ? \
			(((cflg) & CBAUD) + CBAUD + 1) : ((cflg) & CBAUD))

static void
mesonuart_program(struct mesonuartcom *mesonuart, int mode)
{
	struct msasyncline *msasync;
	int baudrate, c_flag;
	int flush_reg;
	int ocflags;
	int icr;
#ifdef DEBUG
	int instance;
#endif

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl));
	ASSERT(mutex_owned(&mesonuart->mesonuart_excl_hi));

	msasync = mesonuart->mesonuart_priv;
#ifdef DEBUG
	instance = UNIT(msasync->msasync_dev);
	DEBUGCONT2(MESONUART_DEBUG_PROCS,
	    "mesonuart%d_program: mode = 0x%08X, enter\n", instance, mode);
#endif

	baudrate = BAUDINDEX(msasync->msasync_ttycommon.t_cflag);

	msasync->msasync_ttycommon.t_cflag &= ~(CIBAUD);

	if (baudrate > CBAUD) {
		msasync->msasync_ttycommon.t_cflag |= CIBAUDEXT;
		msasync->msasync_ttycommon.t_cflag |=
		    (((baudrate - CBAUD - 1) << IBSHIFT) & CIBAUD);
	} else {
		msasync->msasync_ttycommon.t_cflag &= ~CIBAUDEXT;
		msasync->msasync_ttycommon.t_cflag |=
		    ((baudrate << IBSHIFT) & CIBAUD);
	}

	c_flag = msasync->msasync_ttycommon.t_cflag &
	    (CLOCAL|CREAD|CSTOPB|CSIZE|PARENB|PARODD|CBAUD|CBAUDEXT);

	ocflags = mesonuart->mesonuart_ocflag;

	/* flush/reset the status registers */
	mesonuart->mesonuart_msr = flush_reg = meson_get_msr(mesonuart);
	/*
	 * The device is programmed in the open sequence, if we
	 * have to hardware handshake, then this is a good time
	 * to check if the device can receive any data.
	 */

	if ((CRTSCTS & msasync->msasync_ttycommon.t_cflag) && !(flush_reg & CTS)) {
		msasync_flowcontrol_hw_output(mesonuart, FLOW_STOP);
	} else {
		/*
		 * We can not use msasync_flowcontrol_hw_output(mesonuart, FLOW_START)
		 * here, because if CRTSCTS is clear, we need clear
		 * MSASYNC_HW_OUT_FLW bit.
		 */
		msasync->msasync_flags &= ~MSASYNC_HW_OUT_FLW;
	}

	/*
	 * If IXON is not set, clear MSASYNC_SW_OUT_FLW;
	 * If IXON is set, no matter what IXON flag is before this
	 * function call to mesonuart_program,
	 * we will use the old MSASYNC_SW_OUT_FLW status.
	 * Because of handling IXON in the driver, we also should re-calculate
	 * the value of MSASYNC_OUT_FLW_RESUME bit, but in fact,
	 * the TCSET* commands which call mesonuart_program
	 * are put into the write queue, so there is no output needed to
	 * be resumed at this point.
	 */
	if (!(IXON & msasync->msasync_ttycommon.t_iflag))
		msasync->msasync_flags &= ~MSASYNC_SW_OUT_FLW;

	if (ocflags != (c_flag & ~CLOCAL) || mode == MESONUART_INIT) {
		/* Set line control */
		int lcr = mesonuart->mesonuart_lcr;
		lcr &= ~(WLS0|WLS1|STB|PEN|EPS);

		if (c_flag & CSTOPB)
			lcr |= STB;	/* 2 stop bits */

		if (c_flag & PARENB)
			lcr |= PEN;

		if ((c_flag & PARODD) == 0)
			lcr |= EPS;

		switch (c_flag & CSIZE) {
		case CS5:
			lcr |= BITS5;
			break;
		case CS6:
			lcr |= BITS6;
			break;
		case CS7:
			lcr |= BITS7;
			break;
		case CS8:
			lcr |= BITS8;
			break;
		}

		/* set the baud rate, unless it is "0" */
		if (baudrate != 0) {
			meson_set_baud(mesonuart, baudrate);
		}
		/* set the line control modes */
		mesonuart->mesonuart_lcr = lcr;
		meson_set_control(mesonuart);

		/*
		 * If we have a FIFO buffer, enable/flush
		 * at intialize time, flush if transitioning from
		 * CREAD off to CREAD on.
		 */
		if ((ocflags & CREAD) == 0 && (c_flag & CREAD) ||
		    mode == MESONUART_INIT)
			if (mesonuart->mesonuart_use_fifo == FIFO_ON)
				meson_reset_fifo(mesonuart, FIFORXFLSH);

		/* remember the new cflags */
		mesonuart->mesonuart_ocflag = c_flag & ~CLOCAL;
	}

	/*
	 * Call the modem status interrupt handler to check for the carrier
	 * in case CLOCAL was turned off after the carrier came on.
	 * (Note: Modem status interrupt is not enabled if CLOCAL is ON.)
	 */
	msasync_msint(mesonuart);

	/* Set interrupt control */
	DEBUGCONT3(MESONUART_DEBUG_MODM2,
	    "mesonuart%d_program: c_flag & CLOCAL = %x t_cflag & CRTSCTS = %x\n",
	    instance, c_flag & CLOCAL,
	    msasync->msasync_ttycommon.t_cflag & CRTSCTS);

	if ((c_flag & CLOCAL) && !(msasync->msasync_ttycommon.t_cflag & CRTSCTS))
		/*
		 * direct-wired line ignores DCD, so we don't enable modem
		 * status interrupts.
		 */
		icr = (TIEN | SIEN);
	else
		icr = (TIEN | SIEN | MIEN);

	if (c_flag & CREAD)
		icr |= RIEN;

	meson_set_icr(mesonuart, icr, TIEN | SIEN | MIEN | RIEN);
	DEBUGCONT1(MESONUART_DEBUG_PROCS, "mesonuart%d_program: done\n", instance);
}

static boolean_t
mesonuart_baudok(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	int baudrate;

	baudrate = BAUDINDEX(msasync->msasync_ttycommon.t_cflag);

	if (baudrate >= N_SU_SPEEDS)
		return (0);

	return 1;
}

/*
 * mesonuartintr() is the High Level Interrupt Handler.
 *
 * There are four different interrupt types indexed by ISR register values:
 *		0: modem
 *		1: Tx holding register is empty, ready for next char
 *		2: Rx register now holds a char to be picked up
 *		3: error or break on line
 * This routine checks the Bit 0 (interrupt-not-pending) to determine if
 * the interrupt is from this port.
 */
uint_t
mesonuartintr(caddr_t argmesonuart)
{
	struct mesonuartcom		*mesonuart = (struct mesonuartcom *)argmesonuart;
	struct msasyncline	*msasync;

	mutex_enter(&mesonuart->mesonuart_excl_hi);

	msasync = mesonuart->mesonuart_priv;


	if ((msasync == NULL) || !(msasync->msasync_flags & (MSASYNC_ISOPEN|MSASYNC_WOPEN))) {
		while (meson_rx_is_ready(mesonuart))
			meson_get_char(mesonuart);

		union uart_status status;
		status.dw = REG_READ(mesonuart, MESON_UART_STATUS);
		if (status.parity_error || status.frame_error || status.rx_fifo_overflow) {
			union uart_control control;
			control.dw = REG_READ(mesonuart, MESON_UART_CONTROL);
			control.clear_error = 1;
			REG_WRITE(mesonuart, MESON_UART_CONTROL, control.dw);
			control.clear_error = 0;
			REG_READ(mesonuart, MESON_UART_CONTROL);
			REG_WRITE(mesonuart, MESON_UART_CONTROL, control.dw);
		}
	} else {
		msasync_rxint(mesonuart);
		msasync_txint(mesonuart);
	}
	mutex_exit(&mesonuart->mesonuart_excl_hi);
	return DDI_INTR_CLAIMED;
}

/*
 * Transmitter interrupt service routine.
 * If there is more data to transmit in the current pseudo-DMA block,
 * send the next character if output is not stopped or draining.
 * Otherwise, queue up a soft interrupt.
 *
 * XXX -  Needs review for HW FIFOs.
 */
static void
msasync_txint(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;

	/*
	 * If MSASYNC_BREAK or MSASYNC_OUT_SUSPEND has been set, return to
	 * mesonuartintr()'s context to claim the interrupt without performing
	 * any action. No character will be loaded into FIFO/THR until
	 * timed or untimed break is removed
	 */
	if (msasync->msasync_flags & (MSASYNC_BREAK|MSASYNC_OUT_SUSPEND)) {
		return;
	}

	msasync_flowcontrol_sw_input(mesonuart, FLOW_CHECK, IN_FLOW_NULL);

	int x = 0;
	if (!(msasync->msasync_flags &
	    (MSASYNC_HW_OUT_FLW|MSASYNC_SW_OUT_FLW|MSASYNC_STOPPED))) {
		while (msasync->msasync_ocnt) {
			if (!meson_tx_is_ready(mesonuart))
				break;
			meson_put_char(mesonuart, *msasync->msasync_optr++);
			msasync->msasync_ocnt--;
			if (++x >= mesonuart_max_tx_fifo)
				break;
		}
		msasync->msasync_flags |= MSASYNC_PROGRESS;
	}

	MESONUARTSETSOFT(mesonuart);
}

/*
 * Interrupt on port: handle PPS event.  This function is only called
 * for a port on which PPS event handling has been enabled.
 */
static void
mesonuart_ppsevent(struct mesonuartcom *mesonuart, int msr)
{
	if (mesonuart->mesonuart_flags & MESONUART_PPS_EDGE) {
		/* Have seen leading edge, now look for and record drop */
		if ((msr & DCD) == 0)
			mesonuart->mesonuart_flags &= ~MESONUART_PPS_EDGE;
		/*
		 * Waiting for leading edge, look for rise; stamp event and
		 * calibrate kernel clock.
		 */
	} else if (msr & DCD) {
			/*
			 * This code captures a timestamp at the designated
			 * transition of the PPS signal (DCD asserted).  The
			 * code provides a pointer to the timestamp, as well
			 * as the hardware counter value at the capture.
			 *
			 * Note: the kernel has nano based time values while
			 * NTP requires micro based, an in-line fast algorithm
			 * to convert nsec to usec is used here -- see hrt2ts()
			 * in common/os/timers.c for a full description.
			 */
			struct timeval *tvp = &mesonuart_ppsev.tv;
			timestruc_t ts;
			long nsec, usec;

			mesonuart->mesonuart_flags |= MESONUART_PPS_EDGE;
			LED_OFF;
			gethrestime(&ts);
			LED_ON;
			nsec = ts.tv_nsec;
			usec = nsec + (nsec >> 2);
			usec = nsec + (usec >> 1);
			usec = nsec + (usec >> 2);
			usec = nsec + (usec >> 4);
			usec = nsec - (usec >> 3);
			usec = nsec + (usec >> 2);
			usec = nsec + (usec >> 3);
			usec = nsec + (usec >> 4);
			usec = nsec + (usec >> 1);
			usec = nsec + (usec >> 6);
			tvp->tv_usec = usec >> 10;
			tvp->tv_sec = ts.tv_sec;

			++mesonuart_ppsev.serial;

			/*
			 * Because the kernel keeps a high-resolution time,
			 * pass the current highres timestamp in tvp and zero
			 * in usec.
			 */
			ddi_hardpps(tvp, 0);
	}
}

/*
 * Receiver interrupt: RxRDY interrupt, FIFO timeout interrupt or receive
 * error interrupt.
 * Try to put the character into the circular buffer for this line; if it
 * overflows, indicate a circular buffer overrun. If this port is always
 * to be serviced immediately, or the character is a STOP character, or
 * more than 15 characters have arrived, queue up a soft interrupt to
 * drain the circular buffer.
 * XXX - needs review for hw FIFOs support.
 */

static void
msasync_rxint(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	uchar_t c;
	uint_t s, needsoft = 0;
	tty_common_t *tp;

	tp = &msasync->msasync_ttycommon;
	if (!(tp->t_cflag & CREAD)) {
		while (meson_rx_is_ready(mesonuart))
			meson_get_char(mesonuart);
		return; /* line is not open for read? */
	}

	for (;;) {
		union uart_status status;
		status.dw = REG_READ(mesonuart, MESON_UART_STATUS);
		c = 0;
		s = 0;				/* reset error status */

		if (status.rx_fifo_empty)
			break;

		/* Handle framing errors */
		if (status.parity_error || status.frame_error || status.rx_fifo_overflow) {
			if (status.parity_error) {
				if (tp->t_iflag & INPCK) /* parity enabled */
					s |= PERROR;
			}

			if (status.frame_error)
				s |= FRERROR;
			if (status.rx_fifo_overflow) {
				msasync->msasync_hw_overrun = 1;
				s |= OVERRUN;
			}

			union uart_control control;
			control.dw = REG_READ(mesonuart, MESON_UART_CONTROL);
			control.clear_error = 1;
			REG_WRITE(mesonuart, MESON_UART_CONTROL, control.dw);
			control.clear_error = 0;
			REG_READ(mesonuart, MESON_UART_CONTROL);
			REG_WRITE(mesonuart, MESON_UART_CONTROL, control.dw);
		}

		c = meson_get_char(mesonuart) & 0xff;

		/*
		 * We handle XON/XOFF char if IXON is set,
		 * but if received char is _POSIX_VDISABLE,
		 * we left it to the up level module.
		 */
		if (tp->t_iflag & IXON) {
			if ((c == msasync->msasync_stopc) && (c != _POSIX_VDISABLE)) {
				msasync_flowcontrol_sw_output(mesonuart, FLOW_STOP);
				continue;
			} else if ((c == msasync->msasync_startc) && (c != _POSIX_VDISABLE)) {
				msasync_flowcontrol_sw_output(mesonuart, FLOW_START);
				needsoft = 1;
				continue;
			}
			if ((tp->t_iflag & IXANY) && (msasync->msasync_flags & MSASYNC_SW_OUT_FLW)) {
				msasync_flowcontrol_sw_output(mesonuart, FLOW_START);
				needsoft = 1;
			}
		}

		/*
		 * Check for character break sequence
		 */
		if ((abort_enable == KIOCABORTALTERNATE) && (mesonuart->mesonuart_flags & MESONUART_CONSOLE)) {
			if (abort_charseq_recognize(c))
				abort_sequence_enter((char *)NULL);
		}

		if (s == 0) {
			if ((tp->t_iflag & PARMRK) && !(tp->t_iflag & (IGNPAR|ISTRIP)) && (c == 0377)) {
				if (RING_POK(msasync, 2)) {
					RING_PUT(msasync, 0377);
					RING_PUT(msasync, c);
				} else {
					msasync->msasync_sw_overrun = 1;
				}
			} else {
				if (RING_POK(msasync, 1))
					RING_PUT(msasync, c);
				else
					msasync->msasync_sw_overrun = 1;
			}
		} else {
			if (s & FRERROR) {/* Handle framing errors */
				if (c == 0) {
					if ((mesonuart->mesonuart_flags & MESONUART_CONSOLE) && (abort_enable != KIOCABORTALTERNATE))
						abort_sequence_enter((char *)0);
					else
						msasync->msasync_break++;
				} else {
					if (RING_POK(msasync, 1))
						RING_MARK(msasync, c, s);
					else
						msasync->msasync_sw_overrun = 1;
				}
			} else { /* Parity errors are handled by ldterm */
				if (RING_POK(msasync, 1))
					RING_MARK(msasync, c, s);
				else
					msasync->msasync_sw_overrun = 1;
			}
		}
	}

	if ((RING_CNT(msasync) > (RINGSIZE * 3)/4) && !(msasync->msasync_inflow_source & IN_FLOW_RINGBUFF)) {
		msasync_flowcontrol_hw_input(mesonuart, FLOW_STOP, IN_FLOW_RINGBUFF);
		msasync_flowcontrol_sw_input(mesonuart, FLOW_STOP, IN_FLOW_RINGBUFF);
	}

	if ((msasync->msasync_flags & MSASYNC_SERVICEIMM) || needsoft || (RING_FRAC(msasync)) || (msasync->msasync_polltid == 0))
		MESONUARTSETSOFT(mesonuart);	/* need a soft interrupt */
}

/*
 * Modem status interrupt.
 *
 * (Note: It is assumed that the MSR hasn't been read by mesonuartintr().)
 */

static void
msasync_msint(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	int msr, t_cflag = msasync->msasync_ttycommon.t_cflag;
#ifdef DEBUG
	int instance = UNIT(msasync->msasync_dev);
#endif

msasync_msint_retry:
	/* this resets the interrupt */
	msr = meson_get_msr(mesonuart);
	DEBUGCONT10(MESONUART_DEBUG_STATE,
	    "msasync%d_msint call #%d:\n"
	    "   transition: %3s %3s %3s %3s\n"
	    "current state: %3s %3s %3s %3s\n",
	    instance,
	    ++(mesonuart->mesonuart_msint_cnt),
	    (msr & DCTS) ? "DCTS" : "    ",
	    (msr & DDSR) ? "DDSR" : "    ",
	    (msr & DRI)  ? "DRI " : "    ",
	    (msr & DDCD) ? "DDCD" : "    ",
	    (msr & CTS)  ? "CTS " : "    ",
	    (msr & DSR)  ? "DSR " : "    ",
	    (msr & RI)   ? "RI  " : "    ",
	    (msr & DCD)  ? "DCD " : "    ");

	/* If CTS status is changed, do H/W output flow control */
	if ((t_cflag & CRTSCTS) && (((mesonuart->mesonuart_msr ^ msr) & CTS) != 0))
		msasync_flowcontrol_hw_output(mesonuart, msr & CTS ? FLOW_START : FLOW_STOP);
	/*
	 * Reading MSR resets the interrupt, we save the
	 * value of msr so that other functions could examine MSR by
	 * looking at mesonuart_msr.
	 */
	mesonuart->mesonuart_msr = (uchar_t)msr;

	/* Handle PPS event */
	if (mesonuart->mesonuart_flags & MESONUART_PPS)
		mesonuart_ppsevent(mesonuart, msr);

	msasync->msasync_ext++;
	MESONUARTSETSOFT(mesonuart);
	/*
	 * We will make sure that the modem status presented to us
	 * during the previous read has not changed. If the chip samples
	 * the modem status on the falling edge of the interrupt line,
	 * and uses this state as the base for detecting change of modem
	 * status, we would miss a change of modem status event that occured
	 * after we initiated a read MSR operation.
	 */
	msr = meson_get_msr(mesonuart);
	if (STATES(msr) != STATES(mesonuart->mesonuart_msr))
		goto	msasync_msint_retry;
}

/*
 * Handle a second-stage interrupt.
 */
/*ARGSUSED*/
uint_t
mesonuartsoftintr(caddr_t intarg)
{
	struct mesonuartcom *mesonuart = (struct mesonuartcom *)intarg;
	struct msasyncline *msasync;
	int rv;
	uint_t cc;

	/*
	 * Test and clear soft interrupt.
	 */
	mutex_enter(&mesonuart->mesonuart_soft_lock);
	DEBUGCONT0(MESONUART_DEBUG_PROCS, "mesonuartsoftintr: enter\n");
	rv = mesonuart->mesonuartsoftpend;
	if (rv != 0)
		mesonuart->mesonuartsoftpend = 0;
	mutex_exit(&mesonuart->mesonuart_soft_lock);

	if (rv) {
		if (mesonuart->mesonuart_priv == NULL)
			return (rv ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED);
		msasync = (struct msasyncline *)mesonuart->mesonuart_priv;
		mutex_enter(&mesonuart->mesonuart_excl_hi);
		if (mesonuart->mesonuart_flags & MESONUART_NEEDSOFT) {
			mesonuart->mesonuart_flags &= ~MESONUART_NEEDSOFT;
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			msasync_softint(mesonuart);
			mutex_enter(&mesonuart->mesonuart_excl_hi);
		}

		/*
		 * There are some instances where the softintr is not
		 * scheduled and hence not called. It so happens that
		 * causes the last few characters to be stuck in the
		 * ringbuffer. Hence, call the handler once again so
		 * the last few characters are cleared.
		 */
		cc = RING_CNT(msasync);
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		if (cc > 0)
			(void) msasync_softint(mesonuart);
	}
	return (rv ? DDI_INTR_CLAIMED : DDI_INTR_UNCLAIMED);
}

/*
 * Handle a software interrupt.
 */
static void
msasync_softint(struct mesonuartcom *mesonuart)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	uint_t	cc;
	mblk_t	*bp;
	queue_t	*q;
	uchar_t	val;
	uchar_t	c;
	tty_common_t	*tp;
	int nb;
	int instance = UNIT(msasync->msasync_dev);

	DEBUGCONT1(MESONUART_DEBUG_PROCS, "msasync%d_softint\n", instance);
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	if (mesonuart->mesonuart_flags & MESONUART_DOINGSOFT) {
		mesonuart->mesonuart_flags |= MESONUART_DOINGSOFT_RETRY;
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		return;
	}
	mesonuart->mesonuart_flags |= MESONUART_DOINGSOFT;
begin:
	mesonuart->mesonuart_flags &= ~MESONUART_DOINGSOFT_RETRY;
	mutex_exit(&mesonuart->mesonuart_excl_hi);
	mutex_enter(&mesonuart->mesonuart_excl);
	tp = &msasync->msasync_ttycommon;
	q = tp->t_readq;
	if (msasync->msasync_flags & MSASYNC_OUT_FLW_RESUME) {
		if (msasync->msasync_ocnt > 0) {
			mutex_enter(&mesonuart->mesonuart_excl_hi);
			msasync_resume(msasync);
			mutex_exit(&mesonuart->mesonuart_excl_hi);
		} else {
			if (msasync->msasync_xmitblk)
				freeb(msasync->msasync_xmitblk);
			msasync->msasync_xmitblk = NULL;
			msasync_start(msasync);
		}
		msasync->msasync_flags &= ~MSASYNC_OUT_FLW_RESUME;
	}

	/*
	 * If data has been added to the circular buffer, remove
	 * it from the buffer, and send it up the stream if there's
	 * somebody listening. Try to do it 16 bytes at a time. If we
	 * have more than 16 bytes to move, move 16 byte chunks and
	 * leave the rest for next time around (maybe it will grow).
	 */
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	if (!(msasync->msasync_flags & MSASYNC_ISOPEN)) {
		RING_INIT(msasync);
		goto rv;
	}
	if ((cc = RING_CNT(msasync)) == 0)
		goto rv;
	mutex_exit(&mesonuart->mesonuart_excl_hi);

	if (!canput(q)) {
		mutex_enter(&mesonuart->mesonuart_excl_hi);
		if (!(msasync->msasync_inflow_source & IN_FLOW_STREAMS)) {
			msasync_flowcontrol_hw_input(mesonuart, FLOW_STOP, IN_FLOW_STREAMS);
			msasync_flowcontrol_sw_input(mesonuart, FLOW_STOP, IN_FLOW_STREAMS);
		}
		goto rv;
	}
	if (msasync->msasync_inflow_source & IN_FLOW_STREAMS) {
		mutex_enter(&mesonuart->mesonuart_excl_hi);
		msasync_flowcontrol_hw_input(mesonuart, FLOW_START, IN_FLOW_STREAMS);
		msasync_flowcontrol_sw_input(mesonuart, FLOW_START, IN_FLOW_STREAMS);
		mutex_exit(&mesonuart->mesonuart_excl_hi);
	}

	DEBUGCONT2(MESONUART_DEBUG_INPUT, "msasync%d_softint: %d char(s) in queue.\n", instance, cc);

	if (!(bp = allocb(cc, BPRI_MED))) {
		mutex_exit(&mesonuart->mesonuart_excl);
		ttycommon_qfull(&msasync->msasync_ttycommon, q);
		mutex_enter(&mesonuart->mesonuart_excl);
		mutex_enter(&mesonuart->mesonuart_excl_hi);
		goto rv;
	}
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	do {
		if (RING_ERR(msasync, S_ERRORS)) {
			RING_UNMARK(msasync);
			c = RING_GET(msasync);
			break;
		} else
			*bp->b_wptr++ = RING_GET(msasync);
	} while (--cc);
	mutex_exit(&mesonuart->mesonuart_excl_hi);
	mutex_exit(&mesonuart->mesonuart_excl);
	if (bp->b_wptr > bp->b_rptr) {
			if (!canput(q)) {
				mesonuarterror(CE_NOTE, "mesonuart%d: local queue full",
				    instance);
				freemsg(bp);
			} else
				(void) putq(q, bp);
	} else
		freemsg(bp);
	/*
	 * If we have a parity error, then send
	 * up an M_BREAK with the "bad"
	 * character as an argument. Let ldterm
	 * figure out what to do with the error.
	 */
	if (cc) {
		(void) putctl1(q, M_BREAK, c);
		MESONUARTSETSOFT(msasync->msasync_common);	/* finish cc chars */
	}
	mutex_enter(&mesonuart->mesonuart_excl);
	mutex_enter(&mesonuart->mesonuart_excl_hi);
rv:
	if ((RING_CNT(msasync) < (RINGSIZE/4)) && (msasync->msasync_inflow_source & IN_FLOW_RINGBUFF)) {
		msasync_flowcontrol_hw_input(mesonuart, FLOW_START, IN_FLOW_RINGBUFF);
		msasync_flowcontrol_sw_input(mesonuart, FLOW_START, IN_FLOW_RINGBUFF);
	}

	/*
	 * If a transmission has finished, indicate that it's finished,
	 * and start that line up again.
	 */
	if (msasync->msasync_break > 0) {
		nb = msasync->msasync_break;
		msasync->msasync_break = 0;
		if (msasync->msasync_flags & MSASYNC_ISOPEN) {
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			mutex_exit(&mesonuart->mesonuart_excl);
			for (; nb > 0; nb--)
				(void) putctl(q, M_BREAK);
			mutex_enter(&mesonuart->mesonuart_excl);
			mutex_enter(&mesonuart->mesonuart_excl_hi);
		}
	}
	if (msasync->msasync_ocnt <= 0 && (msasync->msasync_flags & MSASYNC_BUSY)) {
		DEBUGCONT2(MESONUART_DEBUG_BUSY,
		    "msasync%d_softint: Clearing MSASYNC_BUSY.  msasync_ocnt=%d\n",
		    instance,
		    msasync->msasync_ocnt);
		msasync->msasync_flags &= ~MSASYNC_BUSY;
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		if (msasync->msasync_xmitblk)
			freeb(msasync->msasync_xmitblk);
		msasync->msasync_xmitblk = NULL;
		msasync_start(msasync);
		/*
		 * If the flag isn't set after doing the msasync_start above, we
		 * may have finished all the queued output.  Signal any thread
		 * stuck in close.
		 */
		if (!(msasync->msasync_flags & MSASYNC_BUSY))
			cv_broadcast(&msasync->msasync_flags_cv);
		mutex_enter(&mesonuart->mesonuart_excl_hi);
	}
	/*
	 * A note about these overrun bits: all they do is *tell* someone
	 * about an error- They do not track multiple errors. In fact,
	 * you could consider them latched register bits if you like.
	 * We are only interested in printing the error message once for
	 * any cluster of overrun errrors.
	 */
	if (msasync->msasync_hw_overrun) {
		if (msasync->msasync_flags & MSASYNC_ISOPEN) {
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			mutex_exit(&mesonuart->mesonuart_excl);
			mesonuarterror(CE_NOTE, "mesonuart%d: silo overflow", instance);
			mutex_enter(&mesonuart->mesonuart_excl);
			mutex_enter(&mesonuart->mesonuart_excl_hi);
		}
		msasync->msasync_hw_overrun = 0;
	}
	if (msasync->msasync_sw_overrun) {
		if (msasync->msasync_flags & MSASYNC_ISOPEN) {
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			mutex_exit(&mesonuart->mesonuart_excl);
			mesonuarterror(CE_NOTE, "mesonuart%d: ring buffer overflow",
			    instance);
			mutex_enter(&mesonuart->mesonuart_excl);
			mutex_enter(&mesonuart->mesonuart_excl_hi);
		}
		msasync->msasync_sw_overrun = 0;
	}
	if (mesonuart->mesonuart_flags & MESONUART_DOINGSOFT_RETRY) {
		mutex_exit(&mesonuart->mesonuart_excl);
		goto begin;
	}
	mesonuart->mesonuart_flags &= ~MESONUART_DOINGSOFT;
	mutex_exit(&mesonuart->mesonuart_excl_hi);
	mutex_exit(&mesonuart->mesonuart_excl);
	DEBUGCONT1(MESONUART_DEBUG_PROCS, "msasync%d_softint: done\n", instance);
}

/*
 * Restart output on a line after a delay or break timer expired.
 */
static void
msasync_restart(void *arg)
{
	struct msasyncline *msasync = (struct msasyncline *)arg;
	struct mesonuartcom *mesonuart = msasync->msasync_common;
	uchar_t lcr;

	/*
	 * If break timer expired, turn off the break bit.
	 */
#ifdef DEBUG
	int instance = UNIT(msasync->msasync_dev);

	DEBUGCONT1(MESONUART_DEBUG_PROCS, "msasync%d_restart\n", instance);
#endif
	mutex_enter(&mesonuart->mesonuart_excl);
	/*
	 * If MSASYNC_OUT_SUSPEND is also set, we don't really
	 * clean the HW break, TIOCCBRK is responsible for this.
	 */
	msasync->msasync_flags &= ~(MSASYNC_DELAY|MSASYNC_BREAK);
	cv_broadcast(&msasync->msasync_flags_cv);
	msasync_start(msasync);

	mutex_exit(&mesonuart->mesonuart_excl);
}

static void
msasync_start(struct msasyncline *msasync)
{
	msasync_nstart(msasync, 0);
}

/*
 * Start output on a line, unless it's busy, frozen, or otherwise.
 */
/*ARGSUSED*/
static void
msasync_nstart(struct msasyncline *msasync, int mode)
{
	struct mesonuartcom *mesonuart = msasync->msasync_common;
	int cc;
	queue_t *q;
	mblk_t *bp;
	uchar_t *xmit_addr;
	uchar_t	val;
	boolean_t didsome __unused; /* XXXARM */
	mblk_t *nbp;

#ifdef DEBUG
	int instance = UNIT(msasync->msasync_dev);

	DEBUGCONT1(MESONUART_DEBUG_PROCS, "msasync%d_nstart\n", instance);
#endif

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl));

	/*
	 * If the chip is busy (i.e., we're waiting for a break timeout
	 * to expire, or for the current transmission to finish, or for
	 * output to finish draining from chip), don't grab anything new.
	 */
	if (msasync->msasync_flags & (MSASYNC_BREAK|MSASYNC_BUSY)) {
		DEBUGCONT2((mode? MESONUART_DEBUG_OUT : 0),
		    "msasync%d_nstart: start %s.\n",
		    instance,
		    msasync->msasync_flags & MSASYNC_BREAK ? "break" : "busy");
		return;
	}

	/*
	 * Check only pended sw input flow control.
	 */
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	msasync_flowcontrol_sw_input(mesonuart, FLOW_CHECK, IN_FLOW_NULL);
	mutex_exit(&mesonuart->mesonuart_excl_hi);

	/*
	 * If we're waiting for a delay timeout to expire, don't grab
	 * anything new.
	 */
	if (msasync->msasync_flags & MSASYNC_DELAY) {
		DEBUGCONT1((mode? MESONUART_DEBUG_OUT : 0),
		    "msasync%d_nstart: start MSASYNC_DELAY.\n", instance);
		return;
	}

	if ((q = msasync->msasync_ttycommon.t_writeq) == NULL) {
		DEBUGCONT1((mode? MESONUART_DEBUG_OUT : 0),
		    "msasync%d_nstart: start writeq is null.\n", instance);
		return;	/* not attached to a stream */
	}

	for (;;) {
		if ((bp = getq(q)) == NULL) {
			return;	/* no data to transmit */
		}

		/*
		 * We have a message block to work on.
		 * Check whether it's a break, a delay, or an ioctl (the latter
		 * occurs if the ioctl in question was waiting for the output
		 * to drain).  If it's one of those, process it immediately.
		 */
		switch (bp->b_datap->db_type) {

		case M_BREAK:
			/*
			 * Set the break bit, and arrange for "msasync_restart"
			 * to be called in 1/4 second; it will turn the
			 * break bit off, and call "msasync_start" to grab
			 * the next message.
			 */
			msasync->msasync_flags |= MSASYNC_BREAK;
			(void) timeout(msasync_restart, (caddr_t)msasync,
			    drv_usectohz(1000000)/4);
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_DELAY:
			/*
			 * Arrange for "msasync_restart" to be called when the
			 * delay expires; it will turn MSASYNC_DELAY off,
			 * and call "msasync_start" to grab the next message.
			 */
			(void) timeout(msasync_restart, (caddr_t)msasync,
			    (int)(*(unsigned char *)bp->b_rptr + 6));
			msasync->msasync_flags |= MSASYNC_DELAY;
			freemsg(bp);
			return;	/* wait for this to finish */

		case M_IOCTL:
			/*
			 * This ioctl was waiting for the output ahead of
			 * it to drain; obviously, it has.  Do it, and
			 * then grab the next message after it.
			 */
			mutex_exit(&mesonuart->mesonuart_excl);
			msasync_ioctl(msasync, q, bp);
			mutex_enter(&mesonuart->mesonuart_excl);
			continue;
		}

		while (bp != NULL && ((cc = MBLKL(bp)) == 0)) {
			nbp = bp->b_cont;
			freeb(bp);
			bp = nbp;
		}
		if (bp != NULL)
			break;
	}

	/*
	 * We have data to transmit.  If output is stopped, put
	 * it back and try again later.
	 */
	if (msasync->msasync_flags & (MSASYNC_HW_OUT_FLW | MSASYNC_SW_OUT_FLW |
	    MSASYNC_STOPPED | MSASYNC_OUT_SUSPEND)) {
		(void) putbq(q, bp);
		return;
	}

	msasync->msasync_xmitblk = bp;
	xmit_addr = bp->b_rptr;
	bp = bp->b_cont;
	if (bp != NULL)
		(void) putbq(q, bp);	/* not done with this message yet */

	/*
	 * In 5-bit mode, the high order bits are used
	 * to indicate character sizes less than five,
	 * so we need to explicitly mask before transmitting
	 */
	if ((msasync->msasync_ttycommon.t_cflag & CSIZE) == CS5) {
		unsigned char *p = xmit_addr;
		int cnt = cc;

		while (cnt--)
			*p++ &= (unsigned char) 0x1f;
	}

	/*
	 * Set up this block for pseudo-DMA.
	 */
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	/*
	 * If the transmitter is ready, shove the first
	 * character out.
	 */
	didsome = B_FALSE;
	int x = 0;
	while (cc > 0) {
		if (!meson_tx_is_ready(mesonuart))
			break;
		meson_put_char(mesonuart, *xmit_addr++);
		cc--;
		didsome = B_TRUE;
		if (++x >= mesonuart_max_tx_fifo)
			break;
	}
	msasync->msasync_optr = xmit_addr;
	msasync->msasync_ocnt = cc;
	msasync->msasync_flags |= MSASYNC_PROGRESS;
	DEBUGCONT2(MESONUART_DEBUG_BUSY,
	    "msasync%d_nstart: Set MSASYNC_BUSY.  msasync_ocnt=%d\n",
	    instance, msasync->msasync_ocnt);
	msasync->msasync_flags |= MSASYNC_BUSY;
	if (cc == 0)
		MESONUARTSETSOFT(mesonuart);

	mutex_exit(&mesonuart->mesonuart_excl_hi);
}

/*
 * Resume output by poking the transmitter.
 */
static void
msasync_resume(struct msasyncline *msasync)
{
	struct mesonuartcom *mesonuart = msasync->msasync_common;
#ifdef DEBUG
	int instance;
#endif

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl_hi));
#ifdef DEBUG
	instance = UNIT(msasync->msasync_dev);
	DEBUGCONT1(MESONUART_DEBUG_PROCS, "msasync%d_resume\n", instance);
#endif

	if (meson_tx_is_ready(mesonuart)) {
		if (msasync_flowcontrol_sw_input(mesonuart, FLOW_CHECK, IN_FLOW_NULL))
			return;
		if (msasync->msasync_ocnt > 0 &&
		    !(msasync->msasync_flags &
		    (MSASYNC_HW_OUT_FLW|MSASYNC_SW_OUT_FLW|MSASYNC_OUT_SUSPEND))) {
			meson_put_char(mesonuart, *msasync->msasync_optr++);
			msasync->msasync_ocnt--;
			msasync->msasync_flags |= MSASYNC_PROGRESS;
		}
	}
}

/*
 * Hold the untimed break to last the minimum time.
 */
static void
msasync_hold_utbrk(void *arg)
{
	struct msasyncline *msasync = arg;
	struct mesonuartcom *mesonuart = msasync->msasync_common;

	mutex_enter(&mesonuart->mesonuart_excl);
	msasync->msasync_flags &= ~MSASYNC_HOLD_UTBRK;
	cv_broadcast(&msasync->msasync_flags_cv);
	msasync->msasync_utbrktid = 0;
	mutex_exit(&mesonuart->mesonuart_excl);
}

/*
 * Resume the untimed break.
 */
static void
msasync_resume_utbrk(struct msasyncline *msasync)
{
	uchar_t	val;
	struct mesonuartcom *mesonuart = msasync->msasync_common;
	ASSERT(mutex_owned(&mesonuart->mesonuart_excl));

	/*
	 * Because the wait time is very short,
	 * so we use uninterruptably wait.
	 */
	while (msasync->msasync_flags & MSASYNC_HOLD_UTBRK) {
		cv_wait(&msasync->msasync_flags_cv, &mesonuart->mesonuart_excl);
	}
	mutex_enter(&mesonuart->mesonuart_excl_hi);
	/*
	 * Timed break and untimed break can exist simultaneously,
	 * if MSASYNC_BREAK is also set at here, we don't
	 * really clean the HW break.
	 */
	msasync->msasync_flags &= ~MSASYNC_OUT_SUSPEND;
	cv_broadcast(&msasync->msasync_flags_cv);
	if (msasync->msasync_ocnt > 0) {
		msasync_resume(msasync);
		mutex_exit(&mesonuart->mesonuart_excl_hi);
	} else {
		msasync->msasync_flags &= ~MSASYNC_BUSY;
		mutex_exit(&mesonuart->mesonuart_excl_hi);
		if (msasync->msasync_xmitblk != NULL) {
			freeb(msasync->msasync_xmitblk);
			msasync->msasync_xmitblk = NULL;
		}
		msasync_start(msasync);
	}
}

/*
 * Process an "ioctl" message sent down to us.
 * Note that we don't need to get any locks until we are ready to access
 * the hardware.  Nothing we access until then is going to be altered
 * outside of the STREAMS framework, so we should be safe.
 */
int mesonuartdelay = 10000;
static void
msasync_ioctl(struct msasyncline *msasync, queue_t *wq, mblk_t *mp)
{
	struct mesonuartcom *mesonuart = msasync->msasync_common;
	tty_common_t  *tp = &msasync->msasync_ttycommon;
	struct iocblk *iocp;
	unsigned datasize;
	int error = 0;
	uchar_t val;
	mblk_t *datamp;
	unsigned int index;

#ifdef DEBUG
	int instance = UNIT(msasync->msasync_dev);

	DEBUGCONT1(MESONUART_DEBUG_PROCS, "msasync%d_ioctl\n", instance);
#endif

	if (tp->t_iocpending != NULL) {
		/*
		 * We were holding an "ioctl" response pending the
		 * availability of an "mblk" to hold data to be passed up;
		 * another "ioctl" came through, which means that "ioctl"
		 * must have timed out or been aborted.
		 */
		freemsg(msasync->msasync_ttycommon.t_iocpending);
		msasync->msasync_ttycommon.t_iocpending = NULL;
	}

	iocp = (struct iocblk *)mp->b_rptr;

	/*
	 * For TIOCMGET and the PPS ioctls, do NOT call ttycommon_ioctl()
	 * because this function frees up the message block (mp->b_cont) that
	 * contains the user location where we pass back the results.
	 *
	 * Similarly, CONSOPENPOLLEDIO needs ioc_count, which ttycommon_ioctl
	 * zaps.  We know that ttycommon_ioctl doesn't know any CONS*
	 * ioctls, so keep the others safe too.
	 */
	DEBUGCONT2(MESONUART_DEBUG_IOCTL, "msasync%d_ioctl: %s\n",
	    instance,
	    iocp->ioc_cmd == TIOCMGET ? "TIOCMGET" :
	    iocp->ioc_cmd == TIOCMSET ? "TIOCMSET" :
	    iocp->ioc_cmd == TIOCMBIS ? "TIOCMBIS" :
	    iocp->ioc_cmd == TIOCMBIC ? "TIOCMBIC" :
	    "other");

	switch (iocp->ioc_cmd) {
	case TIOCMGET:
	case TIOCGPPS:
	case TIOCSPPS:
	case TIOCGPPSEV:
	case CONSOPENPOLLEDIO:
	case CONSCLOSEPOLLEDIO:
	case CONSSETABORTENABLE:
	case CONSGETABORTENABLE:
		error = -1; /* Do Nothing */
		break;
	default:

		/*
		 * The only way in which "ttycommon_ioctl" can fail is if the
		 * "ioctl" requires a response containing data to be returned
		 * to the user, and no mblk could be allocated for the data.
		 * No such "ioctl" alters our state.  Thus, we always go ahead
		 * and do any state-changes the "ioctl" calls for.  If we
		 * couldn't allocate the data, "ttycommon_ioctl" has stashed
		 * the "ioctl" away safely, so we just call "bufcall" to
		 * request that we be called back when we stand a better
		 * chance of allocating the data.
		 */
		if ((datasize = ttycommon_ioctl(tp, wq, mp, &error)) != 0) {
			if (msasync->msasync_wbufcid)
				unbufcall(msasync->msasync_wbufcid);
			msasync->msasync_wbufcid = bufcall(datasize, BPRI_HI,
			    (void (*)(void *)) msasync_reioctl,
			    (void *)(intptr_t)msasync->msasync_common->mesonuart_unit);
			return;
		}
	}

	mutex_enter(&mesonuart->mesonuart_excl);

	if (error == 0) {
		/*
		 * "ttycommon_ioctl" did most of the work; we just use the
		 * data it set up.
		 */
		switch (iocp->ioc_cmd) {

		case TCSETS:
			mutex_enter(&mesonuart->mesonuart_excl_hi);
			if (mesonuart_baudok(mesonuart))
				mesonuart_program(mesonuart, MESONUART_NOINIT);
			else
				error = EINVAL;
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			break;
		case TCSETSF:
		case TCSETSW:
		case TCSETA:
		case TCSETAW:
		case TCSETAF:
			mutex_enter(&mesonuart->mesonuart_excl_hi);
			if (!mesonuart_baudok(mesonuart))
				error = EINVAL;
			else {
				if (mesonuart_isbusy(mesonuart))
					mesonuart_waiteot(mesonuart);
				mesonuart_program(mesonuart, MESONUART_NOINIT);
			}
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			break;
		}
	} else if (error < 0) {
		/*
		 * "ttycommon_ioctl" didn't do anything; we process it here.
		 */
		error = 0;
		switch (iocp->ioc_cmd) {

		case TIOCGPPS:
			/*
			 * Get PPS on/off.
			 */
			if (mp->b_cont != NULL)
				freemsg(mp->b_cont);

			mp->b_cont = allocb(sizeof (int), BPRI_HI);
			if (mp->b_cont == NULL) {
				error = ENOMEM;
				break;
			}
			if (mesonuart->mesonuart_flags & MESONUART_PPS)
				*(int *)mp->b_cont->b_wptr = 1;
			else
				*(int *)mp->b_cont->b_wptr = 0;
			mp->b_cont->b_wptr += sizeof (int);
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = sizeof (int);
			break;

		case TIOCSPPS:
			/*
			 * Set PPS on/off.
			 */
			error = miocpullup(mp, sizeof (int));
			if (error != 0)
				break;

			mutex_enter(&mesonuart->mesonuart_excl_hi);
			if (*(int *)mp->b_cont->b_rptr)
				mesonuart->mesonuart_flags |= MESONUART_PPS;
			else
				mesonuart->mesonuart_flags &= ~MESONUART_PPS;
			/* Reset edge sense */
			mesonuart->mesonuart_flags &= ~MESONUART_PPS_EDGE;
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			mp->b_datap->db_type = M_IOCACK;
			break;

		case TIOCGPPSEV:
		{
			/*
			 * Get PPS event data.
			 */
			mblk_t *bp;
			void *buf;
			struct ppsclockev ppsclockev;

			if (mp->b_cont != NULL) {
				freemsg(mp->b_cont);
				mp->b_cont = NULL;
			}

			if ((mesonuart->mesonuart_flags & MESONUART_PPS) == 0) {
				error = ENXIO;
				break;
			}

			/* Protect from incomplete mesonuart_ppsev */
			mutex_enter(&mesonuart->mesonuart_excl_hi);
			ppsclockev = mesonuart_ppsev;
			mutex_exit(&mesonuart->mesonuart_excl_hi);

			buf = &ppsclockev;
			iocp->ioc_count = sizeof (struct ppsclockev);

			if ((bp = allocb(iocp->ioc_count, BPRI_HI)) == NULL) {
				error = ENOMEM;
				break;
			}
			mp->b_cont = bp;

			bcopy(buf, bp->b_wptr, iocp->ioc_count);
			bp->b_wptr += iocp->ioc_count;
			mp->b_datap->db_type = M_IOCACK;
			break;
		}

		case TCSBRK:
			error = miocpullup(mp, sizeof (int));
			if (error != 0)
				break;

			if (*(int *)mp->b_cont->b_rptr == 0) {

				/*
				 * XXX Arrangements to ensure that a break
				 * isn't in progress should be sufficient.
				 * This ugly delay() is the only thing
				 * that seems to work on the NCR Worldmark.
				 * It should be replaced. Note that an
				 * mesonuart_waiteot() also does not work.
				 */
				if (mesonuartdelay)
					delay(drv_usectohz(mesonuartdelay));

				while (msasync->msasync_flags & MSASYNC_BREAK) {
					cv_wait(&msasync->msasync_flags_cv,
					    &mesonuart->mesonuart_excl);
				}
				mutex_enter(&mesonuart->mesonuart_excl_hi);
				/*
				 * We loop until the TSR is empty and then
				 * set the break.  MSASYNC_BREAK has been set
				 * to ensure that no characters are
				 * transmitted while the TSR is being
				 * flushed and SOUT is being used for the
				 * break signal.
				 *
				 * The wait period is equal to
				 * clock / (baud * 16) * 16 * 2.
				 */
				index = BAUDINDEX(
				    msasync->msasync_ttycommon.t_cflag);
				msasync->msasync_flags |= MSASYNC_BREAK;

				while (meson_is_busy(mesonuart)) {
					mutex_exit(&mesonuart->mesonuart_excl_hi);
					mutex_exit(&mesonuart->mesonuart_excl);
					drv_usecwait(mesonuart->mesonuart_clock / baudtable[index] * 2);
					mutex_enter(&mesonuart->mesonuart_excl);
					mutex_enter(&mesonuart->mesonuart_excl_hi);
				}
				/*
				 * Arrange for "msasync_restart"
				 * to be called in 1/4 second;
				 * it will turn the break bit off, and call
				 * "msasync_start" to grab the next message.
				 */
				mutex_exit(&mesonuart->mesonuart_excl_hi);
				(void) timeout(msasync_restart, (caddr_t)msasync,
				    drv_usectohz(1000000)/4);
			} else {
				DEBUGCONT1(MESONUART_DEBUG_OUT,
				    "msasync%d_ioctl: wait for flush.\n",
				    instance);
				mutex_enter(&mesonuart->mesonuart_excl_hi);
				mesonuart_waiteot(mesonuart);
				mutex_exit(&mesonuart->mesonuart_excl_hi);
				DEBUGCONT1(MESONUART_DEBUG_OUT,
				    "msasync%d_ioctl: ldterm satisfied.\n",
				    instance);
			}
			break;

		case TIOCSBRK:
			if (!(msasync->msasync_flags & MSASYNC_OUT_SUSPEND)) {
				mutex_enter(&mesonuart->mesonuart_excl_hi);
				msasync->msasync_flags |= MSASYNC_OUT_SUSPEND;
				msasync->msasync_flags |= MSASYNC_HOLD_UTBRK;
				index = BAUDINDEX(
				    msasync->msasync_ttycommon.t_cflag);
				while (meson_is_busy(mesonuart)) {
					mutex_exit(&mesonuart->mesonuart_excl_hi);
					mutex_exit(&mesonuart->mesonuart_excl);
					drv_usecwait(mesonuart->mesonuart_clock / baudtable[index] * 2);
					mutex_enter(&mesonuart->mesonuart_excl);
					mutex_enter(&mesonuart->mesonuart_excl_hi);
				}
				mutex_exit(&mesonuart->mesonuart_excl_hi);
				/* wait for 100ms to hold BREAK */
				msasync->msasync_utbrktid =
				    timeout((void (*)())msasync_hold_utbrk,
				    (caddr_t)msasync,
				    drv_usectohz(mesonuart_min_utbrk));
			}
			mioc2ack(mp, NULL, 0, 0);
			break;

		case TIOCCBRK:
			if (msasync->msasync_flags & MSASYNC_OUT_SUSPEND)
				msasync_resume_utbrk(msasync);
			mioc2ack(mp, NULL, 0, 0);
			break;

		case CONSOPENPOLLEDIO:
			error = miocpullup(mp, sizeof (struct cons_polledio *));
			if (error != 0)
				break;

			*(struct cons_polledio **)mp->b_cont->b_rptr =
			    &mesonuart->polledio;

			mp->b_datap->db_type = M_IOCACK;
			break;

		case CONSCLOSEPOLLEDIO:
			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;
			break;

		case CONSSETABORTENABLE:
			error = secpolicy_console(iocp->ioc_cr);
			if (error != 0)
				break;

			if (iocp->ioc_count != TRANSPARENT) {
				error = EINVAL;
				break;
			}

			if (*(intptr_t *)mp->b_cont->b_rptr)
				mesonuart->mesonuart_flags |= MESONUART_CONSOLE;
			else
				mesonuart->mesonuart_flags &= ~MESONUART_CONSOLE;

			mp->b_datap->db_type = M_IOCACK;
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;
			break;

		case CONSGETABORTENABLE:
			/*CONSTANTCONDITION*/
			ASSERT(sizeof (boolean_t) <= sizeof (boolean_t *));
			/*
			 * Store the return value right in the payload
			 * we were passed.  Crude.
			 */
			mcopyout(mp, NULL, sizeof (boolean_t), NULL, NULL);
			*(boolean_t *)mp->b_cont->b_rptr =
			    (mesonuart->mesonuart_flags & MESONUART_CONSOLE) != 0;
			break;

		default:
			/*
			 * If we don't understand it, it's an error.  NAK it.
			 */
			error = EINVAL;
			break;
		}
	}
	if (error != 0) {
		iocp->ioc_error = error;
		mp->b_datap->db_type = M_IOCNAK;
	}
	mutex_exit(&mesonuart->mesonuart_excl);
	qreply(wq, mp);
	DEBUGCONT1(MESONUART_DEBUG_PROCS, "msasync%d_ioctl: done\n", instance);
}

static int
mesonuartrsrv(queue_t *q)
{
	mblk_t *bp;
	struct msasyncline *msasync;

	msasync = (struct msasyncline *)q->q_ptr;

	while (canputnext(q) && (bp = getq(q)))
		putnext(q, bp);
	MESONUARTSETSOFT(msasync->msasync_common);
	msasync->msasync_polltid = 0;
	return (0);
}

/*
 * The MESONUARTWPUTDO_NOT_SUSP macro indicates to mesonuartwputdo() whether it should
 * handle messages as though the driver is operating normally or is
 * suspended.  In the suspended case, some or all of the processing may have
 * to be delayed until the driver is resumed.
 */
#define	MESONUARTWPUTDO_NOT_SUSP(msasync, wput) \
	!((wput) && ((msasync)->msasync_flags & MSASYNC_DDI_SUSPENDED))

/*
 * Processing for write queue put procedure.
 * Respond to M_STOP, M_START, M_IOCTL, and M_FLUSH messages here;
 * set the flow control character for M_STOPI and M_STARTI messages;
 * queue up M_BREAK, M_DELAY, and M_DATA messages for processing
 * by the start routine, and then call the start routine; discard
 * everything else.  Note that this driver does not incorporate any
 * mechanism to negotiate to handle the canonicalization process.
 * It expects that these functions are handled in upper module(s),
 * as we do in ldterm.
 */
static int
mesonuartwputdo(queue_t *q, mblk_t *mp, boolean_t wput)
{
	struct msasyncline *msasync;
	struct mesonuartcom *mesonuart;
#ifdef DEBUG
	int instance;
#endif
	int error;

	msasync = (struct msasyncline *)q->q_ptr;

#ifdef DEBUG
	instance = UNIT(msasync->msasync_dev);
#endif
	mesonuart = msasync->msasync_common;

	switch (mp->b_datap->db_type) {

	case M_STOP:
		/*
		 * Since we don't do real DMA, we can just let the
		 * chip coast to a stop after applying the brakes.
		 */
		mutex_enter(&mesonuart->mesonuart_excl);
		msasync->msasync_flags |= MSASYNC_STOPPED;
		mutex_exit(&mesonuart->mesonuart_excl);
		freemsg(mp);
		break;

	case M_START:
		mutex_enter(&mesonuart->mesonuart_excl);
		if (msasync->msasync_flags & MSASYNC_STOPPED) {
			msasync->msasync_flags &= ~MSASYNC_STOPPED;
			if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
				/*
				 * If an output operation is in progress,
				 * resume it.  Otherwise, prod the start
				 * routine.
				 */
				if (msasync->msasync_ocnt > 0) {
					mutex_enter(&mesonuart->mesonuart_excl_hi);
					msasync_resume(msasync);
					mutex_exit(&mesonuart->mesonuart_excl_hi);
				} else {
					msasync_start(msasync);
				}
			}
		}
		mutex_exit(&mesonuart->mesonuart_excl);
		freemsg(mp);
		break;

	case M_IOCTL:
		switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {

		case TCSBRK:
			error = miocpullup(mp, sizeof (int));
			if (error != 0) {
				miocnak(q, mp, 0, error);
				return (0);
			}

			if (*(int *)mp->b_cont->b_rptr != 0) {
				DEBUGCONT1(MESONUART_DEBUG_OUT,
				    "msasync%d_ioctl: flush request.\n",
				    instance);
				(void) putq(q, mp);

				mutex_enter(&mesonuart->mesonuart_excl);
				if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
					/*
					 * If an TIOCSBRK is in progress,
					 * clean it as TIOCCBRK does,
					 * then kick off output.
					 * If TIOCSBRK is not in progress,
					 * just kick off output.
					 */
					msasync_resume_utbrk(msasync);
				}
				mutex_exit(&mesonuart->mesonuart_excl);
				break;
			}
			/*FALLTHROUGH*/
		case TCSETSW:
		case TCSETSF:
		case TCSETAW:
		case TCSETAF:
			/*
			 * The changes do not take effect until all
			 * output queued before them is drained.
			 * Put this message on the queue, so that
			 * "msasync_start" will see it when it's done
			 * with the output before it.  Poke the
			 * start routine, just in case.
			 */
			(void) putq(q, mp);

			mutex_enter(&mesonuart->mesonuart_excl);
			if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
				/*
				 * If an TIOCSBRK is in progress,
				 * clean it as TIOCCBRK does.
				 * then kick off output.
				 * If TIOCSBRK is not in progress,
				 * just kick off output.
				 */
				msasync_resume_utbrk(msasync);
			}
			mutex_exit(&mesonuart->mesonuart_excl);
			break;

		default:
			/*
			 * Do it now.
			 */
			mutex_enter(&mesonuart->mesonuart_excl);
			if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
				mutex_exit(&mesonuart->mesonuart_excl);
				msasync_ioctl(msasync, q, mp);
				break;
			}
			msasync_put_suspq(mesonuart, mp);
			mutex_exit(&mesonuart->mesonuart_excl);
			break;
		}
		break;

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW) {
			mutex_enter(&mesonuart->mesonuart_excl);

			/*
			 * Abort any output in progress.
			 */
			mutex_enter(&mesonuart->mesonuart_excl_hi);
			if (msasync->msasync_flags & MSASYNC_BUSY) {
				DEBUGCONT1(MESONUART_DEBUG_BUSY, "mesonuart%dwput: "
				    "Clearing msasync_ocnt, "
				    "leaving MSASYNC_BUSY set\n",
				    instance);
				msasync->msasync_ocnt = 0;
				msasync->msasync_flags &= ~MSASYNC_BUSY;
			} /* if */

			if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
				/* Flush FIFO buffers */
				if (mesonuart->mesonuart_use_fifo == FIFO_ON) {
					meson_reset_fifo(mesonuart, FIFOTXFLSH);
				}
			}
			mutex_exit(&mesonuart->mesonuart_excl_hi);

			/* Flush FIFO buffers */
			if (mesonuart->mesonuart_use_fifo == FIFO_ON) {
				meson_reset_fifo(mesonuart, FIFOTXFLSH);
			}

			/*
			 * Flush our write queue.
			 */
			flushq(q, FLUSHDATA);	/* XXX doesn't flush M_DELAY */
			if (msasync->msasync_xmitblk != NULL) {
				freeb(msasync->msasync_xmitblk);
				msasync->msasync_xmitblk = NULL;
			}
			mutex_exit(&mesonuart->mesonuart_excl);
			*mp->b_rptr &= ~FLUSHW;	/* it has been flushed */
		}
		if (*mp->b_rptr & FLUSHR) {
			if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
				/* Flush FIFO buffers */
				if (mesonuart->mesonuart_use_fifo == FIFO_ON) {
					meson_reset_fifo(mesonuart, FIFORXFLSH);
				}
			}
			flushq(RD(q), FLUSHDATA);
			qreply(q, mp);	/* give the read queues a crack at it */
		} else {
			freemsg(mp);
		}

		/*
		 * We must make sure we process messages that survive the
		 * write-side flush.
		 */
		if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
			mutex_enter(&mesonuart->mesonuart_excl);
			msasync_start(msasync);
			mutex_exit(&mesonuart->mesonuart_excl);
		}
		break;

	case M_BREAK:
	case M_DELAY:
	case M_DATA:
		/*
		 * Queue the message up to be transmitted,
		 * and poke the start routine.
		 */
		(void) putq(q, mp);
		if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
			mutex_enter(&mesonuart->mesonuart_excl);
			msasync_start(msasync);
			mutex_exit(&mesonuart->mesonuart_excl);
		}
		break;

	case M_STOPI:
		mutex_enter(&mesonuart->mesonuart_excl);
		if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
			mutex_enter(&mesonuart->mesonuart_excl_hi);
			if (!(msasync->msasync_inflow_source & IN_FLOW_USER)) {
				msasync_flowcontrol_hw_input(mesonuart, FLOW_STOP,
				    IN_FLOW_USER);
				(void) msasync_flowcontrol_sw_input(mesonuart,
				    FLOW_STOP, IN_FLOW_USER);
			}
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			mutex_exit(&mesonuart->mesonuart_excl);
			freemsg(mp);
			break;
		}
		msasync_put_suspq(mesonuart, mp);
		mutex_exit(&mesonuart->mesonuart_excl);
		break;

	case M_STARTI:
		mutex_enter(&mesonuart->mesonuart_excl);
		if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
			mutex_enter(&mesonuart->mesonuart_excl_hi);
			if (msasync->msasync_inflow_source & IN_FLOW_USER) {
				msasync_flowcontrol_hw_input(mesonuart, FLOW_START,
				    IN_FLOW_USER);
				(void) msasync_flowcontrol_sw_input(mesonuart,
				    FLOW_START, IN_FLOW_USER);
			}
			mutex_exit(&mesonuart->mesonuart_excl_hi);
			mutex_exit(&mesonuart->mesonuart_excl);
			freemsg(mp);
			break;
		}
		msasync_put_suspq(mesonuart, mp);
		mutex_exit(&mesonuart->mesonuart_excl);
		break;

	case M_CTL:
		if (MBLKL(mp) >= sizeof (struct iocblk) &&
		    ((struct iocblk *)mp->b_rptr)->ioc_cmd == MC_POSIXQUERY) {
			mutex_enter(&mesonuart->mesonuart_excl);
			if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
				((struct iocblk *)mp->b_rptr)->ioc_cmd =
				    MC_HAS_POSIX;
				mutex_exit(&mesonuart->mesonuart_excl);
				qreply(q, mp);
				break;
			} else {
				msasync_put_suspq(mesonuart, mp);
			}
		} else {
			/*
			 * These MC_SERVICE type messages are used by upper
			 * modules to tell this driver to send input up
			 * immediately, or that it can wait for normal
			 * processing that may or may not be done.  Sun
			 * requires these for the mouse module.
			 * (XXX - for x86?)
			 */
			mutex_enter(&mesonuart->mesonuart_excl);
			switch (*mp->b_rptr) {

			case MC_SERVICEIMM:
				msasync->msasync_flags |= MSASYNC_SERVICEIMM;
				break;

			case MC_SERVICEDEF:
				msasync->msasync_flags &= ~MSASYNC_SERVICEIMM;
				break;
			}
			mutex_exit(&mesonuart->mesonuart_excl);
			freemsg(mp);
		}
		break;

	case M_IOCDATA:
		mutex_enter(&mesonuart->mesonuart_excl);
		if (MESONUARTWPUTDO_NOT_SUSP(msasync, wput)) {
			mutex_exit(&mesonuart->mesonuart_excl);
			msasync_iocdata(q, mp);
			break;
		}
		msasync_put_suspq(mesonuart, mp);
		mutex_exit(&mesonuart->mesonuart_excl);
		break;

	default:
		freemsg(mp);
		break;
	}
	return (0);
}

static int
mesonuartwput(queue_t *q, mblk_t *mp)
{
	return (mesonuartwputdo(q, mp, B_TRUE));
}

/*
 * Retry an "ioctl", now that "bufcall" claims we may be able to allocate
 * the buffer we need.
 */
static void
msasync_reioctl(void *unit)
{
	int instance = (uintptr_t)unit;
	struct msasyncline *msasync;
	struct mesonuartcom *mesonuart;
	queue_t	*q;
	mblk_t	*mp;

	mesonuart = ddi_get_soft_state(mesonuart_soft_state, instance);
	ASSERT(mesonuart != NULL);
	msasync = mesonuart->mesonuart_priv;

	/*
	 * The bufcall is no longer pending.
	 */
	mutex_enter(&mesonuart->mesonuart_excl);
	msasync->msasync_wbufcid = 0;
	if ((q = msasync->msasync_ttycommon.t_writeq) == NULL) {
		mutex_exit(&mesonuart->mesonuart_excl);
		return;
	}
	if ((mp = msasync->msasync_ttycommon.t_iocpending) != NULL) {
		/* not pending any more */
		msasync->msasync_ttycommon.t_iocpending = NULL;
		mutex_exit(&mesonuart->mesonuart_excl);
		msasync_ioctl(msasync, q, mp);
	} else
		mutex_exit(&mesonuart->mesonuart_excl);
}

static void
msasync_iocdata(queue_t *q, mblk_t *mp)
{
	struct msasyncline	*msasync = (struct msasyncline *)q->q_ptr;
	struct mesonuartcom		*mesonuart;
	struct iocblk *ip;
	struct copyresp *csp;
#ifdef DEBUG
	int instance = UNIT(msasync->msasync_dev);
#endif

	mesonuart = msasync->msasync_common;
	ip = (struct iocblk *)mp->b_rptr;
	csp = (struct copyresp *)mp->b_rptr;

	if (csp->cp_rval != 0) {
		if (csp->cp_private)
			freemsg(csp->cp_private);
		freemsg(mp);
		return;
	}

	mutex_enter(&mesonuart->mesonuart_excl);
	DEBUGCONT2(MESONUART_DEBUG_MODEM, "msasync%d_iocdata: case %s\n",
	    instance,
	    csp->cp_cmd == TIOCMGET ? "TIOCMGET" :
	    csp->cp_cmd == TIOCMSET ? "TIOCMSET" :
	    csp->cp_cmd == TIOCMBIS ? "TIOCMBIS" :
	    "TIOCMBIC");
	switch (csp->cp_cmd) {

	case TIOCMGET:
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
		}
		mp->b_datap->db_type = M_IOCACK;
		ip->ioc_error = 0;
		ip->ioc_count = 0;
		ip->ioc_rval = 0;
		mp->b_wptr = mp->b_rptr + sizeof (struct iocblk);
		break;

	default:
		mp->b_datap->db_type = M_IOCNAK;
		ip->ioc_error = EINVAL;
		break;
	}
	qreply(q, mp);
	mutex_exit(&mesonuart->mesonuart_excl);
}

/*
 * debugger/console support routines.
 */

/*
 * put a character out
 * Do not use interrupts.  If char is LF, put out CR, LF.
 */
static void
mesonuartputchar(cons_polledio_arg_t arg, uchar_t c)
{
	struct mesonuartcom *mesonuart = (struct mesonuartcom *)arg;

	if (c == '\n')
		mesonuartputchar(arg, '\r');

	while (!meson_tx_is_ready(mesonuart)) {
		/* wait for xmit to finish */
		drv_usecwait(10);
	}

	/* put the character out */
	meson_put_char(mesonuart, c);
}

/*
 * See if there's a character available. If no character is
 * available, return 0. Run in polled mode, no interrupts.
 */
static boolean_t
mesonuartischar(cons_polledio_arg_t arg)
{
	struct mesonuartcom *mesonuart = (struct mesonuartcom *)arg;

	return meson_rx_is_ready(mesonuart);
}

/*
 * Get a character. Run in polled mode, no interrupts.
 */
static int
mesonuartgetchar(cons_polledio_arg_t arg)
{
	struct mesonuartcom *mesonuart = (struct mesonuartcom *)arg;

	while (!mesonuartischar(arg))
		drv_usecwait(10);
	return (meson_get_char(mesonuart));
}

static int
dmtomesonuart(int bits)
{
	int b = 0;

	DEBUGCONT1(MESONUART_DEBUG_MODEM, "dmtomesonuart: bits = %x\n", bits);
#ifdef	CAN_NOT_SET	/* only DTR and RTS can be set */
	if (bits & TIOCM_CAR)
		b |= DCD;
	if (bits & TIOCM_CTS)
		b |= CTS;
	if (bits & TIOCM_DSR)
		b |= DSR;
	if (bits & TIOCM_RNG)
		b |= RI;
#endif

	if (bits & TIOCM_RTS) {
		DEBUGCONT0(MESONUART_DEBUG_MODEM, "dmtomesonuart: set b & RTS\n");
		b |= RTS;
	}
	if (bits & TIOCM_DTR) {
		DEBUGCONT0(MESONUART_DEBUG_MODEM, "dmtomesonuart: set b & DTR\n");
		b |= DTR;
	}

	return (b);
}

static void
mesonuarterror(int level, const char *fmt, ...)
{
	va_list adx;
	static	time_t	last;
	static	const char *lastfmt;
	time_t	now;

	/*
	 * Don't print the same error message too often.
	 * Print the message only if we have not printed the
	 * message within the last second.
	 * Note: that fmt cannot be a pointer to a string
	 * stored on the stack. The fmt pointer
	 * must be in the data segment otherwise lastfmt would point
	 * to non-sense.
	 */
	now = gethrestime_sec();
	if (last == now && lastfmt == fmt)
		return;

	last = now;
	lastfmt = fmt;

	va_start(adx, fmt);
	vcmn_err(level, fmt, adx);
	va_end(adx);
}

/*
 * mesonuart_parse_mode(dev_info_t *devi, struct mesonuartcom *mesonuart)
 * The value of this property is in the form of "9600,8,n,1,-"
 * 1) speed: 9600, 4800, ...
 * 2) data bits
 * 3) parity: n(none), e(even), o(odd)
 * 4) stop bits
 * 5) handshake: -(none), h(hardware: rts/cts), s(software: xon/off)
 *
 * This parsing came from a SPARCstation eeprom.
 */
static void
mesonuart_parse_mode(dev_info_t *devi, struct mesonuartcom *mesonuart)
{
	char		name[40];
	char		val[40];
	int		len;
	int		ret;
	char		*p;
	char		*p1;

	ASSERT(mesonuart->mesonuart_com_port != 0);

	/*
	 * Parse the ttyx-mode property
	 */
	(void) sprintf(name, "tty%c-mode", mesonuart->mesonuart_com_port + 'a' - 1);
	len = sizeof (val);
	ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	if (ret != DDI_PROP_SUCCESS) {
		(void) sprintf(name, "com%c-mode", mesonuart->mesonuart_com_port + '0');
		len = sizeof (val);
		ret = GET_PROP(devi, name, DDI_PROP_CANSLEEP, val, &len);
	}

	/* no property to parse */
	mesonuart->mesonuart_cflag = 0;
	if (ret != DDI_PROP_SUCCESS)
		return;

	p = val;
	/* ---- baud rate ---- */
	mesonuart->mesonuart_cflag = CREAD |
	    (MESONUART_DEFAULT_BAUD & CBAUD) |
	    (MESONUART_DEFAULT_BAUD > CBAUD? CBAUDEXT: 0);		/* initial default */
	if (p && (p1 = strchr(p, ',')) != 0) {
		*p1++ = '\0';
	} else {
		mesonuart->mesonuart_cflag |= BITS8;	/* add default bits */
		return;
	}

	if (strcmp(p, "110") == 0)
		mesonuart->mesonuart_bidx = B110;
	else if (strcmp(p, "150") == 0)
		mesonuart->mesonuart_bidx = B150;
	else if (strcmp(p, "300") == 0)
		mesonuart->mesonuart_bidx = B300;
	else if (strcmp(p, "600") == 0)
		mesonuart->mesonuart_bidx = B600;
	else if (strcmp(p, "1200") == 0)
		mesonuart->mesonuart_bidx = B1200;
	else if (strcmp(p, "2400") == 0)
		mesonuart->mesonuart_bidx = B2400;
	else if (strcmp(p, "4800") == 0)
		mesonuart->mesonuart_bidx = B4800;
	else if (strcmp(p, "9600") == 0)
		mesonuart->mesonuart_bidx = B9600;
	else if (strcmp(p, "19200") == 0)
		mesonuart->mesonuart_bidx = B19200;
	else if (strcmp(p, "38400") == 0)
		mesonuart->mesonuart_bidx = B38400;
	else if (strcmp(p, "57600") == 0)
		mesonuart->mesonuart_bidx = B57600;
	else if (strcmp(p, "115200") == 0)
		mesonuart->mesonuart_bidx = B115200;
	else
		mesonuart->mesonuart_bidx = MESONUART_DEFAULT_BAUD;

	mesonuart->mesonuart_cflag &= ~(CBAUD | CBAUDEXT);
	if (mesonuart->mesonuart_bidx > CBAUD) {	/* > 38400 uses the CBAUDEXT bit */
		mesonuart->mesonuart_cflag |= CBAUDEXT;
		mesonuart->mesonuart_cflag |= mesonuart->mesonuart_bidx - CBAUD - 1;
	} else {
		mesonuart->mesonuart_cflag |= mesonuart->mesonuart_bidx;
	}

	ASSERT(mesonuart->mesonuart_bidx == BAUDINDEX(mesonuart->mesonuart_cflag));

	/* ---- Next item is data bits ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		mesonuart->mesonuart_cflag |= BITS8;	/* add default bits */
		return;
	}
	switch (*p) {
		default:
		case '8':
			mesonuart->mesonuart_cflag |= CS8;
			mesonuart->mesonuart_lcr = BITS8;
			break;
		case '7':
			mesonuart->mesonuart_cflag |= CS7;
			mesonuart->mesonuart_lcr = BITS7;
			break;
		case '6':
			mesonuart->mesonuart_cflag |= CS6;
			mesonuart->mesonuart_lcr = BITS6;
			break;
		case '5':
			/* LINTED: CS5 is currently zero (but might change) */
			mesonuart->mesonuart_cflag |= CS5;
			mesonuart->mesonuart_lcr = BITS5;
			break;
	}

	/* ---- Parity info ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		return;
	}
	switch (*p)  {
		default:
		case 'n':
			break;
		case 'e':
			mesonuart->mesonuart_cflag |= PARENB;
			mesonuart->mesonuart_lcr |= PEN; break;
		case 'o':
			mesonuart->mesonuart_cflag |= PARENB|PARODD;
			mesonuart->mesonuart_lcr |= PEN|EPS;
			break;
	}

	/* ---- Find stop bits ---- */
	p = p1;
	if (p && (p1 = strchr(p, ',')) != 0)  {
		*p1++ = '\0';
	} else {
		return;
	}
	if (*p == '2') {
		mesonuart->mesonuart_cflag |= CSTOPB;
		mesonuart->mesonuart_lcr |= STB;
	}

	/* ---- handshake is next ---- */
	p = p1;
	if (p) {
		if ((p1 = strchr(p, ',')) != 0)
			*p1++ = '\0';

		if (*p == 'h')
			mesonuart->mesonuart_cflag |= CRTSCTS;
		else if (*p == 's')
			mesonuart->mesonuart_cflag |= CRTSXOFF;
	}
}

/*
 * Check for abort character sequence
 */
static boolean_t
abort_charseq_recognize(uchar_t ch)
{
	static int state = 0;
#define	CNTRL(c) ((c)&037)
	static char sequence[] = { '\r', '~', CNTRL('b') };

	if (ch == sequence[state]) {
		if (++state >= sizeof (sequence)) {
			state = 0;
			return (B_TRUE);
		}
	} else {
		state = (ch == sequence[0]) ? 1 : 0;
	}
	return (B_FALSE);
}

/*
 * Flow control functions
 */
/*
 * Software input flow control
 * This function can execute software input flow control sucessfully
 * at most of situations except that the line is in BREAK status
 * (timed and untimed break).
 * INPUT VALUE of onoff:
 *               FLOW_START means to send out a XON char
 *                          and clear SW input flow control flag.
 *               FLOW_STOP means to send out a XOFF char
 *                          and set SW input flow control flag.
 *               FLOW_CHECK means to check whether there is pending XON/XOFF
 *                          if it is true, send it out.
 * INPUT VALUE of type:
 *		 IN_FLOW_RINGBUFF means flow control is due to RING BUFFER
 *		 IN_FLOW_STREAMS means flow control is due to STREAMS
 *		 IN_FLOW_USER means flow control is due to user's commands
 * RETURN VALUE: B_FALSE means no flow control char is sent
 *               B_TRUE means one flow control char is sent
 */
static boolean_t
msasync_flowcontrol_sw_input(struct mesonuartcom *mesonuart, msasync_flowc_action onoff, int type)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	int instance = UNIT(msasync->msasync_dev);
	int rval = B_FALSE;

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl_hi));

	if (!(msasync->msasync_ttycommon.t_iflag & IXOFF))
		return (rval);

	/*
	 * If we get this far, then we know IXOFF is set.
	 */
	switch (onoff) {
	case FLOW_STOP:
		msasync->msasync_inflow_source |= type;

		/*
		 * We'll send an XOFF character for each of up to
		 * three different input flow control attempts to stop input.
		 * If we already send out one XOFF, but FLOW_STOP comes again,
		 * it seems that input flow control becomes more serious,
		 * then send XOFF again.
		 */
		if (msasync->msasync_inflow_source & (IN_FLOW_RINGBUFF |
		    IN_FLOW_STREAMS | IN_FLOW_USER))
			msasync->msasync_flags |= MSASYNC_SW_IN_FLOW |
			    MSASYNC_SW_IN_NEEDED;
		DEBUGCONT2(MESONUART_DEBUG_SFLOW, "msasync%d: input sflow stop, "
		    "type = %x\n", instance, msasync->msasync_inflow_source);
		break;
	case FLOW_START:
		msasync->msasync_inflow_source &= ~type;
		if (msasync->msasync_inflow_source == 0) {
			msasync->msasync_flags = (msasync->msasync_flags &
			    ~MSASYNC_SW_IN_FLOW) | MSASYNC_SW_IN_NEEDED;
			DEBUGCONT1(MESONUART_DEBUG_SFLOW, "msasync%d: "
			    "input sflow start\n", instance);
		}
		break;
	default:
		break;
	}

	if ((msasync->msasync_flags & (MSASYNC_SW_IN_NEEDED | MSASYNC_BREAK | MSASYNC_OUT_SUSPEND)) == MSASYNC_SW_IN_NEEDED) {
		/*
		 * If we get this far, then we know we need to send out
		 * XON or XOFF char.
		 */
		msasync->msasync_flags = (msasync->msasync_flags & ~MSASYNC_SW_IN_NEEDED) | MSASYNC_BUSY;
		while (!meson_tx_is_ready(mesonuart)) {}
		meson_put_char(mesonuart, msasync->msasync_flags & MSASYNC_SW_IN_FLOW ?  msasync->msasync_stopc : msasync->msasync_startc);
		rval = B_TRUE;
	}
	return (rval);
}

/*
 * Software output flow control
 * This function can be executed sucessfully at any situation.
 * It does not handle HW, and just change the SW output flow control flag.
 * INPUT VALUE of onoff:
 *                 FLOW_START means to clear SW output flow control flag,
 *			also combine with HW output flow control status to
 *			determine if we need to set MSASYNC_OUT_FLW_RESUME.
 *                 FLOW_STOP means to set SW output flow control flag,
 *			also clear MSASYNC_OUT_FLW_RESUME.
 */
static void
msasync_flowcontrol_sw_output(struct mesonuartcom *mesonuart, msasync_flowc_action onoff)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	int instance = UNIT(msasync->msasync_dev);

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl_hi));

	if (!(msasync->msasync_ttycommon.t_iflag & IXON))
		return;

	switch (onoff) {
	case FLOW_STOP:
		msasync->msasync_flags |= MSASYNC_SW_OUT_FLW;
		msasync->msasync_flags &= ~MSASYNC_OUT_FLW_RESUME;
		DEBUGCONT1(MESONUART_DEBUG_SFLOW, "msasync%d: output sflow stop\n",
		    instance);
		break;
	case FLOW_START:
		msasync->msasync_flags &= ~MSASYNC_SW_OUT_FLW;
		if (!(msasync->msasync_flags & MSASYNC_HW_OUT_FLW))
			msasync->msasync_flags |= MSASYNC_OUT_FLW_RESUME;
		DEBUGCONT1(MESONUART_DEBUG_SFLOW, "msasync%d: output sflow start\n",
		    instance);
		break;
	default:
		break;
	}
}

/*
 * Hardware input flow control
 * This function can be executed sucessfully at any situation.
 * It directly changes RTS depending on input parameter onoff.
 * INPUT VALUE of onoff:
 *       FLOW_START means to clear HW input flow control flag,
 *                  and pull up RTS if it is low.
 *       FLOW_STOP means to set HW input flow control flag,
 *                  and low RTS if it is high.
 * INPUT VALUE of type:
 *		 IN_FLOW_RINGBUFF means flow control is due to RING BUFFER
 *		 IN_FLOW_STREAMS means flow control is due to STREAMS
 *		 IN_FLOW_USER means flow control is due to user's commands
 */
static void
msasync_flowcontrol_hw_input(struct mesonuartcom *mesonuart, msasync_flowc_action onoff,
    int type)
{
	uchar_t	mcr;
	uchar_t	flag;
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	int instance = UNIT(msasync->msasync_dev);

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl_hi));

	if (!(msasync->msasync_ttycommon.t_cflag & CRTSXOFF))
		return;

	switch (onoff) {
	case FLOW_STOP:
		msasync->msasync_inflow_source |= type;
		if (msasync->msasync_inflow_source & (IN_FLOW_RINGBUFF |
		    IN_FLOW_STREAMS | IN_FLOW_USER))
			msasync->msasync_flags |= MSASYNC_HW_IN_FLOW;
		DEBUGCONT2(MESONUART_DEBUG_HFLOW, "msasync%d: input hflow stop, "
		    "type = %x\n", instance, msasync->msasync_inflow_source);
		break;
	case FLOW_START:
		msasync->msasync_inflow_source &= ~type;
		if (msasync->msasync_inflow_source == 0) {
			msasync->msasync_flags &= ~MSASYNC_HW_IN_FLOW;
			DEBUGCONT1(MESONUART_DEBUG_HFLOW, "msasync%d: "
			    "input hflow start\n", instance);
		}
		break;
	default:
		break;
	}
}

/*
 * Hardware output flow control
 * This function can execute HW output flow control sucessfully
 * at any situation.
 * It doesn't really change RTS, and just change
 * HW output flow control flag depending on CTS status.
 * INPUT VALUE of onoff:
 *                FLOW_START means to clear HW output flow control flag.
 *			also combine with SW output flow control status to
 *			determine if we need to set MSASYNC_OUT_FLW_RESUME.
 *                FLOW_STOP means to set HW output flow control flag.
 *			also clear MSASYNC_OUT_FLW_RESUME.
 */
static void
msasync_flowcontrol_hw_output(struct mesonuartcom *mesonuart, msasync_flowc_action onoff)
{
	struct msasyncline *msasync = mesonuart->mesonuart_priv;
	int instance = UNIT(msasync->msasync_dev);

	ASSERT(mutex_owned(&mesonuart->mesonuart_excl_hi));

	if (!(msasync->msasync_ttycommon.t_cflag & CRTSCTS))
		return;

	switch (onoff) {
	case FLOW_STOP:
		msasync->msasync_flags |= MSASYNC_HW_OUT_FLW;
		msasync->msasync_flags &= ~MSASYNC_OUT_FLW_RESUME;
		DEBUGCONT1(MESONUART_DEBUG_HFLOW, "msasync%d: output hflow stop\n",
		    instance);
		break;
	case FLOW_START:
		msasync->msasync_flags &= ~MSASYNC_HW_OUT_FLW;
		if (!(msasync->msasync_flags & MSASYNC_SW_OUT_FLW))
			msasync->msasync_flags |= MSASYNC_OUT_FLW_RESUME;
		DEBUGCONT1(MESONUART_DEBUG_HFLOW, "msasync%d: output hflow start\n",
		    instance);
		break;
	default:
		break;
	}
}


/*
 * quiesce(9E) entry point.
 *
 * This function is called when the system is single-threaded at high
 * PIL with preemption disabled. Therefore, this function must not be
 * blocked.
 *
 * This function returns DDI_SUCCESS on success, or DDI_FAILURE on failure.
 * DDI_FAILURE indicates an error condition and should almost never happen.
 */
static int
mesonuartquiesce(dev_info_t *devi)
{
	int instance;
	struct mesonuartcom *mesonuart;

	instance = ddi_get_instance(devi);	/* find out which unit */

	mesonuart = ddi_get_soft_state(mesonuart_soft_state, instance);
	if (mesonuart == NULL)
		return (DDI_FAILURE);

	/* disable all interrupts */
	meson_set_icr(mesonuart, 0, RIEN | TIEN);

	/* reset the FIFO */
	meson_reset_fifo(mesonuart, FIFOTXFLSH | FIFORXFLSH);

	return (DDI_SUCCESS);
}
