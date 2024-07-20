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
 * Copyright (c) 1992, 2010, Oracle and/or its affiliates. All rights reserved.
 *
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2012 Garrett D'Amore <garrett@damore.org>
 * Copyright 2014 Pluribus Networks, Inc.
 * Copyright 2016 Nexenta Systems, Inc.
 * Copyright 2017 Hayashi Naoyuki
 * Copyright 2018 Joyent, Inc.
 * Copyright 2024 Michael van der Westhuizen
 */

#include <sys/types.h>
#include <sys/autoconf.h>
#include <sys/avintr.h>
#include <sys/bootconf.h>
#include <sys/conf.h>
#include <sys/cpuvar.h>
#include <sys/ddi_impldefs.h>
#include <sys/ethernet.h>
#include <sys/instance.h>
#include <sys/kmem.h>
#include <sys/machsystm.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/ddi_impldefs.h>
#include <sys/sysmacros.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <sys/atomic.h>
#include <sys/spl.h>
#include <sys/archsystm.h>
#include <vm/seg_kmem.h>
#include <sys/ontrap.h>
#include <sys/fm/protocol.h>
#include <sys/ramdisk.h>
#include <sys/sunndi.h>
#include <sys/vmem.h>
#include <sys/lgrp.h>
#include <sys/mach_intr.h>
#include <vm/hat_aarch64.h>

size_t dma_max_copybuf_size = 0x101000;		/* 1M + 4K */
uint64_t ramdisk_start, ramdisk_end;

static void impl_bus_initialprobe(void);

/*
 * Platform drivers on this platform
 */
char *platform_module_list[] = {
	"sad",
	(char *)0
};

/*
 * We use an AVL tree to store contiguous address allocations made with the
 * kalloca() routine, so that we can return the size to free with kfreea().
 * Note that in the future it would be vastly faster if we could eliminate
 * this lookup by insisting that all callers keep track of their own sizes,
 * just as for kmem_alloc().
 */
struct ctgas {
	avl_node_t ctg_link;
	void *ctg_addr;
	size_t ctg_size;
};

static avl_tree_t ctgtree;

static kmutex_t		ctgmutex;
#define	CTGLOCK()	mutex_enter(&ctgmutex)
#define	CTGUNLOCK()	mutex_exit(&ctgmutex)


uint8_t
i_ddi_get8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	uint8_t x;
	__asm__ volatile("ldrb %w0, %1" : "=r" (x) : "Q" (*addr) : "memory");
	return (x);
}

uint16_t
i_ddi_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	uint16_t x;
	__asm__ volatile("ldrh %w0, %1" : "=r" (x) : "Q" (*addr) : "memory");
	return (x);
}

uint32_t
i_ddi_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	uint32_t x;
	__asm__ volatile("ldr %w0, %1" : "=r" (x) : "Q" (*addr) : "memory");
	return (x);
}

uint64_t
i_ddi_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	uint64_t x;
	__asm__ volatile("ldr %0, %1" : "=r" (x) : "Q" (*addr) : "memory");
	return (x);
}

void
i_ddi_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	__asm__ volatile("strb %w1, %0" : "=Q" (*addr) : "r"(value) : "memory");
}

void
i_ddi_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	__asm__ volatile("strh %w1, %0" : "=Q" (*addr) : "r"(value) : "memory");
}

void
i_ddi_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	__asm__ volatile("str %w1, %0" : "=Q" (*addr) : "r"(value) : "memory");
}

void
i_ddi_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	__asm__ volatile("str %1, %0" : "=Q" (*addr) : "r"(value) : "memory");
}

void
i_ddi_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr, uint8_t *dev_addr,
    size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("ldrb %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(dev_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("ldrb %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(dev_addr))
			    : "memory");
}

void
i_ddi_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr, uint16_t *dev_addr,
    size_t repcount, const uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("ldrh %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(dev_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("ldrh %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(dev_addr))
			    : "memory");
}

void
i_ddi_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr, uint32_t *dev_addr,
    size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("ldr %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(dev_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("ldr %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(dev_addr))
			    : "memory");
}

void
i_ddi_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr, uint64_t *dev_addr,
    size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("ldr %0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(dev_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("ldr %0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(dev_addr))
			    : "memory");
}

void
i_ddi_rep_put8(ddi_acc_impl_t *hdlp, uint8_t *host_addr, uint8_t *dev_addr,
    size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("strb %w1, %0"
			    : "=Q" (*(dev_addr++))
			    : "r"(*(host_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("strb %w1, %0"
			    : "=Q" (*(dev_addr))
			    : "r"(*(host_addr++))
			    : "memory");
}

void
i_ddi_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr, uint16_t *dev_addr,
    size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("strh %w1, %0"
			    : "=Q" (*(dev_addr++))
			    : "r"(*(host_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("strh %w1, %0"
			    : "=Q" (*(dev_addr))
			    : "r"(*(host_addr++))
			    : "memory");
}

void
i_ddi_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr, uint32_t *dev_addr,
    size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("str %w1, %0"
			    : "=Q" (*(dev_addr++))
			    : "r"(*(host_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("str %w1, %0"
			    : "=Q" (*(dev_addr))
			    : "r"(*(host_addr++))
			    : "memory");
}

void
i_ddi_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr, uint64_t *dev_addr,
    size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("str %1, %0"
			    : "=Q" (*(dev_addr++))
			    : "r"(*(host_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("str %1, %0"
			    : "=Q" (*(dev_addr))
			    : "r"(*(host_addr++))
			    : "memory");
}

uint16_t
i_ddi_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	uint16_t x;
	__asm__ volatile("ldrh %w0, %1"
	    : "=r" (x)
	    : "Q" (*addr)
	    : "memory");
	return (__builtin_bswap16(x));
}

uint32_t
i_ddi_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	uint32_t x;
	__asm__ volatile("ldr %w0, %1"
	    : "=r" (x)
	    : "Q" (*addr)
	    : "memory");
	return (__builtin_bswap32(x));
}

uint64_t
i_ddi_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	uint64_t x;
	__asm__ volatile("ldr %0, %1"
	    : "=r" (x)
	    : "Q" (*addr)
	    : "memory");
	return (__builtin_bswap64(x));
}

void
i_ddi_swap_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	__asm__ volatile("strh %w1, %0"
	    : "=Q" (*addr)
	    : "r"(__builtin_bswap16(value))
	    : "memory");
}

void
i_ddi_swap_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	__asm__ volatile("str %w1, %0"
	    : "=Q" (*addr)
	    : "r"(__builtin_bswap32(value))
	    : "memory");
}

void
i_ddi_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	__asm__ volatile("str %1, %0"
	    : "=Q" (*addr)
	    : "r"(__builtin_bswap64(value))
	    : "memory");
}

void
i_ddi_swap_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, const uint_t flags)
{
	uint16_t x;
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--) {
			__asm__ volatile("ldrh %w0, %1"
			    : "=r" (x)
			    : "Q" (*(dev_addr++))
			    : "memory");
			*host_addr++ = __builtin_bswap16(x);
		}
	else
		while (repcount--) {
			__asm__ volatile("ldrh %w0, %1"
			    : "=r" (x)
			    : "Q" (*(dev_addr))
			    : "memory");
			*host_addr++ = __builtin_bswap16(x);
		}
}

void
i_ddi_swap_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t x;
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--) {
			__asm__ volatile("ldr %w0, %1"
			    : "=r" (x)
			    : "Q" (*(dev_addr++))
			    : "memory");
			*host_addr++ = __builtin_bswap32(x);
		}
	else
		while (repcount--) {
			__asm__ volatile("ldr %w0, %1"
			    : "=r" (x)
			    : "Q" (*(dev_addr))
			    : "memory");
			*host_addr++ = __builtin_bswap32(x);
		}
}

void
i_ddi_swap_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t x;
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--) {
			__asm__ volatile("ldr %0, %1"
			    : "=r" (x)
			    : "Q" (*(dev_addr++))
			    : "memory");
			*host_addr++ = __builtin_bswap64(x);
		}
	else
		while (repcount--) {
			__asm__ volatile("ldr %0, %1"
			    : "=r" (x)
			    : "Q" (*(dev_addr))
			    : "memory");
			*host_addr++ = __builtin_bswap64(x);
		}
}

void
i_ddi_swap_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("strh %w1, %0"
			    : "=Q" (*(dev_addr++))
			    : "r"(__builtin_bswap16(*(host_addr++)))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("strh %w1, %0"
			    : "=Q" (*(dev_addr))
			    : "r"(__builtin_bswap16(*(host_addr++)))
			    : "memory");
}

void
i_ddi_swap_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("str %w1, %0"
			    : "=Q" (*(dev_addr++))
			    : "r"(__builtin_bswap32(*(host_addr++)))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("str %w1, %0"
			    : "=Q" (*(dev_addr))
			    : "r"(__builtin_bswap32(*(host_addr++)))
			    : "memory");
}

void
i_ddi_swap_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("str %1, %0"
			    : "=Q" (*(dev_addr++))
			    : "r"(__builtin_bswap64(*(host_addr++)))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("str %1, %0"
			    : "=Q" (*(dev_addr))
			    : "r"(__builtin_bswap64(*(host_addr++)))
			    : "memory");
}

uint8_t
i_ddi_io_get8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	uint8_t *io_addr =
	    (uint8_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	uint8_t x;
	__asm__ volatile("ldrb %w0, %1" : "=r" (x) : "Q" (*io_addr) : "memory");
	return (x);
}

uint16_t
i_ddi_io_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	uint16_t *io_addr =
	    (uint16_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	uint16_t x;
	__asm__ volatile("ldrh %w0, %1" : "=r" (x) : "Q" (*io_addr) : "memory");
	return (x);
}

uint32_t
i_ddi_io_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	uint32_t *io_addr =
	    (uint32_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	uint32_t x;
	__asm__ volatile("ldr %w0, %1" : "=r" (x) : "Q" (*io_addr) : "memory");
	return (x);
}

uint64_t
i_ddi_io_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	uint64_t *io_addr =
	    (uint64_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	uint64_t x;
	__asm__ volatile("ldr %0, %1" : "=r" (x) : "Q" (*io_addr) : "memory");
	return (x);
}

void
i_ddi_io_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	uint8_t *io_addr =
	    (uint8_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	__asm__ volatile("strb %w1, %0"
	    : "=Q" (*io_addr)
	    : "r"(value)
	    : "memory");
}

void
i_ddi_io_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	uint16_t *io_addr =
	    (uint16_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	__asm__ volatile("strh %w1, %0"
	    : "=Q" (*io_addr)
	    : "r"(value)
	    : "memory");
}

void
i_ddi_io_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	uint32_t *io_addr =
	    (uint32_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	__asm__ volatile("str %w1, %0"
	    : "=Q" (*io_addr)
	    : "r"(value)
	    : "memory");
}

void
i_ddi_io_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	uint64_t *io_addr =
	    (uint64_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	__asm__ volatile("str %1, %0"
	    : "=Q" (*io_addr)
	    : "r"(value)
	    : "memory");
}

void
i_ddi_io_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
    uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t *io_dev_addr =
	    (uint8_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("ldrb %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(io_dev_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("ldrb %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(io_dev_addr))
			    : "memory");
}

void
i_ddi_io_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, const uint_t flags)
{
	uint16_t *io_dev_addr =
	    (uint16_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("ldrh %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(io_dev_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("ldrh %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(io_dev_addr))
			    : "memory");
}

void
i_ddi_io_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *io_dev_addr =
	    (uint32_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("ldr %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(io_dev_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("ldr %w0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(io_dev_addr))
			    : "memory");
}

