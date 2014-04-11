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

/* Copyright 2014, Richard Lowe. */

#ifndef _DT_LINK_H
#define	_DT_LINK_H

#include <dtrace.h>
#include <gelf.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Private interfaces between libdtrace and ld(1)
 */
typedef struct dt_probe dt_probe_t;
typedef struct dt_provider dt_provider_t;

extern dt_provider_t *dt_provider_lookup(dtrace_hdl_t *, const char *);
extern dt_probe_t *dt_probe_lookup(dt_provider_t *, const char *);

extern int dt_modtext(dtrace_hdl_t *, char *, int, GElf_Rela *, uint32_t *);
extern int dt_probe_define(dt_provider_t *, dt_probe_t *,
    const char *, const char *, uint32_t, int);
extern void dtrace_program_setversion(dtrace_hdl_t *, dtrace_prog_t *, uint_t);

extern int dtrace_parse_symbol(const char *, dtrace_probedesc_t *, int *);
extern Elf *dtrace_dof_elf(dtrace_hdl_t *, const dof_hdr_t *, char *);

extern int dt_header_script_fencode(dtrace_hdl_t *, FILE *, int, FILE *);
extern int dt_header_script_strencode(dtrace_hdl_t *, char *, int, FILE *);
extern char *dt_header_script_decode(void *);

#ifdef __cplusplus
}
#endif

#endif /* _DT_LINK_H */
