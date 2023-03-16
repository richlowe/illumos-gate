/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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

#ifndef	_SYS_FRAME_H
#define	_SYS_FRAME_H

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

	/*
	 *	|
	 *	+----------------
	 *	|	LR''
	 *	+----------------
	 *	|	FP''
	 * FP'	+----------------
	 *	|   Stack Args
	 * SP'	 +----------------
	 *	| GR Args Saved
	 *	+----------------
	 *	| VR Args Saved
	 *	+----------------
	 *	|  Callee Saved
	 *	+----------------
	 *	| Local Variable
	 *	+----------------
	 *	|	LR'
	 *	+----------------
	 *	|	FP'
	 * FP	+----------------
	 *	|   Stack Args
	 * SP	+----------------
	 */

struct frame {
	greg_t	fr_savfp;
	greg_t	fr_savpc;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FRAME_H */