void
i_ddi_io_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *io_dev_addr =
	    (uint64_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("ldr %0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(io_dev_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("ldr %0, %1"
			    : "=r" (*(host_addr++))
			    : "Q" (*(io_dev_addr))
			    : "memory");
}

void
i_ddi_io_rep_put8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
    uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t *io_dev_addr =
	    (uint8_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("strb %w1, %0"
			    : "=Q" (*(io_dev_addr++))
			    : "r"(*(host_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("strb %w1, %0"
			    : "=Q" (*(io_dev_addr))
			    : "r"(*(host_addr++))
			    : "memory");
}

void
i_ddi_io_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *io_dev_addr =
	    (uint16_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("strh %w1, %0"
			    : "=Q" (*(io_dev_addr++))
			    : "r"(*(host_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("strh %w1, %0"
			    : "=Q" (*(io_dev_addr))
			    : "r"(*(host_addr++))
			    : "memory");
}

void
i_ddi_io_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *io_dev_addr =
	    (uint32_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("str %w1, %0"
			    : "=Q" (*(io_dev_addr++))
			    : "r"(*(host_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("str %w1, %0"
			    : "=Q" (*(io_dev_addr))
			    : "r"(*(host_addr++))
			    : "memory");
}

void
i_ddi_io_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *io_dev_addr =
	    (uint64_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("str %1, %0"
			    : "=Q" (*(io_dev_addr++))
			    : "r"(*(host_addr++))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("str %1, %0"
			    : "=Q" (*(io_dev_addr))
			    : "r"(*(host_addr++))
			    : "memory");
}

uint16_t
i_ddi_io_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	uint16_t *io_addr =
	    (uint16_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	uint16_t x;
	__asm__ volatile("ldrh %w0, %1"
	    : "=r" (x)
	    : "Q" (*io_addr)
	    : "memory");
	return (__builtin_bswap16(x));
}

uint32_t
i_ddi_io_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	uint32_t *io_addr =
	    (uint32_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	uint32_t x;
	__asm__ volatile("ldr %w0, %1"
	    : "=r" (x)
	    : "Q" (*io_addr)
	    : "memory");
	return (__builtin_bswap32(x));
}

uint64_t
i_ddi_io_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	uint64_t *io_addr =
	    (uint64_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	uint64_t x;
	__asm__ volatile("ldr %0, %1"
	    : "=r" (x)
	    : "Q" (*io_addr)
	    : "memory");
	return (__builtin_bswap64(x));
}

void
i_ddi_io_swap_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	uint16_t *io_addr =
	    (uint16_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	__asm__ volatile("strh %w1, %0"
	    : "=Q" (*io_addr)
	    : "r"(__builtin_bswap16(value))
	    : "memory");
}

void
i_ddi_io_swap_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	uint32_t *io_addr =
	    (uint32_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	__asm__ volatile("str %w1, %0"
	    : "=Q" (*io_addr)
	    : "r"(__builtin_bswap32(value))
	    : "memory");
}

void
i_ddi_io_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	uint64_t *io_addr =
	    (uint64_t *)((uintptr_t)addr + hdlp->ahi_io_port_base);
	__asm__ volatile("str %1, %0"
	    : "=Q" (*io_addr)
	    : "r"(__builtin_bswap64(value))
	    : "memory");
}

void
i_ddi_io_swap_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, const uint_t flags)
{
	uint16_t *io_dev_addr =
	    (uint16_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	uint16_t x;
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--) {
			__asm__ volatile("ldrh %w0, %1"
			    : "=r" (x)
			    : "Q" (*(io_dev_addr++))
			    : "memory");
			*host_addr++ = __builtin_bswap16(x);
		}
	else
		while (repcount--) {
			__asm__ volatile("ldrh %w0, %1"
			    : "=r" (x)
			    : "Q" (*(io_dev_addr))
			    : "memory");
			*host_addr++ = __builtin_bswap16(x);
		}
}

void
i_ddi_io_swap_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *io_dev_addr =
	    (uint32_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	uint32_t x;
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--) {
			__asm__ volatile("ldr %w0, %1"
			    : "=r" (x)
			    : "Q" (*(io_dev_addr++))
			    : "memory");
			*host_addr++ = __builtin_bswap32(x);
		}
	else
		while (repcount--) {
			__asm__ volatile("ldr %w0, %1"
			    : "=r" (x)
			    : "Q" (*(io_dev_addr))
			    : "memory");
			*host_addr++ = __builtin_bswap32(x);
		}
}

void
i_ddi_io_swap_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *io_dev_addr =
	    (uint64_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	uint64_t x;
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--) {
			__asm__ volatile("ldr %0, %1"
			    : "=r" (x)
			    : "Q" (*(io_dev_addr++))
			    : "memory");
			*host_addr++ = __builtin_bswap64(x);
		}
	else
		while (repcount--) {
			__asm__ volatile("ldr %0, %1"
			    : "=r" (x)
			    : "Q" (*(io_dev_addr))
			    : "memory");
			*host_addr++ = __builtin_bswap64(x);
		}
}

void
i_ddi_io_swap_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *io_dev_addr =
	    (uint16_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("strh %w1, %0"
			    : "=Q" (*(io_dev_addr++))
			    : "r"(__builtin_bswap16(*(host_addr++)))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("strh %w1, %0"
			    : "=Q" (*(io_dev_addr))
			    : "r"(__builtin_bswap16(*(host_addr++)))
			    : "memory");
}

void
i_ddi_io_swap_rep_put32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *io_dev_addr =
	    (uint32_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("str %w1, %0"
			    : "=Q" (*(io_dev_addr++))
			    : "r"(__builtin_bswap32(*(host_addr++)))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("str %w1, %0"
			    : "=Q" (*(io_dev_addr))
			    : "r"(__builtin_bswap32(*(host_addr++)))
			    : "memory");
}

void
i_ddi_io_swap_rep_put64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	uint64_t *io_dev_addr =
	    (uint64_t *)((uintptr_t)dev_addr + hdlp->ahi_io_port_base);
	if (flags == DDI_DEV_AUTOINCR)
		while (repcount--)
			__asm__ volatile("str %1, %0"
			    : "=Q" (*(io_dev_addr++))
			    : "r"(__builtin_bswap64(*(host_addr++)))
			    : "memory");
	else
		while (repcount--)
			__asm__ volatile("str %1, %0"
			    : "=Q" (*(io_dev_addr))
			    : "r"(__builtin_bswap64(*(host_addr++)))
			    : "memory");
}

static void
i_ddi_caut_getput_ctlops(ddi_acc_impl_t *hp, uint64_t host_addr,
    uint64_t dev_addr, size_t size, size_t repcount,
    uint_t flags, ddi_ctl_enum_t cmd)
{
	peekpoke_ctlops_t	cautacc_ctlops_arg;

	cautacc_ctlops_arg.size = size;
	cautacc_ctlops_arg.dev_addr = dev_addr;
	cautacc_ctlops_arg.host_addr = host_addr;
	cautacc_ctlops_arg.handle = (ddi_acc_handle_t)hp;
	cautacc_ctlops_arg.repcount = repcount;
	cautacc_ctlops_arg.flags = flags;

	(void) ddi_ctlops(hp->ahi_common.ah_dip, hp->ahi_common.ah_dip,
	    cmd, &cautacc_ctlops_arg, NULL);
}

uint8_t
i_ddi_caut_get8(ddi_acc_impl_t *hp, uint8_t *addr)
{
	uint8_t value;
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)&value, (uintptr_t)addr,
	    sizeof (uint8_t), 1, 0, DDI_CTLOPS_PEEK);

	return (value);
}

uint16_t
i_ddi_caut_get16(ddi_acc_impl_t *hp, uint16_t *addr)
{
	uint16_t value;
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)&value, (uintptr_t)addr,
	    sizeof (uint16_t), 1, 0, DDI_CTLOPS_PEEK);

	return (value);
}

uint32_t
i_ddi_caut_get32(ddi_acc_impl_t *hp, uint32_t *addr)
{
	uint32_t value;
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)&value, (uintptr_t)addr,
	    sizeof (uint32_t), 1, 0, DDI_CTLOPS_PEEK);

	return (value);
}

uint64_t
i_ddi_caut_get64(ddi_acc_impl_t *hp, uint64_t *addr)
{
	uint64_t value;
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)&value, (uintptr_t)addr,
	    sizeof (uint64_t), 1, 0, DDI_CTLOPS_PEEK);

	return (value);
}

void
i_ddi_caut_put8(ddi_acc_impl_t *hp, uint8_t *addr, uint8_t value)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)&value, (uintptr_t)addr,
	    sizeof (uint8_t), 1, 0, DDI_CTLOPS_POKE);
}

void
i_ddi_caut_put16(ddi_acc_impl_t *hp, uint16_t *addr, uint16_t value)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)&value, (uintptr_t)addr,
	    sizeof (uint16_t), 1, 0, DDI_CTLOPS_POKE);
}

void
i_ddi_caut_put32(ddi_acc_impl_t *hp, uint32_t *addr, uint32_t value)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)&value, (uintptr_t)addr,
	    sizeof (uint32_t), 1, 0, DDI_CTLOPS_POKE);
}

void
i_ddi_caut_put64(ddi_acc_impl_t *hp, uint64_t *addr, uint64_t value)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)&value, (uintptr_t)addr,
	    sizeof (uint64_t), 1, 0, DDI_CTLOPS_POKE);
}

void
i_ddi_caut_rep_get8(ddi_acc_impl_t *hp, uint8_t *host_addr,
    uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)host_addr, (uintptr_t)dev_addr,
	    sizeof (uint8_t), repcount, flags, DDI_CTLOPS_PEEK);
}

void
i_ddi_caut_rep_get16(ddi_acc_impl_t *hp, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)host_addr, (uintptr_t)dev_addr,
	    sizeof (uint16_t), repcount, flags, DDI_CTLOPS_PEEK);
}

void
i_ddi_caut_rep_get32(ddi_acc_impl_t *hp, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)host_addr, (uintptr_t)dev_addr,
	    sizeof (uint32_t), repcount, flags, DDI_CTLOPS_PEEK);
}

void
i_ddi_caut_rep_get64(ddi_acc_impl_t *hp, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)host_addr, (uintptr_t)dev_addr,
	    sizeof (uint64_t), repcount, flags, DDI_CTLOPS_PEEK);
}

void
i_ddi_caut_rep_put8(ddi_acc_impl_t *hp, uint8_t *host_addr,
    uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)host_addr, (uintptr_t)dev_addr,
	    sizeof (uint8_t), repcount, flags, DDI_CTLOPS_POKE);
}

void
i_ddi_caut_rep_put16(ddi_acc_impl_t *hp, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)host_addr, (uintptr_t)dev_addr,
	    sizeof (uint16_t), repcount, flags, DDI_CTLOPS_POKE);
}

void
i_ddi_caut_rep_put32(ddi_acc_impl_t *hp, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)host_addr, (uintptr_t)dev_addr,
	    sizeof (uint32_t), repcount, flags, DDI_CTLOPS_POKE);
}

void
i_ddi_caut_rep_put64(ddi_acc_impl_t *hp, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	i_ddi_caut_getput_ctlops(hp, (uintptr_t)host_addr, (uintptr_t)dev_addr,
	    sizeof (uint64_t), repcount, flags, DDI_CTLOPS_POKE);
}

