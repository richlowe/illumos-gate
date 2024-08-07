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
 * Copyright 2020 Joyent, Inc.
 * Copyright 2022 Tintri by DDN, Inc. All rights reserved.
 * Copyright 2022 OmniOS Community Edition (OmniOSce) Association.
 * Copyright 2024 Oxide Computer Company
 */

/*
 * NVMe hardware interface
 */

#ifndef _NVME_REG_H
#define	_NVME_REG_H

#include <sys/nvme.h>

#pragma pack(1)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NVMe constants
 */
#define	NVME_MAX_ADMIN_QUEUE_LEN	4096

/*
 * NVMe registers and register fields
 */
#define	NVME_REG_CAP	0x0		/* Controller Capabilities */
#define	NVME_REG_VS	0x8		/* Version */
#define	NVME_REG_INTMS	0xc		/* Interrupt Mask Set */
#define	NVME_REG_INTMC	0x10		/* Interrupt Mask Clear */
#define	NVME_REG_CC	0x14		/* Controller Configuration */
#define	NVME_REG_CSTS	0x1c		/* Controller Status */
#define	NVME_REG_NSSR	0x20		/* NVM Subsystem Reset */
#define	NVME_REG_AQA	0x24		/* Admin Queue Attributes */
#define	NVME_REG_ASQ	0x28		/* Admin Submission Queue */
#define	NVME_REG_ACQ	0x30		/* Admin Completion Qeueu */
#define	NVME_REG_CMBLOC	0x38		/* Controller Memory Buffer Location */
#define	NVME_REG_CMBSZ	0x3C		/* Controller Memory Buffer Size */
#define	NVME_REG_BPINFO	0x40		/* Boot Partition Information */
#define	NVME_REG_BPRSEL	0x44		/* Boot Partition Read Select */
#define	NVME_REG_BPMBL	0x48		/* Boot Partition Memory Buffer Loc */
#define	NVME_REG_SQTDBL(nvme, n) \
	(0x1000 + ((2 * (n)) * nvme->n_doorbell_stride))
#define	NVME_REG_CQHDBL(nvme, n) \
	(0x1000 + ((2 * (n) + 1) * nvme->n_doorbell_stride))

#define	 NVME_CAP_CSS_NVM	1	/* NVM Command Set */
#define	 NVME_CAP_AMS_WRR	1	/* Weighted Round-Robin */

/* CAP -- Controller Capabilities */
typedef union {
	struct {
		uint16_t cap_mqes;	/* Maximum Queue Entries Supported */
		uint8_t cap_cqr:1;	/* Contiguous Queues Required */
		uint8_t cap_ams:2;	/* Arbitration Mechanisms Supported */
		uint8_t cap_rsvd1:5;
		uint8_t cap_to;		/* Timeout */
		uint16_t cap_dstrd:4;	/* Doorbell Stride */
		uint16_t cap_nssrs:1;	/* NVM Subsystem Reset Supported */
		uint16_t cap_css:8;	/* Command Sets Supported */
		uint16_t cap_rsvd2:2;
		uint8_t cap_bps:1;	/* Boot Partition Support */
		uint8_t cap_mpsmin:4;	/* Memory Page Size Minimum */
		uint8_t cap_mpsmax:4;	/* Memory Page Size Maximum */
		uint8_t cap_rsvd3;
	} b;
	uint64_t r;
} nvme_reg_cap_t;

/* VS -- Version */
typedef union {
	struct {
		uint8_t vs_rsvd;
		uint8_t vs_mnr;		/* Minor Version Number */
		uint16_t vs_mjr;	/* Major Version Number */
	} b;
	uint32_t r;
} nvme_reg_vs_t;

/* CC -- Controller Configuration */
#define	NVME_CC_SHN_NORMAL	1	/* Normal Shutdown Notification */
#define	NVME_CC_SHN_ABRUPT	2	/* Abrupt Shutdown Notification */

typedef union {
	struct {
		uint16_t cc_en:1;	/* Enable */
		uint16_t cc_rsvd1:3;
		uint16_t cc_css:3;	/* I/O Command Set Selected */
		uint16_t cc_mps:4;	/* Memory Page Size */
		uint16_t cc_ams:3;	/* Arbitration Mechanism Selected */
		uint16_t cc_shn:2;	/* Shutdown Notification */
		uint8_t cc_iosqes:4;	/* I/O Submission Queue Entry Size */
		uint8_t cc_iocqes:4;	/* I/O Completion Queue Entry Size */
		uint8_t cc_rsvd2;
	} b;
	uint32_t r;
} nvme_reg_cc_t;

