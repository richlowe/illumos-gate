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

#ifndef _SYS_FFA_H
#define	_SYS_FFA_H

/*
 * Arm Firmware Framework for Arm A-profile (FF-A) per Arm DEN0077A v1.3.
 *
 * FF-A provides a standardised interface for communication between the
 * normal world and secure partitions managed by a Secure Partition Manager
 * Core (SPMC).  This module is a self-probing client that discovers FF-A
 * via SMCCC at boot and exposes a call interface for kernel consumers.
 *
 * Specification references are against DEN0077A v1.3.
 */

#include <sys/types.h>
#include <sys/uuid.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * FF-A version encoding (§13.2).
 *
 * Major in bits [30:16], minor in bits [15:0].  Bit 31 is reserved (MBZ).
 */
#define	FFA_VERSION_MAJOR(v)		(((v) >> 16) & 0x7FFF)
#define	FFA_VERSION_MINOR(v)		((v) & 0xFFFF)
#define	FFA_VERSION(maj, min)		(((maj) << 16) | (min))

#define	FFA_VERSION_1_0			FFA_VERSION(1, 0)
#define	FFA_VERSION_1_1			FFA_VERSION(1, 1)
#define	FFA_VERSION_1_2			FFA_VERSION(1, 2)

/*
 * We advertise v1.2 and accept >= v1.0.
 */
#define	FFA_MY_VERSION			FFA_VERSION_1_2
#define	FFA_MIN_VERSION			FFA_VERSION_1_0

/*
 * FF-A function IDs (Chapters 12-17).
 *
 * 32-bit calls use SMC32 encoding (bit 30 set, bit 31 clear).
 * 64-bit calls use SMC64 encoding (bits 30-31 set).
 */
#define	FFA_ERROR			0x84000060	/* §12.2 */
#define	FFA_SUCCESS32			0x84000061	/* §12.3 */
#define	FFA_SUCCESS64			0xC4000061
#define	FFA_INTERRUPT			0x84000062	/* §14.4 */
#define	FFA_VERSION_FID			0x84000063	/* §13.2 */
#define	FFA_FEATURES			0x84000064	/* §13.3 */
#define	FFA_RX_RELEASE			0x84000065	/* §13.5 */
#define	FFA_RXTX_MAP32			0x84000066	/* §13.6 */
#define	FFA_RXTX_MAP64			0xC4000066
#define	FFA_RXTX_UNMAP			0x84000067	/* §13.7 */
#define	FFA_PARTITION_INFO_GET		0x84000068	/* §13.8 */
#define	FFA_ID_GET			0x84000069	/* §13.10 */
#define	FFA_YIELD32			0x8400006C	/* §14.2 */
#define	FFA_YIELD64			0xC400006C
#define	FFA_RUN32			0x8400006D	/* §14.3 */
#define	FFA_RUN64			0xC400006D
#define	FFA_NOTIFICATION_INFO_GET32	0x8400008A	/* §16.11 */
#define	FFA_NOTIFICATION_INFO_GET64	0xC400008A
#define	FFA_PARTITION_INFO_GET_REGS	0xC400008B	/* §13.9, v1.2+ */
#define	FFA_MSG_SEND_DIRECT_REQ2	0xC400008D	/* §15.4 */
#define	FFA_MSG_SEND_DIRECT_RESP2	0xC400008E	/* §15.5 */

/*
 * FF-A error codes (Table 12.2).
 */
#define	FFA_ERR_NOT_SUPPORTED		(-1)
#define	FFA_ERR_INVALID_PARAMETERS	(-2)
#define	FFA_ERR_NO_MEMORY		(-3)
#define	FFA_ERR_BUSY			(-4)
#define	FFA_ERR_INTERRUPTED		(-5)
#define	FFA_ERR_DENIED			(-6)
#define	FFA_ERR_RETRY			(-7)
#define	FFA_ERR_ABORTED			(-8)
#define	FFA_ERR_NO_DATA			(-9)
#define	FFA_ERR_NOT_READY		(-10)

/*
 * Feature IDs for FFA_FEATURES (Table 13.14).
 *
 * Bit 31 clear distinguishes these from function IDs.
 */
#define	FFA_FEAT_NPI		0x1	/* Notification Pending Interrupt */
#define	FFA_FEAT_SRI		0x2	/* Schedule Receiver Interrupt */
#define	FFA_FEAT_MEI		0x3	/* Managed Exit Interrupt */

/*
 * RXTX buffer size encoding in FFA_FEATURES return (§13.3).
 *
 * w2 bits[1:0] encode the minimum buffer size and alignment boundary.
 * w2 bits[31:16] encode the maximum size as a count of 4K pages (0 = no
 * limit).
 */
#define	FFA_RXTX_MIN_SZ_MASK	0x3
#define	FFA_RXTX_MIN_SZ_4K	0x0
#define	FFA_RXTX_MIN_SZ_64K	0x1
#define	FFA_RXTX_MIN_SZ_16K	0x2

#define	FFA_RXTX_MAX_PAGES(w2)	(((w2) >> 16) & 0xFFFF)

/*
 * RXTX_MAP page count field (§13.6): w3/x3 bits[5:0].
 */
#define	FFA_RXTX_PAGE_CNT_MASK	0x3F

/*
 * Sender endpoint ID for normal world physical instance (§8.1).
 */
#define	FFA_NS_PHYS_SENDER_ID	0

/*
 * DIRECT_REQ2/RESP2 payload registers: x4 through x17 (14 registers).
 */
#define	FFA_DIRECT_REQ2_NARGS	14

/*
 * Number of registers returned by ffa_direct_req2_raw: x0 through x17.
 */
#define	FFA_DIRECT_REQ2_NREGS	18

/*
 * Public interface.
 */
extern boolean_t ffa_available(void);
extern uint32_t ffa_version(void);
extern int ffa_partition_lookup(const uuid_t *uuid, uint16_t *part_id);
extern int ffa_direct_req2_raw(uint16_t part_id, const uuid_t *uuid,
    const uint64_t *in_args, uint64_t *out_regs, uint_t nargs);
extern int ffa_direct_req2(uint16_t part_id, const uuid_t *uuid,
    const uint64_t *args, uint64_t *results, uint_t nargs);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FFA_H */