int
i_ddi_add_softint(ddi_softint_hdl_impl_t *hdlp)
{
	int ret;

	ret = add_avsoftintr((void *)hdlp, hdlp->ih_pri, hdlp->ih_cb_func,
	    DEVI(hdlp->ih_dip)->devi_name, hdlp->ih_cb_arg1, hdlp->ih_cb_arg2);
	return (ret ? DDI_SUCCESS : DDI_FAILURE);
}

void
i_ddi_remove_softint(ddi_softint_hdl_impl_t *hdlp)
{
	(void) rem_avsoftintr((void *)hdlp, hdlp->ih_pri, hdlp->ih_cb_func);
}

int
i_ddi_intr_ops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_op_t op,
    ddi_intr_handle_impl_t *hdlp, void * result)
{
	dev_info_t	*pdip = (dev_info_t *)DEVI(dip)->devi_parent;
	int		ret = DDI_FAILURE;

	if (NEXUS_HAS_INTR_OP(pdip))
		ret = (*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_intr_op))(
		    pdip, rdip, op, hdlp, result);
	else
		cmn_err(CE_WARN, "Failed to process interrupt "
		    "for %s%d due to down-rev nexus driver %s%d",
		    ddi_get_name(rdip), ddi_get_instance(rdip),
		    ddi_get_name(pdip), ddi_get_instance(pdip));
	return (ret);
}

extern void (*setsoftint)(int, struct av_softinfo *);
extern boolean_t av_check_softint_pending(struct av_softinfo *, boolean_t);

int
i_ddi_trigger_softint(ddi_softint_hdl_impl_t *hdlp, void *arg2)
{
	if (av_check_softint_pending(hdlp->ih_pending, B_FALSE))
		return (DDI_EPENDING);

	update_avsoftintr_args((void *)hdlp, hdlp->ih_pri, arg2);

	(*setsoftint)(hdlp->ih_pri, hdlp->ih_pending);
	return (DDI_SUCCESS);
}

int
i_ddi_set_softint_pri(ddi_softint_hdl_impl_t *hdlp, uint_t old_pri)
{
	int ret;

	if (av_check_softint_pending(hdlp->ih_pending, B_TRUE))
		return (DDI_FAILURE);

	ret = av_softint_movepri((void *)hdlp, old_pri);
	return (ret ? DDI_SUCCESS : DDI_FAILURE);
}

void
i_ddi_alloc_intr_phdl(ddi_intr_handle_impl_t *hdlp)
{
	hdlp->ih_private = (void *)kmem_zalloc(sizeof (ihdl_plat_t), KM_SLEEP);
}

void
i_ddi_free_intr_phdl(ddi_intr_handle_impl_t *hdlp)
{
	kmem_free(hdlp->ih_private, sizeof (ihdl_plat_t));
	hdlp->ih_private = NULL;
}

int
i_ddi_get_intx_nintrs(dev_info_t *dip)
{
	int intrlen;
	int intr_sz;
	int *ip;
	int ret = 0;

	if (ddi_getlongprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS |
	    DDI_PROP_CANSLEEP,
	    "interrupts", (caddr_t)&ip, &intrlen) == DDI_SUCCESS) {
		intr_sz = ddi_getprop(DDI_DEV_T_ANY, dip,
		    0, "#interrupt-cells", 1);
		/* adjust for number of bytes */
		intr_sz = CELLS_1275_TO_BYTES(intr_sz);

		ret = intrlen / intr_sz;

		kmem_free(ip, intrlen);
	}

	return (ret);
}

void
i_ddi_intr_redist_all_cpus()
{
	/* nothing (yet) */
}

uint_t
impl_assign_instance(dev_info_t *dip)
{
	return ((uint_t)-1);
}

int
impl_keep_instance(dev_info_t *dip)
{
	return (DDI_FAILURE);
}

int
impl_free_instance(dev_info_t *dip)
{
	return (DDI_FAILURE);
}

int
impl_check_cpu(dev_info_t *devi)
{
	return (DDI_SUCCESS);
}

void
impl_fix_props(dev_info_t *dip, dev_info_t *ch_dip, char *name,
    int len, caddr_t buffer)
{
	/* nothing (yet) */
}

static int
get_prop_int_array(dev_info_t *di, char *pname, int **pval, uint_t *plen)
{
	int ret;

	if ((ret = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, di,
	    DDI_PROP_DONTPASS, pname, pval, plen))
	    == DDI_PROP_SUCCESS) {
		*plen = CELLS_1275_TO_BYTES(*plen);
	}
	return (ret);
}


static int
get_address_cells(pnode_t node)
{
	int address_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, "#address-cells");
		if (len > 0) {
			ASSERT(len == sizeof (int));
			int prop;
			prom_getprop(node, "#address-cells", (caddr_t)&prop);
			address_cells = ntohl(prop);
			break;
		}
		node = prom_parentnode(node);
	}
	return (address_cells);
}

static int
get_size_cells(pnode_t node)
{
	int size_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, "#size-cells");
		if (len > 0) {
			ASSERT(len == sizeof (int));
			int prop;
			prom_getprop(node, "#size-cells", (caddr_t)&prop);
			size_cells = ntohl(prop);
			break;
		}
		node = prom_parentnode(node);
	}
	return (size_cells);
}

/*
 * init_regspec_64:
 *
 * If the parent #size-cells is 2, convert the upa-style or
 * safari-style reg property from 2-size cells to 1 size cell
 * format, ignoring the size_hi, which must be zero for devices.
 * (It won't be zero in the memory list properties in the memory
 * nodes, but that doesn't matter here.)
 *
 * While we have no UPA or Safari, we do have roots with #size-cells == 2,
 * notably QEMU's "virt" machine.
 */
struct ddi_parent_private_data *
init_regspec_64(dev_info_t *dip)
{
	struct ddi_parent_private_data *pd;
	dev_info_t *parent;
	int size_cells;

	/*
	 * If there are no "reg"s in the child node, return.
	 */
	pd = ddi_get_parent_data(dip);
	if ((pd == NULL) || (pd->par_nreg == 0)) {
		return (pd);
	}
	parent = ddi_get_parent(dip);

	size_cells = ddi_prop_get_int(DDI_DEV_T_ANY, parent,
	    DDI_PROP_DONTPASS, "#size-cells", 1);

	if (size_cells != 1)  {
		int n, j;
		struct regspec *irp;
		struct reg_64 {
			uint_t addr_hi, addr_lo, size_hi, size_lo;
		};
		struct reg_64 *r64_rp;
		struct regspec *rp;
		uint_t len = 0;
		int *reg_prop;

		VERIFY(size_cells == 2);

		/*
		 * We already looked the property up once before if
		 * pd is non-NULL.
		 */
		(void) ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip,
		    DDI_PROP_DONTPASS, OBP_REG, &reg_prop, &len);

		VERIFY(len != 0);

		n = BYTES_TO_1275_CELLS(sizeof (struct reg_64));
		n = len / n;

		/*
		 * We're allocating a buffer the size of the PROM's property,
		 * but we're only using a smaller portion when we assign it
		 * to a regspec.  We do this so that in the
		 * impl_ddi_sunbus_removechild function, we will
		 * always free the right amount of memory.
		 */
		irp = rp = (struct regspec *)reg_prop;
		r64_rp = (struct reg_64 *)pd->par_reg;

		for (j = 0; j < n; ++j, ++rp, ++r64_rp) {
			if (r64_rp->size_hi != 0) {
				/* XXXROOTNEX */
				dev_err(dip, CE_WARN, "requires "
				    "64-bit register size");
			}
			rp->regspec_bustype = r64_rp->addr_hi;
			rp->regspec_addr = r64_rp->addr_lo;
			rp->regspec_size = r64_rp->size_lo;
		}

		ddi_prop_free(pd->par_reg);
		pd->par_nreg = n;
		pd->par_reg = irp;
	}
	return (pd);
}

static int
get_interrupt_cells(pnode_t node)
{
	int interrupt_cells = 0;

	while (node > 0) {
		int len = prom_getproplen(node, "#interrupt-cells");
		if (len > 0) {
			ASSERT(len == sizeof (int));
			int prop;
			prom_getprop(node, "#interrupt-cells", (caddr_t)&prop);
			interrupt_cells = ntohl(prop);
			break;
		}
		len = prom_getproplen(node, "interrupt-parent");
		if (len > 0) {
			ASSERT(len == sizeof (int));
			int prop;
			prom_getprop(node, "interrupt-parent", (caddr_t)&prop);
			node = prom_findnode_by_phandle(ntohl(prop));
			continue;
		}
		node = prom_parentnode(node);
	}
	return (interrupt_cells);
}

/*
 * For armv8, we're prepared to claim that the interrupt string is either in
 * the form of a list of <vec> specifications
 *
 * or the arm,gic format: a list of <type, vec, flags> specifications
 */

#define	VEC_MIN	0
#define	VEC_MAX	987

static int
impl_xlate_intrs(dev_info_t *child, int *in,
    struct ddi_parent_private_data *pdptr)
{
	size_t size;
	int n;
	struct intrspec *new;
	caddr_t got_prop;
	int *inpri = NULL;
	int got_len;

	static char bad_intr_fmt[] =
	    "bad interrupt spec from %s%d - ipl %d, irq %d\n";


	int interrupt_cells =
	    get_interrupt_cells(ddi_get_nodeid(child));

	/*
	 * XXXROOTNEX:
	 * 1 == normal <vec>
	 * 3 == arm,gic <type, vec, flags>
	 *
	 * There's no reason, I don't think, that someone couldn't use a 1 or
	 * 3-tuple to a different end.  We should probably be checking the
	 * node more carefully.
	 */
	VERIFY(interrupt_cells == 1 || interrupt_cells == 3);

	/*
	 * We only support output of the "new style" property, a list of <vec>
	 */

	/*
	 * The input list consists of elements a vector of multiples of
	 * #interrupt-cells, with the total number of vector elements in [0].
	 */
	if ((n = (*in++)) < 1)
		return (DDI_FAILURE);

	/* ...though it isn't aware of #interrupt-cells */
	n /= interrupt_cells;

	pdptr->par_nintr = n;
	size = n * sizeof (struct intrspec);
	new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

	if (ddi_getlongprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
	    "interrupt-priorities", (caddr_t)&got_prop, &got_len)
	    == DDI_PROP_SUCCESS) {
		if (n != BYTES_TO_1275_CELLS(got_len)) {
			cmn_err(CE_CONT,
			    "bad interrupt-priorities length"
			    " from %s%d: expected %d, got %d\n",
			    DEVI(child)->devi_name,
			    DEVI(child)->devi_instance, n,
			    (int)BYTES_TO_1275_CELLS(got_len));
			goto broken;
		}
		inpri = (int *)got_prop;
	}

	while (n--) {
		uint32_t type, vec, flags;
		uint32_t level = 5;

		if (interrupt_cells == 3) {
			type = *in++;
			vec = *in++;
			flags = *in++;
		} else {
			type = 0;
			vec = *in++;
			flags = 4;
		}

		switch (type) {
		case 0:
			vec = 32 + vec;
			break;
		case 1:
			vec = 16 + vec;
			break;
		default:
			dev_err(child, CE_PANIC, "Unknown interrupts entry "
			    "type 0x%x\n", type);
		}

		if (inpri == NULL)
			level = 5;
		else
			level = *inpri++;

		if (level < 1 || level > MAXIPL ||
		    vec < VEC_MIN || vec > VEC_MAX) {
			cmn_err(CE_CONT, bad_intr_fmt,
			    DEVI(child)->devi_name,
			    DEVI(child)->devi_instance, level, vec);
			goto broken;
		}

		new->intrspec_pri = level;
		new->intrspec_vec = vec;
		new->intrspec_cfg = flags;
		new++;
	}

	if (inpri != NULL)
		kmem_free(got_prop, got_len);
	return (DDI_SUCCESS);