/* CSTS -- Controller Status */
#define	NVME_CSTS_SHN_OCCURING	1	/* Shutdown Processing Occuring */
#define	NVME_CSTS_SHN_COMPLETE	2	/* Shutdown Processing Complete */

typedef union {
	struct {
		uint32_t csts_rdy:1;	/* Ready */
		uint32_t csts_cfs:1;	/* Controller Fatal Status */
		uint32_t csts_shst:2;	/* Shutdown Status */
		uint32_t csts_nssro:1;	/* NVM Subsystem Reset Occured */
		uint32_t csts_pp:1;	/* Processing Paused */
		uint32_t csts_rsvd:26;
	} b;
	uint32_t r;
} nvme_reg_csts_t;

/* NSSR -- NVM Subsystem Reset */
#define	NVME_NSSR_NSSRC	0x4e564d65	/* NSSR magic value */
typedef uint32_t nvme_reg_nssr_t;

/* AQA -- Admin Queue Attributes */
typedef union {
	struct {
		uint16_t aqa_asqs:12;	/* Admin Submission Queue Size */
		uint16_t aqa_rsvd1:4;
		uint16_t aqa_acqs:12;	/* Admin Completion Queue Size */
		uint16_t aqa_rsvd2:4;
	} b;
	uint32_t r;
} nvme_reg_aqa_t;

/*
 * The spec specifies the lower 12 bits of ASQ and ACQ as reserved, which is
 * probably a specification bug. The full 64bit regs are used as base address,
 * and the lower bits must be zero to ensure alignment on the page size
 * specified in CC.MPS.
 */
/* ASQ -- Admin Submission Queue Base Address */
typedef uint64_t nvme_reg_asq_t;	/* Admin Submission Queue Base */

/* ACQ -- Admin Completion Queue Base Address */
typedef uint64_t nvme_reg_acq_t;	/* Admin Completion Queue Base */

/* CMBLOC - Controller Memory Buffer Location */
typedef union {
	struct {
		uint32_t cmbloc_bir:3;		/* Base Indicator Register */
		uint32_t cmbloc_rsvd:9;
		uint32_t cmbloc_ofst:20;	/* Offset */
	} b;
	uint32_t r;
} nvme_reg_cmbloc_t;

/* CMBSZ - Controller Memory Buffer Size */
typedef union {
	struct {
		uint32_t cmbsz_sqs:1;	/* Submission Queue Support */
		uint32_t cmbsz_cqs:1;	/* Completion Queue Support */
		uint32_t cmbsz_lists:1;	/* PRP SGL List Support */
		uint32_t cmbsz_rds:1;	/* Read Data Support */
		uint32_t cmbsz_wds:1;	/* Write Data Support */
		uint32_t cmbsz_rsvd:3;
		uint32_t cmbsz_szu:4;	/* Size Units */
		uint32_t cmbsz_sz:20;	/* Size */
	} b;
	uint32_t r;
} nvme_reg_cmbsz_t;

/* BPINFO - Boot Partition Information */
typedef union {
	struct {
		uint32_t bpinfo_bpsz:15;	/* Boot Partition Size */
		uint32_t bpinfo_rsvd:9;
		uint32_t bpinfo_brs:2;		/* Boot Read Status */
		uint32_t bpinfo_rsvd2:5;
		uint32_t bpinfo_abpid:1;	/* Active Boot Partition ID */
	} b;
	uint32_t r;
} nvme_reg_bpinfo_t;

/* BPRSEL - Boot Partition Read Select */
typedef union {
	struct {
		uint32_t bprsel_bprsz:10;	/* Boot Partition Read Size */
		uint32_t bprsel_bprof:20;	/* Boot Partition Read Offset */
		uint32_t bprsel_rsvd:1;
		uint32_t bprsel_bpid:1;		/* Boot Partition Identifier */
	} b;
	uint32_t r;
} nvme_reg_bprsel_t;

/* BPMBL - Boot Partition Memory Location Buffer Location */
typedef uint64_t nvme_reg_bpbml_t;	/* Memory Buffer Base Address */