broken:
	kmem_free(pdptr->par_intr, size);
	pdptr->par_intr = NULL;
	pdptr->par_nintr = 0;
	if (inpri != NULL)
		kmem_free(got_prop, got_len);

	return (DDI_FAILURE);
}

/*
 * Create a ddi_parent_private_data structure from the ddi properties of
 * the dev_info node.
 *
 * The "reg" and "interrupts" properties are required
 * if the driver wishes to create mappings or field interrupts on behalf
 * of the device.
 *
 * The "reg" property is in a firmware-defined shape, and converted into
 * `struct regspec`.
 *
 * The "interrupts" property is in firmware-defined shap, and converted into
 * `struct intrspec`
 */
void
make_ddi_ppd(dev_info_t *child, struct ddi_parent_private_data **ppd)
{
	struct ddi_parent_private_data *pdptr;
	int n;
	int *reg_prop, *rng_prop, *irupts_prop;
	uint_t reg_len, rng_len, irupts_len;
	dev_info_t *parent;
	int parent_addr_cells, parent_size_cells;
	int child_addr_cells, child_size_cells;

	*ppd = pdptr = kmem_zalloc(sizeof (*pdptr), KM_SLEEP);

	/*
	 * Handle the 'reg' property.
	 *
	 * XXXROOTNEX: The information here is invalid until `init_regspec_64`
	 * has done its thing.  I have to confess I don't understand why this
	 * is done in two passes, but the approach I've taken is from SPARC.
	 */
	if ((get_prop_int_array(child, "reg", &reg_prop, &reg_len) ==
	    DDI_PROP_SUCCESS) && (reg_len != 0)) {
		pdptr->par_nreg = reg_len / sizeof (struct regspec);
		pdptr->par_reg = (struct regspec *)reg_prop;
	}

	/*
	 * Handle the 'interrupts' property
	 */
	if (get_prop_int_array(child, "interrupts", &irupts_prop,
	    &irupts_len) != DDI_PROP_SUCCESS) {
		irupts_len = 0;
	}

	if ((n = irupts_len) != 0) {
		size_t size;
		int *out;

		/*
		 * Translate the 'interrupts' property into an array
		 * of intrspecs for the rest of the DDI framework to
		 * toy with.  Only our ancestors really know how to
		 * do this, so ask 'em.  We massage the 'interrupts'
		 * property so that it is pre-pended by a count of
		 * the number of integers in the argument.
		 */
		size = n + sizeof (int);
		out = kmem_alloc(size, KM_SLEEP);
		*out = BYTES_TO_1275_CELLS(n);
		bcopy(irupts_prop, out + 1, (size_t)n);
		ddi_prop_free((void *)irupts_prop);
		if (impl_xlate_intrs(child, out, pdptr) != DDI_SUCCESS) {
			dev_err(child, CE_CONT,
			    "Unable to translate 'interrupts'\n");
		}
		kmem_free(out, size);
	}

	/*
	 * root node has no parent ranges
	 * XXXROOTNEX: Triple check we don't have to initial /'s ppd by hand
	 */
	if ((parent = ddi_get_parent(child)) == NULL) {
		dev_err(child, CE_CONT, "skipping root node's ranges\n");
		return;
	}

	child_addr_cells = ddi_prop_get_int(DDI_DEV_T_ANY, child,
	    DDI_PROP_DONTPASS, "#address-cells", 2);
	child_size_cells = ddi_prop_get_int(DDI_DEV_T_ANY, child,
	    DDI_PROP_DONTPASS, "#size-cells", 1);
	parent_addr_cells = ddi_prop_get_int(DDI_DEV_T_ANY, parent,
	    DDI_PROP_DONTPASS, "#address-cells", 2);
	parent_size_cells = ddi_prop_get_int(DDI_DEV_T_ANY, parent,
	    DDI_PROP_DONTPASS, "#size-cells", 1);

	if (get_prop_int_array(child, "ranges", &rng_prop, &rng_len)
	    == DDI_PROP_SUCCESS) {
		if (child_addr_cells != 2 || parent_addr_cells != 2 ||
		    child_size_cells != 1 || parent_size_cells != 1) {
			/*
			 * XXXROOTNEX: This was much quieter on other platforms
			 */
			dev_err(child, CE_WARN, "ranges not made in parent data; "
			    "#address-cells or #size-cells have non-default values\n"
			    "\tparent: #address-cells = %d, #size-cells = %d\n"
			    "\tchild: #address-cells = %d, #size-cells = %d",
			    parent_addr_cells, parent_size_cells,
			    child_addr_cells, child_size_cells);
			return;
		}

		pdptr->par_nrng = rng_len / (sizeof (struct rangespec));
		pdptr->par_rng = (struct rangespec *)rng_prop;
	}
}

static int
impl_sunbus_name_child(dev_info_t *child, char *name, int namelen)
{
	/*
	 * Fill in parent-private data and this function returns to us
	 * an indication if it used "registers" to fill in the data.
	 */
	if (ddi_get_parent_data(child) == NULL) {
		struct ddi_parent_private_data *pdptr;
		make_ddi_ppd(child, &pdptr);
		ddi_set_parent_data(child, pdptr);
	}

	name[0] = '\0';

	if (sparc_pd_getnreg(child) > 0) {
		(void) snprintf(name, namelen, "%x,%x",
		    (uint_t)sparc_pd_getreg(child, 0)->regspec_bustype,
		    (uint_t)sparc_pd_getreg(child, 0)->regspec_addr);
	}

	return (DDI_SUCCESS);
}

int
impl_ddi_sunbus_initchild(dev_info_t *child)
{
	void impl_ddi_sunbus_removechild(dev_info_t *);
	char name[MAXNAMELEN] = {0};

	impl_sunbus_name_child(child, name, MAXNAMELEN);
	ddi_set_name_addr(child, name);

	if ((ndi_dev_is_persistent_node(child) == 0) &&
	    (ndi_merge_node(child, impl_sunbus_name_child) == DDI_SUCCESS)) {
		impl_ddi_sunbus_removechild(child);
		return (DDI_FAILURE);
	}

	(void) init_regspec_64(child);

	return (DDI_SUCCESS);
}

void
impl_free_ddi_ppd(dev_info_t *dip)
{
	struct ddi_parent_private_data *pdptr;
	size_t n;

	if ((pdptr = ddi_get_parent_data(dip)) == NULL)
		return;

	if ((n = (size_t)pdptr->par_nintr) != 0)
		kmem_free(pdptr->par_intr, n * sizeof (struct intrspec));

	if ((n = (size_t)pdptr->par_nrng) != 0)
		ddi_prop_free((void *)pdptr->par_rng);

	if ((n = pdptr->par_nreg) != 0)
		ddi_prop_free((void *)pdptr->par_reg);

	kmem_free(pdptr, sizeof (*pdptr));
	ddi_set_parent_data(dip, NULL);
}

void
impl_ddi_sunbus_removechild(dev_info_t *dip)
{
	impl_free_ddi_ppd(dip);
	ddi_set_name_addr(dip, NULL);
	impl_rem_dev_props(dip);
}

/*
 * Copy name to property_name, since name
 * is in the low address range below kernelbase.
 */
static void
copy_boot_str(const char *boot_str, char *kern_str, int len)
{
	int i = 0;

	while (i < len - 1 && boot_str[i] != '\0') {
		kern_str[i] = boot_str[i];
		i++;
	}

	kern_str[i] = 0;	/* null terminate */
	if (boot_str[i] != '\0')
		cmn_err(CE_WARN,
		    "boot property string is truncated to %s", kern_str);
}

static void
get_boot_properties(void)
{
	extern char hw_provider[];
	dev_info_t *devi;
	char *name;
	int length, flags;
	char property_name[50], property_val[50];
	void *bop_staging_area;

	bop_staging_area = kmem_zalloc(MMU_PAGESIZE, KM_NOSLEEP);

	/*
	 * Import "root" properties from the boot.
	 *
	 * We do this by invoking BOP_NEXTPROP until the list
	 * is completely copied in.
	 */

	devi = ddi_root_node();
	for (name = BOP_NEXTPROP(bootops, "");		/* get first */
	    name;					/* NULL => DONE */
	    name = BOP_NEXTPROP(bootops, name)) {	/* get next */

		/* copy string to memory above kernelbase */
		copy_boot_str(name, property_name, 50);

		/*
		 * Skip vga properties. They will be picked up later
		 * by get_vga_properties.
		 */
		if (strcmp(property_name, "display-edif-block") == 0 ||
		    strcmp(property_name, "display-edif-id") == 0) {
			continue;
		}

		length = BOP_GETPROPLEN(bootops, property_name);
		if (length < 0)
			continue;
		if (length > MMU_PAGESIZE) {
			cmn_err(CE_NOTE,
			    "boot property %s longer than 0x%lx, ignored\n",
			    property_name, MMU_PAGESIZE);
			continue;
		}
		BOP_GETPROP(bootops, property_name, bop_staging_area);
		flags = do_bsys_getproptype(bootops, property_name);

		/*
		 * special properties:
		 * si-machine, si-hw-provider
		 *	goes to kernel data structures.
		 * bios-boot-device and stdout
		 *	goes to hardware property list so it may show up
		 *	in the prtconf -vp output. This is needed by
		 *	Install/Upgrade. Once we fix install upgrade,
		 *	this can be taken out.
		 * XXXARM: It's unclear whether we need bios-boot-device.
		 */
		if (strcmp(name, "si-machine") == 0) {
			(void) strncpy(utsname.machine, bop_staging_area,
			    SYS_NMLN);
			utsname.machine[SYS_NMLN - 1] = '\0';
			continue;
		}
		if (strcmp(name, "si-hw-provider") == 0) {
			(void) strncpy(hw_provider, bop_staging_area, SYS_NMLN);
			hw_provider[SYS_NMLN - 1] = '\0';
			continue;
		}
		if (strcmp(name, "bios-boot-device") == 0) {
			copy_boot_str(bop_staging_area, property_val, 50);
			(void) ndi_prop_update_string(DDI_DEV_T_NONE, devi,
			    property_name, property_val);
			continue;
		}
		if (strcmp(name, "stdout") == 0) {
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, devi,
			    property_name, *((int *)bop_staging_area));
			continue;
		}

		/* Boolean property */
		if (length == 0) {
			(void) e_ddi_prop_create(DDI_DEV_T_NONE, devi,
			    DDI_PROP_CANSLEEP, property_name, NULL, 0);
			continue;
		}

		/* Now anything else based on type. */
		switch (flags) {
		case DDI_PROP_TYPE_INT:
			if (length == sizeof (int)) {
				(void) e_ddi_prop_update_int(DDI_DEV_T_NONE,
				    devi, property_name,
				    *((int *)bop_staging_area));
			} else {
				(void) e_ddi_prop_update_int_array(
				    DDI_DEV_T_NONE, devi, property_name,
				    bop_staging_area,
				    BYTES_TO_1275_CELLS(length));
			}
			break;
		case DDI_PROP_TYPE_STRING:
			(void) e_ddi_prop_update_string(DDI_DEV_T_NONE, devi,
			    property_name, bop_staging_area);
			break;
		case DDI_PROP_TYPE_BYTE:
			(void) e_ddi_prop_update_byte_array(DDI_DEV_T_NONE,
			    devi, property_name, bop_staging_area, length);
			break;
		case DDI_PROP_TYPE_INT64:
			if (length == sizeof (int64_t)) {
				(void) e_ddi_prop_update_int64(DDI_DEV_T_NONE,
				    devi, property_name,
				    *((int64_t *)bop_staging_area));
			} else {
				(void) e_ddi_prop_update_int64_array(
				    DDI_DEV_T_NONE, devi, property_name,
				    bop_staging_area,
				    length / sizeof (int64_t));
			}
			break;
		default:
			/* Property type unknown, use old prop interface */
			(void) e_ddi_prop_create(DDI_DEV_T_NONE, devi,
			    DDI_PROP_CANSLEEP, property_name, bop_staging_area,
			    length);
		}
	}

	kmem_free(bop_staging_area, MMU_PAGESIZE);
}

void
impl_setup_ddi(void)
{
	dev_info_t *xdip, *isa_dip;
	rd_existing_t rd_mem_prop;
	int err;

	ndi_devi_alloc_sleep(
	    ddi_root_node(), "ramdisk", (pnode_t)DEVI_SID_NODEID, &xdip);
	(void) BOP_GETPROP(bootops, "ramdisk_start", (void *)&ramdisk_start);
	(void) BOP_GETPROP(bootops, "ramdisk_end", (void *)&ramdisk_end);

	rd_mem_prop.phys = ramdisk_start;
	rd_mem_prop.size = ramdisk_end - ramdisk_start + 1;

	ndi_prop_update_byte_array(DDI_DEV_T_NONE, xdip, RD_EXISTING_PROP_NAME,
	    (uchar_t *)&rd_mem_prop, sizeof (rd_mem_prop));
	err = ndi_devi_bind_driver(xdip, 0);
	ASSERT(err == 0);

	/*
	 * Read in the properties from the boot.
	 */
	get_boot_properties();

	/* do bus dependent probes. */
	impl_bus_initialprobe();
}


/*
 * DDI Memory/DMA
 */

/*
 * Support for allocating DMAable memory to implement
 * ddi_dma_mem_alloc(9F) interface.
 */

#define	KA_ALIGN_SHIFT	7
#define	KA_ALIGN	(1 << KA_ALIGN_SHIFT)
#define	KA_NCACHE	(PAGESHIFT + 1 - KA_ALIGN_SHIFT)

/*
 * Dummy DMA attribute template for kmem_io[].kmem_io_attr.  We only
 * care about addr_lo, addr_hi, and align.  addr_hi will be dynamically set.
 */

static ddi_dma_attr_t kmem_io_attr = {
	DMA_ATTR_V0,
	0x0000000000000000ULL,		/* dma_attr_addr_lo */
	0x0000000000000000ULL,		/* dma_attr_addr_hi */
	0x00ffffff,
	0x1000,				/* dma_attr_align */
	1, 1, 0xffffffffULL, 0xffffffffULL, 0x1, 1, 0
};

/* kmem io memory ranges and indices */
enum {
	IO_4P, IO_64G, IO_4G, IO_2G, IO_1G, IO_512M,
	IO_256M, IO_128M, IO_64M, IO_32M, IO_16M, MAX_MEM_RANGES
};

static struct {
	vmem_t		*kmem_io_arena;
	kmem_cache_t	*kmem_io_cache[KA_NCACHE];
	ddi_dma_attr_t	kmem_io_attr;
} kmem_io[MAX_MEM_RANGES];

static int kmem_io_idx;		/* index of first populated kmem_io[] */

static page_t *
page_create_io_wrapper(void *addr, size_t len, int vmflag, void *arg)
{
	extern page_t *page_create_io(vnode_t *, u_offset_t, uint_t,
	    uint_t, struct as *, caddr_t, ddi_dma_attr_t *);

	return (page_create_io(&kvp, (u_offset_t)(uintptr_t)addr, len,
	    PG_EXCL | ((vmflag & VM_NOSLEEP) ? 0 : PG_WAIT), &kas, addr, arg));
}

static void *
segkmem_alloc_io_4P(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_4P].kmem_io_attr));
}

static void *
segkmem_alloc_io_64G(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_64G].kmem_io_attr));
}

static void *
segkmem_alloc_io_4G(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_4G].kmem_io_attr));
}

static void *
segkmem_alloc_io_2G(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_2G].kmem_io_attr));
}

static void *
segkmem_alloc_io_1G(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_1G].kmem_io_attr));
}

static void *
segkmem_alloc_io_512M(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_512M].kmem_io_attr));
}

static void *
segkmem_alloc_io_256M(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_256M].kmem_io_attr));
}

static void *
segkmem_alloc_io_128M(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_128M].kmem_io_attr));
}

static void *
segkmem_alloc_io_64M(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_64M].kmem_io_attr));
}

static void *
segkmem_alloc_io_32M(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_32M].kmem_io_attr));
}

static void *
segkmem_alloc_io_16M(vmem_t *vmp, size_t size, int vmflag)
{
	return (segkmem_xalloc(vmp, NULL, size, vmflag, 0,
	    page_create_io_wrapper, &kmem_io[IO_16M].kmem_io_attr));
}

struct {
	uint64_t	io_limit;
	char		*io_name;
	void		*(*io_alloc)(vmem_t *, size_t, int);
	int		io_initial;	/* kmem_io_init during startup */
} io_arena_params[MAX_MEM_RANGES] = {
	{0x000fffffffffffffULL,	"kmem_io_4P",	segkmem_alloc_io_4P,	1},
	{0x0000000fffffffffULL,	"kmem_io_64G",	segkmem_alloc_io_64G,	0},
	{0x00000000ffffffffULL,	"kmem_io_4G",	segkmem_alloc_io_4G,	1},
	{0x000000007fffffffULL,	"kmem_io_2G",	segkmem_alloc_io_2G,	1},
	{0x000000003fffffffULL,	"kmem_io_1G",	segkmem_alloc_io_1G,	0},
	{0x000000001fffffffULL,	"kmem_io_512M",	segkmem_alloc_io_512M,	0},
	{0x000000000fffffffULL,	"kmem_io_256M",	segkmem_alloc_io_256M,	0},
	{0x0000000007ffffffULL,	"kmem_io_128M",	segkmem_alloc_io_128M,	0},
	{0x0000000003ffffffULL,	"kmem_io_64M",	segkmem_alloc_io_64M,	0},
	{0x0000000001ffffffULL,	"kmem_io_32M",	segkmem_alloc_io_32M,	0},
	{0x0000000000ffffffULL,	"kmem_io_16M",	segkmem_alloc_io_16M,	1}
};

void
kmem_io_init(int a)
{
	int	c;
	char name[40];

	kmem_io[a].kmem_io_arena = vmem_create(io_arena_params[a].io_name,
	    NULL, 0, PAGESIZE, io_arena_params[a].io_alloc,
	    segkmem_free,
	    heap_arena, 0, VM_SLEEP);

	for (c = 0; c < KA_NCACHE; c++) {
		size_t size = KA_ALIGN << c;
		(void) sprintf(name, "%s_%lu",
		    io_arena_params[a].io_name, size);
		kmem_io[a].kmem_io_cache[c] = kmem_cache_create(name,
		    size, size, NULL, NULL, NULL, NULL,
		    kmem_io[a].kmem_io_arena, 0);
	}
}

/*
 * Return the index of the highest memory range for addr.
 */
static int
kmem_io_index(uint64_t addr)
{
	int n;

	for (n = kmem_io_idx; n < MAX_MEM_RANGES; n++) {
		if (kmem_io[n].kmem_io_attr.dma_attr_addr_hi <= addr) {
			if (kmem_io[n].kmem_io_arena == NULL)
				kmem_io_init(n);
			return (n);
		}
	}
	panic("kmem_io_index: invalid addr - must be at least 16m");

	/*NOTREACHED*/
}

/*
 * Return the index of the next kmem_io populated memory range
 * after curindex.
 */
static int
kmem_io_index_next(int curindex)
{
	int n;

	for (n = curindex + 1; n < MAX_MEM_RANGES; n++) {
		if (kmem_io[n].kmem_io_arena)
			return (n);
	}
	return (-1);
}

/*
 * allow kmem to be mapped in with different PTE cache attribute settings.
 * Used by i_ddi_mem_alloc()
 */
int
kmem_override_cache_attrs(caddr_t kva, size_t size, uint_t order)
{
	uint_t hat_flags;
	caddr_t kva_end;
	uint_t hat_attr;
	pfn_t pfn;

	if (hat_getattr(kas.a_hat, kva, &hat_attr) == -1) {
		return (-1);
	}

	hat_attr &= ~HAT_ORDER_MASK;
	hat_attr |= order | HAT_NOSYNC;
	hat_flags = HAT_LOAD_LOCK;

	kva_end = (caddr_t)(((uintptr_t)kva + size + PAGEOFFSET) &
	    (uintptr_t)PAGEMASK);
	kva = (caddr_t)((uintptr_t)kva & (uintptr_t)PAGEMASK);

	while (kva < kva_end) {
		pfn = hat_getpfnum(kas.a_hat, kva);
		hat_unload(kas.a_hat, kva, PAGESIZE, HAT_UNLOAD_UNLOCK);
		hat_devload(kas.a_hat, kva, PAGESIZE, pfn, hat_attr, hat_flags);
		kva += MMU_PAGESIZE;
	}

	return (0);
}

static int
ctgcompare(const void *a1, const void *a2)
{
	/* we just want to compare virtual addresses */
	a1 = ((struct ctgas *)a1)->ctg_addr;
	a2 = ((struct ctgas *)a2)->ctg_addr;
	return (a1 == a2 ? 0 : (a1 < a2 ? -1 : 1));
}

void
ka_init(void)
{
	int a;
	paddr_t maxphysaddr;
	extern pfn_t physmax;

	maxphysaddr = mmu_ptob((paddr_t)physmax) + MMU_PAGEOFFSET;

	ASSERT(maxphysaddr <= io_arena_params[0].io_limit);

	for (a = 0; a < MAX_MEM_RANGES; a++) {
		if (maxphysaddr >= io_arena_params[a + 1].io_limit) {
			if (maxphysaddr > io_arena_params[a + 1].io_limit)
				io_arena_params[a].io_limit = maxphysaddr;
			else
				a++;
			break;
		}
	}
	kmem_io_idx = a;

	for (; a < MAX_MEM_RANGES; a++) {
		kmem_io[a].kmem_io_attr = kmem_io_attr;
		kmem_io[a].kmem_io_attr.dma_attr_addr_hi =
		    io_arena_params[a].io_limit;
		/*
		 * initialize kmem_io[] arena/cache corresponding to
		 * maxphysaddr and to the "common" io memory ranges that
		 * have io_initial set to a non-zero value.
		 */
		if (io_arena_params[a].io_initial || a == kmem_io_idx)
			kmem_io_init(a);
	}

	/* initialize ctgtree */
	avl_create(&ctgtree, ctgcompare, sizeof (struct ctgas),
	    offsetof(struct ctgas, ctg_link));
}

/*
 * put contig address/size
 */
static void *
putctgas(void *addr, size_t size)
{
	struct ctgas    *ctgp;
	if ((ctgp = kmem_zalloc(sizeof (*ctgp), KM_NOSLEEP)) != NULL) {
		ctgp->ctg_addr = addr;
		ctgp->ctg_size = size;
		CTGLOCK();
		avl_add(&ctgtree, ctgp);
		CTGUNLOCK();
	}
	return (ctgp);
}