/* SQyTDBL -- Submission Queue y Tail Doorbell */
typedef union {
	struct {
		uint16_t sqtdbl_sqt;	/* Submission Queue Tail */
		uint16_t sqtdbl_rsvd;
	} b;
	uint32_t r;
} nvme_reg_sqtdbl_t;

/* CQyHDBL -- Completion Queue y Head Doorbell */
typedef union {
	struct {
		uint16_t cqhdbl_cqh;	/* Completion Queue Head */
		uint16_t cqhdbl_rsvd;
	} b;
	uint32_t r;
} nvme_reg_cqhdbl_t;

/*
 * NVMe submission queue entries
 */

/* NVMe scatter/gather list descriptor */
typedef struct {
	uint64_t sgl_addr;		/* Address */
	uint32_t sgl_len;		/* Length */
	uint8_t sgl_rsvd[3];
	uint8_t sgl_zero:4;
	uint8_t sgl_type:4;		/* SGL descriptor type */
} nvme_sgl_t;

/* NVMe SGL descriptor type */
#define	NVME_SGL_DATA_BLOCK	0
#define	NVME_SGL_BIT_BUCKET	1
#define	NVME_SGL_SEGMENT	2
#define	NVME_SGL_LAST_SEGMENT	3
#define	NVME_SGL_VENDOR		0xf

/* NVMe submission queue entry */
typedef struct {
	uint8_t sqe_opc;		/* Opcode */
	uint8_t sqe_fuse:2;		/* Fused Operation */
	uint8_t sqe_rsvd:5;
	uint8_t sqe_psdt:1;		/* PRP or SGL for Data Transfer */
	uint16_t sqe_cid;		/* Command Identifier */
	uint32_t sqe_nsid;		/* Namespace Identifier */
	uint64_t sqe_rsvd1;
	union {
		uint64_t m_ptr;		/* Metadata Pointer */
		uint64_t m_sglp;	/* Metadata SGL Segment Pointer */
	} sqe_m;
	union {
		uint64_t d_prp[2];	/* Physical Page Region Entries 1 & 2 */
		nvme_sgl_t d_sgl;	/* SGL Entry 1 */
	} sqe_dptr;			/* Data Pointer */
	uint32_t sqe_cdw10;		/* Number of Dwords in Data Transfer */
	uint32_t sqe_cdw11;		/* Number of Dwords in Metadata Xfer */
	uint32_t sqe_cdw12;
	uint32_t sqe_cdw13;
	uint32_t sqe_cdw14;
	uint32_t sqe_cdw15;
} nvme_sqe_t;

/* NVMe admin command opcodes */
#define	NVME_OPC_DELETE_SQUEUE	0x0
#define	NVME_OPC_CREATE_SQUEUE	0x1
#define	NVME_OPC_GET_LOG_PAGE	0x2
#define	NVME_OPC_DELETE_CQUEUE	0x4
#define	NVME_OPC_CREATE_CQUEUE	0x5
#define	NVME_OPC_IDENTIFY	0x6
#define	NVME_OPC_ABORT		0x8
#define	NVME_OPC_SET_FEATURES	0x9
#define	NVME_OPC_GET_FEATURES	0xa
#define	NVME_OPC_ASYNC_EVENT	0xc
#define	NVME_OPC_NS_MGMT	0xd	/* 1.2 */
#define	NVME_OPC_FW_ACTIVATE	0x10
#define	NVME_OPC_FW_IMAGE_LOAD	0x11
#define	NVME_OPC_SELF_TEST	0x14	/* 1.3 */
#define	NVME_OPC_NS_ATTACH	0x15	/* 1.2 */
#define	NVME_OPC_KEEP_ALIVE	0x18	/* 1.3 */
#define	NVME_OPC_DIRECTIVE_SEND	0x19	/* 1.3 */
#define	NVME_OPC_DIRECTIVE_RECV	0x1A	/* 1.3 */
#define	NVME_OPC_VIRT_MGMT	0x1C	/* 1.3 */
#define	NVME_OPC_NVMEMI_SEND	0x1D	/* 1.3 */
#define	NVME_OPC_NVMEMI_RECV	0x1E	/* 1.3 */
#define	NVME_OPC_DB_CONFIG	0x7C	/* 1.3 */