/*
 * get contig size by addr
 */
static size_t
getctgsz(void *addr)
{
	struct ctgas    *ctgp;
	struct ctgas    find;
	size_t		sz = 0;

	find.ctg_addr = addr;
	CTGLOCK();
	if ((ctgp = avl_find(&ctgtree, &find, NULL)) != NULL) {
		avl_remove(&ctgtree, ctgp);
	}
	CTGUNLOCK();

	if (ctgp != NULL) {
		sz = ctgp->ctg_size;
		kmem_free(ctgp, sizeof (*ctgp));
	}

	return (sz);
}

/*
 * contig_alloc:
 *
 *	allocates contiguous memory to satisfy the 'size' and dma attributes
 *	specified in 'attr'.
 *
 *	Not all of memory need to be physically contiguous if the
 *	scatter-gather list length is greater than 1.
 */

/*ARGSUSED*/
static void *
contig_alloc(size_t size, ddi_dma_attr_t *attr, uintptr_t align, int cansleep)
{
	pgcnt_t		pgcnt = btopr(size);
	size_t		asize = pgcnt * PAGESIZE;
	page_t		*ppl;
	int		pflag;
	void		*addr;

	extern page_t *page_create_io(vnode_t *, u_offset_t, uint_t,
	    uint_t, struct as *, caddr_t, ddi_dma_attr_t *);

	/* segkmem_xalloc */

	if (align <= PAGESIZE)
		addr = vmem_alloc(heap_arena, asize,
		    (cansleep) ? VM_SLEEP : VM_NOSLEEP);
	else
		addr = vmem_xalloc(heap_arena, asize, align, 0, 0, NULL, NULL,
		    (cansleep) ? VM_SLEEP : VM_NOSLEEP);
	if (addr) {
		ASSERT(!((uintptr_t)addr & (align - 1)));

		if (page_resv(pgcnt, (cansleep) ? KM_SLEEP : KM_NOSLEEP) == 0) {
			vmem_free(heap_arena, addr, asize);
			return (NULL);
		}
		pflag = PG_EXCL;

		if (cansleep)
			pflag |= PG_WAIT;

		/* 4k req gets from freelists rather than pfn search */
		if (pgcnt > 1 || align > PAGESIZE)
			pflag |= PG_PHYSCONTIG;

		ppl = page_create_io(&kvp, (u_offset_t)(uintptr_t)addr,
		    asize, pflag, &kas, (caddr_t)addr, attr);

		if (!ppl) {
			vmem_free(heap_arena, addr, asize);
			page_unresv(pgcnt);
			return (NULL);
		}

		while (ppl != NULL) {
			page_t	*pp = ppl;
			page_sub(&ppl, pp);
			ASSERT(page_iolock_assert(pp));
			page_io_unlock(pp);
			page_downgrade(pp);
			hat_memload(kas.a_hat, (caddr_t)(uintptr_t)pp->p_offset,
			    pp, (PROT_ALL & ~PROT_USER) |
			    HAT_NOSYNC, HAT_LOAD_LOCK);
		}
	}
	return (addr);
}

static void
contig_free(void *addr, size_t size)
{
	pgcnt_t	pgcnt = btopr(size);
	size_t	asize = pgcnt * PAGESIZE;
	caddr_t	a, ea;
	page_t	*pp;

	hat_unload(kas.a_hat, addr, asize, HAT_UNLOAD_UNLOCK);

	for (a = addr, ea = a + asize; a < ea; a += PAGESIZE) {
		pp = page_find(&kvp, (u_offset_t)(uintptr_t)a);
		if (!pp)
			panic("contig_free: contig pp not found");

		if (!page_tryupgrade(pp)) {
			page_unlock(pp);
			pp = page_lookup(&kvp,
			    (u_offset_t)(uintptr_t)a, SE_EXCL);
			if (pp == NULL)
				panic("contig_free: page freed");
		}
		page_destroy(pp, 0);
	}

	page_unresv(pgcnt);
	vmem_free(heap_arena, addr, asize);
}

/*
 * Allocate from the system, aligned on a specific boundary.
 * The alignment, if non-zero, must be a power of 2.
 */
static void *
kalloca(size_t size, size_t align, int cansleep, int physcontig,
    ddi_dma_attr_t *attr)
{
	size_t *addr, *raddr, rsize;
	size_t hdrsize = 4 * sizeof (size_t);	/* must be power of 2 */
	int a, i, c;
	vmem_t *vmp;
	kmem_cache_t *cp = NULL;

	align = MAX(align, hdrsize);
	ASSERT((align & (align - 1)) == 0);

	/*
	 * All of our allocators guarantee 16-byte alignment, so we don't
	 * need to reserve additional space for the header.
	 * To simplify picking the correct kmem_io_cache, we round up to
	 * a multiple of KA_ALIGN.
	 */
	rsize = P2ROUNDUP_TYPED(size + align, KA_ALIGN, size_t);

	if (physcontig && rsize > PAGESIZE) {
		if (addr = contig_alloc(size, attr, align, cansleep)) {
			if (!putctgas(addr, size))
				contig_free(addr, size);
			else
				return (addr);
		}
		return (NULL);
	}

	a = kmem_io_index(attr->dma_attr_addr_hi);

	if (rsize > PAGESIZE) {
		vmp = kmem_io[a].kmem_io_arena;
		raddr = vmem_alloc(vmp, rsize,
		    (cansleep) ? VM_SLEEP : VM_NOSLEEP);
	} else {
		c = highbit((rsize >> KA_ALIGN_SHIFT) - 1);
		cp = kmem_io[a].kmem_io_cache[c];
		raddr = kmem_cache_alloc(cp, (cansleep) ? KM_SLEEP :
		    KM_NOSLEEP);
	}

	if (raddr == NULL) {
		int	na;

		ASSERT(cansleep == 0);
		if (rsize > PAGESIZE)
			return (NULL);
		/*
		 * System does not have memory in the requested range.
		 * Try smaller kmem io ranges and larger cache sizes
		 * to see if there might be memory available in
		 * these other caches.
		 */

		for (na = kmem_io_index_next(a); na >= 0;
		    na = kmem_io_index_next(na)) {
			ASSERT(kmem_io[na].kmem_io_arena);
			cp = kmem_io[na].kmem_io_cache[c];
			raddr = kmem_cache_alloc(cp, KM_NOSLEEP);
			if (raddr)
				goto kallocdone;
		}
		/* now try the larger kmem io cache sizes */
		for (na = a; na >= 0; na = kmem_io_index_next(na)) {
			for (i = c + 1; i < KA_NCACHE; i++) {
				cp = kmem_io[na].kmem_io_cache[i];
				raddr = kmem_cache_alloc(cp, KM_NOSLEEP);
				if (raddr)
					goto kallocdone;
			}
		}
		return (NULL);
	}

kallocdone:
	ASSERT(!P2BOUNDARY((uintptr_t)raddr, rsize, PAGESIZE) ||
	    rsize > PAGESIZE);

	addr = (size_t *)P2ROUNDUP((uintptr_t)raddr + hdrsize, align);
	ASSERT((uintptr_t)addr + size - (uintptr_t)raddr <= rsize);

	addr[-4] = (size_t)cp;
	addr[-3] = (size_t)vmp;
	addr[-2] = (size_t)raddr;
	addr[-1] = rsize;

	return (addr);
}

static void
kfreea(void *addr)
{
	size_t		size;

	if (!((uintptr_t)addr & PAGEOFFSET) && (size = getctgsz(addr))) {
		contig_free(addr, size);
	} else {
		size_t	*saddr = addr;
		if (saddr[-4] == 0)
			vmem_free((vmem_t *)saddr[-3], (void *)saddr[-2],
			    saddr[-1]);
		else
			kmem_cache_free((kmem_cache_t *)saddr[-4],
			    (void *)saddr[-2]);
	}
}

/*ARGSUSED*/
void
i_ddi_devacc_to_hatacc(ddi_device_acc_attr_t *devaccp, uint_t *hataccp)
{
}

/*
 * Check if the specified cache attribute is supported on the platform.
 * This function must be called before i_ddi_cacheattr_to_hatacc().
 */
boolean_t
i_ddi_check_cache_attr(uint_t flags)
{
	/*
	 * The cache attributes are mutually exclusive. Any combination of
	 * the attributes leads to a failure.
	 */
	uint_t cache_attr = IOMEM_CACHE_ATTR(flags);
	if ((cache_attr != 0) && !ISP2(cache_attr))
		return (B_FALSE);

	/* All cache attributes are supported on X86/X64 */
	if (cache_attr & (IOMEM_DATA_UNCACHED | IOMEM_DATA_CACHED |
	    IOMEM_DATA_UC_WR_COMBINE))
		return (B_TRUE);

	/* undefined attributes */
	return (B_FALSE);
}

/* set HAT cache attributes from the cache attributes */
void
i_ddi_cacheattr_to_hatacc(uint_t flags, uint_t *hataccp)
{
	uint_t cache_attr = IOMEM_CACHE_ATTR(flags);
	static char *fname = "i_ddi_cacheattr_to_hatacc";

	/*
	 * set HAT attrs according to the cache attrs.
	 */
	switch (cache_attr) {
	case IOMEM_DATA_UNCACHED:
		*hataccp &= ~HAT_ORDER_MASK;
		*hataccp |= (HAT_STRICTORDER | HAT_PLAT_NOCACHE);
		break;
	case IOMEM_DATA_UC_WR_COMBINE:
		*hataccp &= ~HAT_ORDER_MASK;
		*hataccp |= (HAT_MERGING_OK | HAT_PLAT_NOCACHE);
		break;
	case IOMEM_DATA_CACHED:
		*hataccp &= ~HAT_ORDER_MASK;
		*hataccp |= (HAT_STORECACHING_OK | HAT_PLAT_NOCACHE);
		break;
	/*
	 * This case must not occur because the cache attribute is scrutinized
	 * before this function is called.
	 */
	default:
		/*
		 * set cacheable to hat attrs.
		 */
		*hataccp &= ~HAT_ORDER_MASK;
		*hataccp |= HAT_STORECACHING_OK;
		cmn_err(CE_WARN, "%s: cache_attr=0x%x is ignored.",
		    fname, cache_attr);
	}
}

struct dma_range
{
	uint64_t cpu_addr;
	uint64_t bus_addr;
	size_t size;
};