#define	NVME_OPC_VENDOR_LOW	0xC0
#define	NVME_OPC_VENDOR_HIGH	0xFF
#define	NVME_IS_VENDOR_SPECIFIC_CMD(x)	\
	((x) >= NVME_OPC_VENDOR_LOW && (x) <= NVME_OPC_VENDOR_HIGH)

/* NVMe NVM command set specific admin command opcodes */
#define	NVME_OPC_NVM_FORMAT	0x80
#define	NVME_OPC_NVM_SEC_SEND	0x81
#define	NVME_OPC_NVM_SEC_RECV	0x82

/* NVMe NVM command opcodes */
#define	NVME_OPC_NVM_FLUSH	0x0
#define	NVME_OPC_NVM_WRITE	0x1
#define	NVME_OPC_NVM_READ	0x2
#define	NVME_OPC_NVM_WRITE_UNC	0x4
#define	NVME_OPC_NVM_COMPARE	0x5
#define	NVME_OPC_NVM_WRITE_ZERO	0x8
#define	NVME_OPC_NVM_DSET_MGMT	0x9
#define	NVME_OPC_NVM_RESV_REG	0xd
#define	NVME_OPC_NVM_RESV_REPRT	0xe
#define	NVME_OPC_NVM_RESV_ACQ	0x11
#define	NVME_OPC_NVM_RESV_REL	0x12

/*
 * NVMe completion queue entry
 */
typedef struct {
	uint32_t cqe_dw0;		/* Command Specific */
	uint32_t cqe_rsvd1;
	uint16_t cqe_sqhd;		/* SQ Head Pointer */
	uint16_t cqe_sqid;		/* SQ Identifier */
	uint16_t cqe_cid;		/* Command Identifier */
	nvme_cqe_sf_t cqe_sf;		/* Status Field */
} nvme_cqe_t;

/*
 * NVMe Asynchronous Event Request
 */
#define	NVME_ASYNC_TYPE_ERROR		0x0	/* Error Status */
#define	NVME_ASYNC_TYPE_HEALTH		0x1	/* SMART/Health Status */
#define	NVME_ASYNC_TYPE_NOTICE		0x2	/* Notice (1.2) */
#define	NVME_ASYNC_TYPE_VENDOR		0x7	/* Vendor specific */

#define	NVME_ASYNC_ERROR_INV_SQ		0x0	/* Invalid Submission Queue */
#define	NVME_ASYNC_ERROR_INV_DBL	0x1	/* Invalid Doorbell Write */
#define	NVME_ASYNC_ERROR_DIAGFAIL	0x2	/* Diagnostic Failure */
#define	NVME_ASYNC_ERROR_PERSISTENT	0x3	/* Persistent Internal Error */
#define	NVME_ASYNC_ERROR_TRANSIENT	0x4	/* Transient Internal Error */
#define	NVME_ASYNC_ERROR_FW_LOAD	0x5	/* Firmware Image Load Error */

#define	NVME_ASYNC_HEALTH_RELIABILITY	0x0	/* Device Reliability */
#define	NVME_ASYNC_HEALTH_TEMPERATURE	0x1	/* Temp. Above Threshold */
#define	NVME_ASYNC_HEALTH_SPARE		0x2	/* Spare Below Threshold */

/* NVMe 1.2 */
#define	NVME_ASYNC_NOTICE_NS_CHANGE	0x0	/* Namespace attribute change */
#define	NVME_ASYNC_NOTICE_FW_ACTIVATE	0x1	/* Firmware activation start */
/* NVMe 1.3 */
#define	NVME_ASYNC_NOTICE_TELEMETRY	0x2	/* Telemetry log changed */
/* NVMe 1.4 */
#define	NVME_ASYNC_NOTICE_NS_ASYMM	0x3	/* Asymm. NS access change */
#define	NVME_ASYNC_NOTICE_LATENCYLOG	0x4	/* Pred. Latency log change */
#define	NVME_ASYNC_NOTICE_LBASTATUS	0x5	/* LBA status alert */
#define	NVME_ASYNC_NOTICE_ENDURANCELOG	0x6	/* Endurance log change */

typedef union {
	struct {
		uint8_t ae_type:3;		/* Asynchronous Event Type */
		uint8_t ae_rsvd1:5;
		uint8_t ae_info;		/* Asynchronous Event Info */
		uint8_t ae_logpage;		/* Associated Log Page */
		uint8_t ae_rsvd2;
	} b;
	uint32_t r;
} nvme_async_event_t;

/*
 * NVMe Create Completion/Submission Queue
 */
typedef union {
	struct {
		uint16_t q_qid;			/* Queue Identifier */
		uint16_t q_qsize;		/* Queue Size */
	} b;
	uint32_t r;
} nvme_create_queue_dw10_t;

typedef union {
	struct {
		uint16_t cq_pc:1;		/* Physically Contiguous */
		uint16_t cq_ien:1;		/* Interrupts Enabled */
		uint16_t cq_rsvd:14;
		uint16_t cq_iv;			/* Interrupt Vector */
	} b;
	uint32_t r;
} nvme_create_cq_dw11_t;

typedef union {
	struct {
		uint16_t sq_pc:1;		/* Physically Contiguous */
		uint16_t sq_qprio:2;		/* Queue Priority */
		uint16_t sq_rsvd:13;
		uint16_t sq_cqid;		/* Completion Queue ID */
	} b;
	uint32_t r;
} nvme_create_sq_dw11_t;

/*
 * NVMe Abort Command
 */
typedef union {
	struct {
		uint16_t ac_sqid;	/* Submission Queue ID */
		uint16_t ac_cid;	/* Command ID */
	} b;
	uint32_t r;
} nvme_abort_cmd_t;

/*
 * NVMe Get Log Page. dw12/13 are the lower and upper halves of the 64-bit
 * offset field respectively in bytes. These must be dword aligned. The offset
 * was added in NVMe v1.2, but requires controller support.
 */
typedef union {
	struct {
		uint8_t lp_lid;		/* Log Page Identifier */
		/*
		 * The log-specific field was introduced as a 4-bit field in
		 * NVMe 1.3. It was extended to be a 7-bit field in NVMe 2.0 and
		 * renamed log-specific parameter.
		 */
		uint8_t lp_lsp:7;
		uint8_t lp_rae:1;	/* Retain Async Event  v1.3 */
		/*
		 * This is the lower number of dwords. This was changed in NVMe
		 * v1.2 to be split between this field and dw11. In NVMe 1.0/1.1
		 * this was only 12 bits long.
		 */
		uint16_t lp_lnumdl;	/* Number of Dwords */
	} b;
	uint32_t r;
} nvme_getlogpage_dw10_t;

typedef union {
	struct {
		uint16_t lp_numdu;	/* Number of dwords v1.2 */
		uint16_t lp_lsi;	/* Log Specific Field v1.3 */
	} b;
	uint32_t r;
} nvme_getlogpage_dw11_t;

typedef union {
	struct {
		uint8_t lp_uuid:7;	/* UUID Index v1.4 */
		uint8_t lp_rsvd1:1;
		uint8_t lp_rsvd2;
		uint8_t lp_rsvd3:7;
		uint8_t lp_ot:1;	/* Offset Type v2.0 */
		uint8_t lp_csi;		/* Command Set Identifier v2.0 */
	} b;
	uint32_t r;
} nvme_getlogpage_dw14_t;

/*
 * dword11 values for the dataset management command. Note that the dword11
 * attributes are distinct from the context attributes (nr_ctxattr) values
 * for an individual range (of the context attribute values defined by the NVMe
 * spec, none are currently used by the NVMe driver).
 */
#define	NVME_DSET_MGMT_ATTR_OPT_READ	0x01
#define	NVME_DSET_MGMT_ATTR_OPT_WRITE	0x02
#define	NVME_DSET_MGMT_ATTR_DEALLOCATE	0x04

#define	NVME_DSET_MGMT_MAX_RANGES	256
typedef struct {
	uint32_t	nr_ctxattr;
	uint32_t	nr_len;
	uint64_t	nr_lba;
} nvme_range_t;

/*
 * NVMe Identify Command
 */
typedef union {
	struct {
		/*
		 * The controller or namespace structure (CNS). This field was
		 * originally a single bit wide in NVMe 1.0. It was two bits
		 * wide in NVMe 1.1 and was increased to 8 bits in NVMe 1.2.
		 */
		uint8_t id_cns;
		uint8_t id_rsvd0;
		uint16_t id_cntid;	/* Controller ID, NVMe 1.2 */
	} b;
	uint32_t r;
} nvme_identify_dw10_t;

#ifdef __cplusplus
}
#endif

#pragma pack() /* pack(1) */

#endif /* _NVME_REG_H */