static int
get_dma_ranges(dev_info_t *dip, struct dma_range **range, int *nrange)
{
	int dma_range_num = 0;
	struct dma_range *dma_ranges = NULL;
	boolean_t *update = NULL;
	int ret = DDI_SUCCESS;

	if (dip == NULL)
		goto err_exit;

	for (;;) {
		dip = ddi_get_parent(dip);
		if (dip == NULL)
			break;
		pnode_t node = ddi_get_nodeid(dip);
		if (node <= 0)
			break;
		if (prom_getproplen(node, "dma-ranges") <= 0)
			continue;

		int bus_address_cells;
		int bus_size_cells;
		int parent_address_cells;
		pnode_t parent;

		parent = prom_parentnode(node);
		if (parent <= 0) {
			cmn_err(CE_WARN,
			    "%s: root node has a dma-ranges property.",
			    __func__);
			goto err_exit;
		}

		bus_address_cells = get_address_cells(node);
		bus_size_cells = get_size_cells(node);
		parent_address_cells = get_address_cells(parent);

		int len = prom_getproplen(node, "dma-ranges");
		if (len % (sizeof (uint32_t) * (bus_address_cells +
		    parent_address_cells + bus_size_cells)) != 0) {
			cmn_err(CE_WARN,
			    "%s: dma-ranges property length is invalid\n"
			    "bus_address_cells %d\n"
			    "parent_address_cells %d\n"
			    "bus_size_cells %d\n"
			    "len %d\n",
			    __func__, bus_address_cells, parent_address_cells,
			    bus_size_cells, len);
			ret = DDI_FAILURE;
			goto err_exit;
		}
		int num = len / (sizeof (uint32_t) * (
		    bus_address_cells + parent_address_cells + bus_size_cells));
		uint32_t *cells = __builtin_alloca(len);
		prom_getprop(node, "dma-ranges", (caddr_t)cells);

		boolean_t first = (dma_ranges == NULL);
		if (first) {
			dma_range_num = num;
			dma_ranges = kmem_zalloc(
			    sizeof (struct dma_range) * dma_range_num,
			    KM_SLEEP);
			update = kmem_zalloc(
			    sizeof (boolean_t) * dma_range_num, KM_SLEEP);
		} else {
			memset(update, 0, sizeof (boolean_t) * dma_range_num);
		}

		for (int i = 0; i < num; i++) {
			uint64_t bus_address = 0;
			uint64_t parent_address = 0;
			uint64_t bus_size = 0;
			for (int j = 0; j < bus_address_cells; j++) {
				bus_address <<= 32;
				bus_address += ntohl(cells[(
				    bus_address_cells + parent_address_cells +
				    bus_size_cells) * i + j]);
			}
			for (int j = 0; j < parent_address_cells; j++) {
				parent_address <<= 32;
				parent_address += ntohl(
				    cells[(bus_address_cells +
				    parent_address_cells + bus_size_cells) *
				    i + bus_address_cells + j]);
			}
			for (int j = 0; j < bus_size_cells; j++) {
				bus_size <<= 32;
				bus_size += ntohl(cells[(bus_address_cells +
				    parent_address_cells + bus_size_cells) *
				    i + bus_address_cells +
				    parent_address_cells + j]);
			}

			if (first) {
				dma_ranges[i].cpu_addr = parent_address;
				dma_ranges[i].bus_addr = bus_address;
				dma_ranges[i].size = bus_size;
				update[i] = B_TRUE;
			} else {
				for (int j = 0; j < dma_range_num; j++) {
					if (bus_address <=
					    dma_ranges[j].cpu_addr &&
					    dma_ranges[j].cpu_addr +
					    dma_ranges[j].size - 1 <=
					    bus_address + bus_size - 1) {
						dma_ranges[j].cpu_addr +=
						    (parent_address -
						    bus_address);
						update[j] = B_TRUE;
						break;
					}
				}
			}
		}
		for (int i = 0; i < dma_range_num; i++) {
			if (!update[i]) {
				cmn_err(CE_WARN,
				    "%s: dma-ranges property is invalid",
				    __func__);
				ret = DDI_FAILURE;
				goto err_exit;
			}
		}
	}

	*nrange = dma_range_num;
	*range = dma_ranges;
err_exit:
	if (ret != DDI_SUCCESS && dma_ranges) {
		kmem_free(
		    dma_ranges, sizeof (struct dma_range) * dma_range_num);
	}
	if (update) {
		kmem_free(update, sizeof (boolean_t) * dma_range_num);
	}
	return (ret);
}

int
i_ddi_convert_dma_attr(
    ddi_dma_attr_t *dst, dev_info_t *dip, const ddi_dma_attr_t *src)
{
	*dst = *src;

	int dma_range_num = 0;
	struct dma_range *dma_ranges = NULL;
	int ret = get_dma_ranges(dip, &dma_ranges, &dma_range_num);
	if (ret != DDI_SUCCESS)
		return (DDI_FAILURE);

	if (dma_range_num > 0) {
		int i;
		for (i = 0; i < dma_range_num; i++) {
			if (dma_ranges[i].bus_addr <= dst->dma_attr_addr_lo &&
			    dst->dma_attr_addr_hi <=
			    dma_ranges[i].bus_addr + dma_ranges[i].size - 1) {
				dst->dma_attr_addr_lo +=
				    (dma_ranges[i].cpu_addr -
				    dma_ranges[i].bus_addr);
				dst->dma_attr_addr_hi +=
				    (dma_ranges[i].cpu_addr -
				    dma_ranges[i].bus_addr);
				break;
			}
		}
		if (i == dma_range_num) {
			cmn_err(CE_WARN,
			    "%s: ddi_dma_attr_t is invalid range", __func__);
			ret = DDI_FAILURE;
		}
	}

	if (dma_ranges) {
		kmem_free(
		    dma_ranges, sizeof (struct dma_range) * dma_range_num);
	}
	return (ret);
}

int
i_ddi_update_dma_attr(dev_info_t *dip, ddi_dma_attr_t *attr)
{
	int dma_range_num = 0;
	struct dma_range *dma_ranges = NULL;
	int ret = get_dma_ranges(dip, &dma_ranges, &dma_range_num);
	if (ret != DDI_SUCCESS)
		return (DDI_FAILURE);

	if (dma_range_num > 0) {
		int dma_range_index = 0;
		for (int i = 0; i < dma_range_num; i++) {
			if (dma_ranges[i].cpu_addr <
			    dma_ranges[dma_range_index].cpu_addr) {
				dma_range_index = i;
			}
		}

		attr->dma_attr_addr_lo = dma_ranges[dma_range_index].bus_addr;
		attr->dma_attr_addr_hi =
		    dma_ranges[dma_range_index].bus_addr +
		    dma_ranges[dma_range_index].size - 1;
	} else {
		ret = DDI_FAILURE;
	}

	if (dma_ranges) {
		kmem_free(
		    dma_ranges, sizeof (struct dma_range) * dma_range_num);
	}

	return (ret);
}

/*
 * This should actually be called i_ddi_dma_mem_alloc. There should
 * also be an i_ddi_pio_mem_alloc. i_ddi_dma_mem_alloc should call
 * through the device tree with the DDI_CTLOPS_DMA_ALIGN ctl ops to
 * get alignment requirements for DMA memory. i_ddi_pio_mem_alloc
 * should use DDI_CTLOPS_PIO_ALIGN. Since we only have i_ddi_mem_alloc
 * so far which is used for both, DMA and PIO, we have to use the DMA
 * ctl ops to make everybody happy.
 */
/*ARGSUSED*/
int
i_ddi_mem_alloc(dev_info_t *dip, ddi_dma_attr_t *oattr,
    size_t length, int cansleep, int flags,
    ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
    size_t *real_length, ddi_acc_hdl_t *ap)
{
	caddr_t a;
	int iomin;
	ddi_acc_impl_t *iap;
	int physcontig = 0;
	pgcnt_t npages;
	pgcnt_t minctg;
	uint_t order;
	int e;
	/*
	 * Check legality of arguments
	 */
	if (length == 0 || kaddrp == NULL || oattr == NULL) {
		return (DDI_FAILURE);
	}

	ddi_dma_attr_t data;
	ddi_dma_attr_t *attr = &data;
	if (i_ddi_convert_dma_attr(attr, dip, oattr) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (attr->dma_attr_minxfer == 0 || attr->dma_attr_align == 0 ||
	    !ISP2(attr->dma_attr_align) || !ISP2(attr->dma_attr_minxfer)) {
		return (DDI_FAILURE);
	}

	/*
	 * figure out most restrictive alignment requirement
	 */
	iomin = attr->dma_attr_minxfer;
	iomin = maxbit(iomin, attr->dma_attr_align);
	if (iomin == 0)
		return (DDI_FAILURE);

	ASSERT((iomin & (iomin - 1)) == 0);

	/*
	 * if we allocate memory with IOMEM_DATA_UNCACHED or
	 * IOMEM_DATA_UC_WR_COMBINE, make sure we allocate a page aligned
	 * memory that ends on a page boundry.
	 * Don't want to have to different cache mappings to the same
	 * physical page.
	 */
	if (OVERRIDE_CACHE_ATTR(flags)) {
		iomin = (iomin + MMU_PAGEOFFSET) & MMU_PAGEMASK;
		length = (length + MMU_PAGEOFFSET) & (size_t)MMU_PAGEMASK;
	}

	/*
	 * Determine if we need to satisfy the request for physically
	 * contiguous memory or alignments larger than pagesize.
	 */
	npages = btopr(length + attr->dma_attr_align);
	minctg = howmany(npages, attr->dma_attr_sgllen);

	if (minctg > 1) {
		uint64_t pfnseg = attr->dma_attr_seg >> PAGESHIFT;
		/*
		 * verify that the minimum contig requirement for the
		 * actual length does not cross segment boundary.
		 */
		length = P2ROUNDUP_TYPED(length, attr->dma_attr_minxfer,
		    size_t);
		npages = btopr(length);
		minctg = howmany(npages, attr->dma_attr_sgllen);
		if (minctg > pfnseg + 1)
			return (DDI_FAILURE);
		physcontig = 1;
	} else {
		length = P2ROUNDUP_TYPED(length, iomin, size_t);
	}

	/*
	 * Allocate the requested amount from the system.
	 */
	a = kalloca(length, iomin, cansleep, physcontig, attr);

	if ((*kaddrp = a) == NULL)
		return (DDI_FAILURE);

	/*
	 * if we to modify the cache attributes, go back and muck with the
	 * mappings.
	 */
	if (OVERRIDE_CACHE_ATTR(flags)) {
		order = 0;
		i_ddi_cacheattr_to_hatacc(flags, &order);
		e = kmem_override_cache_attrs(a, length, order);
		if (e != 0) {
			kfreea(a);
			return (DDI_FAILURE);
		}
	}

	if (real_length) {
		*real_length = length;
	}
	if (ap) {
		/*
		 * initialize access handle
		 */
		iap = (ddi_acc_impl_t *)ap->ah_platform_private;
		iap->ahi_acc_attr |= DDI_ACCATTR_CPU_VADDR;
		impl_acc_hdl_init(ap);
	}

	return (DDI_SUCCESS);
}

/* ARGSUSED */
void
i_ddi_mem_free(caddr_t kaddr, ddi_acc_hdl_t *ap)
{
	if (ap != NULL) {
		/*
		 * if we modified the cache attributes on alloc, go back and
		 * fix them since this memory could be returned to the
		 * general pool.
		 */
		if (OVERRIDE_CACHE_ATTR(ap->ah_xfermodes)) {
			uint_t order = 0;
			int e;
			i_ddi_cacheattr_to_hatacc(IOMEM_DATA_CACHED, &order);
			e = kmem_override_cache_attrs(kaddr, ap->ah_len, order);
			if (e != 0) {
				cmn_err(CE_WARN, "i_ddi_mem_free() failed to "
				    "override cache attrs, memory leaked\n");
				return;
			}
		}
	}
	kfreea(kaddr);
}


void
translate_devid(dev_info_t *dip)
{
}

pfn_t
i_ddi_paddr_to_pfn(paddr_t paddr)
{
	pfn_t pfn;

	pfn = mmu_btop(paddr);

	return (pfn);
}


/*
 * Perform a copy from a memory mapped device (whose devinfo pointer is devi)
 * separately mapped at devaddr in the kernel to a kernel buffer at kaddr.
 */
/*ARGSUSED*/
int
e_ddi_copyfromdev(dev_info_t *devi,
    off_t off, const void *devaddr, void *kaddr, size_t len)
{
	bcopy(devaddr, kaddr, len);
	return (0);
}

/*
 * Perform a copy to a memory mapped device (whose devinfo pointer is devi)
 * separately mapped at devaddr in the kernel from a kernel buffer at kaddr.
 */
/*ARGSUSED*/
int
e_ddi_copytodev(dev_info_t *devi,
    off_t off, const void *kaddr, void *devaddr, size_t len)
{
	bcopy(kaddr, devaddr, len);
	return (0);
}

static int
getlongprop_buf(int id, char *name, char *buf, int maxlen)
{
	int size;

	size = prom_getproplen((pnode_t)id, name);
	if (size <= 0 || (size > maxlen - 1))
		return (-1);

	if (-1 == prom_getprop((pnode_t)id, name, buf))
		return (-1);

	if (strcmp("name", name) == 0) {
		if (buf[size - 1] != '\0') {
			buf[size] = '\0';
			size += 1;
		}
	}

	return (size);
}
/*
 * The "status" property indicates the operational status of a device.
 * If this property is present, the value is a string indicating the
 * status of the device as follows:
 *
 *	"okay"		operational.
 *	"disabled"	not operational, but might become operational.
 *	"fail"		not operational because a fault has been detected,
 *			and it is unlikely that the device will become
 *			operational without repair. no additional details
 *			are available.
 *	"fail-xxx"	not operational because a fault has been detected,
 *			and it is unlikely that the device will become
 *			operational without repair. "xxx" is additional
 *			human-readable information about the particular
 *			fault condition that was detected.
 *
 * The absence of this property means that the operational status is
 * unknown or okay.
 *
 * This routine checks the status property of the specified device node
 * and returns 0 if the operational status indicates failure, and 1 otherwise.
 *
 * The property may exist on plug-in cards the existed before IEEE 1275-1994.
 * And, in that case, the property may not even be a string. So we carefully
 * check for the value "fail", in the beginning of the string, noting
 * the property length.
 */
int
status_okay(int id, char *buf, int buflen)
{
	char status_buf[OBP_MAXPROPNAME];
	char *bufp = buf;
	int len = buflen;
	int proplen;
	static const char *status = "status";
	static const char *fail = "fail";
	int fail_len = (int)strlen(fail);

	/*
	 * Get the proplen ... if it's smaller than "fail",
	 * or doesn't exist ... then we don't care, since
	 * the value can't begin with the char string "fail".
	 *
	 * NB: proplen, if it's a string, includes the NULL in the
	 * the size of the property, and fail_len does not.
	 */
	proplen = prom_getproplen((pnode_t)id, (caddr_t)status);
	if (proplen <= fail_len)	/* nonexistant or uninteresting len */
		return (1);

	/*
	 * if a buffer was provided, use it
	 */
	if ((buf == (char *)NULL) || (buflen <= 0)) {
		bufp = status_buf;
		len = sizeof (status_buf);
	}
	*bufp = (char)0;

	/*
	 * Get the property into the buffer, to the extent of the buffer,
	 * and in case the buffer is smaller than the property size,
	 * NULL terminate the buffer. (This handles the case where
	 * a buffer was passed in and the caller wants to print the
	 * value, but the buffer was too small).
	 */
	(void) prom_bounded_getprop((pnode_t)id, (caddr_t)status,
	    (caddr_t)bufp, len);
	*(bufp + len - 1) = (char)0;

	/*
	 * If the value begins with the char string "fail",
	 * then it means the node is failed. We don't care
	 * about any other values. We assume the node is ok
	 * although it might be 'disabled'.
	 */
	if (strncmp(bufp, fail, fail_len) == 0)
		return (0);

	return (1);
}

/*
 * Check the status of the device node passed as an argument.
 *
 *	if ((status is OKAY) || (status is DISABLED))
 *		return DDI_SUCCESS
 *	else
 *		print a warning and return DDI_FAILURE
 */
/*ARGSUSED1*/
int
check_status(int id, char *name, dev_info_t *parent)
{
	char status_buf[64];
	char devtype_buf[OBP_MAXPROPNAME];
	int retval = DDI_FAILURE;

	/*
	 * is the status okay?
	 */
	if (status_okay(id, status_buf, sizeof (status_buf)))
		return (DDI_SUCCESS);

	/*
	 * a status property indicating bad memory will be associated
	 * with a node which has a "device_type" property with a value of
	 * "memory-controller". in this situation, return DDI_SUCCESS
	 */
	if (getlongprop_buf(id, OBP_DEVICETYPE, devtype_buf,
	    sizeof (devtype_buf)) > 0) {
		if (strcmp(devtype_buf, "memory-controller") == 0)
			retval = DDI_SUCCESS;
	}

	/*
	 * print the status property information
	 */
	cmn_err(CE_WARN, "status '%s' for '%s'", status_buf, name);
	return (retval);
}

static int
poke_mem(peekpoke_ctlops_t *in_args)
{
	volatile int err = DDI_SUCCESS;
	on_trap_data_t otd;

	/* Set up protected environment. */
	if (!on_trap(&otd, OT_DATA_ACCESS)) {
		switch (in_args->size) {
		case sizeof (uint8_t):
			*(uint8_t *)(in_args->dev_addr) =
			    *(uint8_t *)in_args->host_addr;
			break;

		case sizeof (uint16_t):
			*(uint16_t *)(in_args->dev_addr) =
			    *(uint16_t *)in_args->host_addr;
			break;

		case sizeof (uint32_t):
			*(uint32_t *)(in_args->dev_addr) =
			    *(uint32_t *)in_args->host_addr;
			break;

		case sizeof (uint64_t):
			*(uint64_t *)(in_args->dev_addr) =
			    *(uint64_t *)in_args->host_addr;
			break;

		default:
			err = DDI_FAILURE;
			break;
		}
	} else
		err = DDI_FAILURE;

	/* Take down protected environment. */
	no_trap();

	return (err);
}

static int
peek_mem(peekpoke_ctlops_t *in_args)
{
	volatile int err = DDI_SUCCESS;
	on_trap_data_t otd;

	if (!on_trap(&otd, OT_DATA_ACCESS)) {
		switch (in_args->size) {
		case sizeof (uint8_t):
			*(uint8_t *)in_args->host_addr =
			    *(uint8_t *)in_args->dev_addr;
			break;

		case sizeof (uint16_t):
			*(uint16_t *)in_args->host_addr =
			    *(uint16_t *)in_args->dev_addr;
			break;

		case sizeof (uint32_t):
			*(uint32_t *)in_args->host_addr =
			    *(uint32_t *)in_args->dev_addr;
			break;

		case sizeof (uint64_t):
			*(uint64_t *)in_args->host_addr =
			    *(uint64_t *)in_args->dev_addr;
			break;

		default:
			err = DDI_FAILURE;
			break;
		}
	} else
		err = DDI_FAILURE;

	no_trap();
	return (err);
}

int
peekpoke_mem(ddi_ctl_enum_t cmd, peekpoke_ctlops_t *in_args)
{
	return (cmd == DDI_CTLOPS_PEEK ? peek_mem(in_args) : poke_mem(in_args));
}

uint_t
softlevel1(caddr_t arg1, caddr_t arg2)
{
	softint();
	return (1);
}

void
configure(void)
{
	extern void i_ddi_init_root();

	i_ddi_init_root();
	i_ddi_attach_hw_nodes("dld");
}

dev_t
getrootdev(void)
{
	/*
	 * Usually rootfs.bo_name is initialized by the
	 * the bootpath property from bootenv.rc, but
	 * defaults to "/ramdisk:a" otherwise.
	 */
	return (ddi_pathname_to_dev_t(rootfs.bo_name));
}

static struct bus_probe {
	struct bus_probe *next;
	void (*probe)(int);
} *bus_probes;
void
impl_bus_add_probe(void (*func)(int));
static int
print_dip(dev_info_t *dip, void *arg)
{
	char *model_str;
	prom_printf("%s: dip=%p\n", __FUNCTION__, dip);
	if (ddi_prop_lookup_string(
	    DDI_DEV_T_ANY, dip, 0, "name", &model_str) != DDI_SUCCESS)
		return (DDI_WALK_CONTINUE);
	prom_printf("%s: name=%s\n", __FUNCTION__, model_str);
	ddi_prop_free(model_str);
	return (DDI_WALK_CONTINUE);
}

/*
 * impl_bus_initialprobe
 *	Modload the prom simulator, then let it probe to verify existence
 *	and type of PCI support.
 */
static void
impl_bus_initialprobe(void)
{
	struct bus_probe *probe;

	modload("misc", "pci_autoconfig");

	probe = bus_probes;
	while (probe) {
		/* run the probe functions */
		(*probe->probe)(0);
		probe = probe->next;
	}

//	ddi_walk_devs(ddi_root_node(), print_dip, (void *)0);
}

void
impl_bus_add_probe(void (*func)(int))
{
	struct bus_probe *probe;
	struct bus_probe *lastprobe = NULL;

	probe = kmem_alloc(sizeof (*probe), KM_SLEEP);
	probe->probe = func;
	probe->next = NULL;

	if (!bus_probes) {
		bus_probes = probe;
		return;
	}

	lastprobe = bus_probes;
	while (lastprobe->next)
		lastprobe = lastprobe->next;
	lastprobe->next = probe;
}

/*ARGSUSED*/
void
impl_bus_delete_probe(void (*func)(int))
{
	struct bus_probe *prev = NULL;
	struct bus_probe *probe = bus_probes;

	while (probe) {
		if (probe->probe == func)
			break;
		prev = probe;
		probe = probe->next;
	}

	if (probe == NULL)
		return;

	if (prev)
		prev->next = probe->next;
	else
		bus_probes = probe->next;

	kmem_free(probe, sizeof (struct bus_probe));
}

boolean_t
i_ddi_copybuf_required(ddi_dma_attr_t *attrp)
{
	uint64_t hi_pa;

	hi_pa = ((uint64_t)physmax + 1ull) << PAGESHIFT;
	if (attrp->dma_attr_addr_hi < hi_pa) {
		return (B_TRUE);
	}

	return (B_FALSE);
}

size_t
i_ddi_copybuf_size()
{
	return (dma_max_copybuf_size);
}

/*
 * i_ddi_dma_max()
 *    returns the maximum DMA size which can be performed in a single DMA
 *    window taking into account the devices DMA contraints (attrp), the
 *    maximum copy buffer size (if applicable), and the worse case buffer
 *    fragmentation.
 */
/*ARGSUSED*/
uint32_t
i_ddi_dma_max(dev_info_t *dip, ddi_dma_attr_t *attrp)
{
	uint64_t maxxfer;


	/*
	 * take the min of maxxfer and the the worse case fragementation
	 * (e.g. every cookie <= 1 page)
	 */
	maxxfer = MIN(attrp->dma_attr_maxxfer,
	    ((uint64_t)(attrp->dma_attr_sgllen - 1) << PAGESHIFT));

	/*
	 * If the DMA engine can't reach all off memory, we also need to take
	 * the max size of the copybuf into consideration.
	 */
	if (i_ddi_copybuf_required(attrp)) {
		maxxfer = MIN(i_ddi_copybuf_size(), maxxfer);
	}

	/*
	 * we only return a 32-bit value. Make sure it's not -1. Round to a
	 * page so it won't be mistaken for an error value during debug.
	 */
	if (maxxfer >= 0xFFFFFFFF) {
		maxxfer = 0xFFFFF000;
	}

	/*
	 * make sure the value we return is a whole multiple of the
	 * granlarity.
	 */
	if (attrp->dma_attr_granular > 1) {
		maxxfer = maxxfer - (maxxfer % attrp->dma_attr_granular);
	}

	return ((uint32_t)maxxfer);
}
