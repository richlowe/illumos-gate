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

/* Copyright 2015, Richard Lowe. */

#include <sys/secflags.h>
#include <sys/types.h>

secflagset_t
secflag_to_bit(secflag_t secflag)
{
	return (1 << secflag);
}

boolean_t
secflag_isset(secflagset_t flags, secflag_t sf)
{
	return ((flags & secflag_to_bit(sf)) != 0);
}

void
secflag_clear(secflagset_t *flags, secflag_t sf)
{
	*flags &= ~secflag_to_bit(sf);
}

void
secflag_set(secflagset_t *flags, secflag_t sf)
{
	*flags |= secflag_to_bit(sf);
}

boolean_t
secflags_isempty(secflagset_t flags)
{
	return (flags == 0);
}

void
secflags_zero(secflagset_t *flags)
{
	*flags = 0;
}

void
secflags_fullset(secflagset_t *flags)
{
	*flags = PROC_SEC_MASK;
}


void
secflags_copy(secflagset_t *dst, const secflagset_t *src)
{
	*dst = *src;
}

boolean_t
secflags_issubset(secflagset_t a, secflagset_t b)
{
	return (!(a & ~b));
}

boolean_t
secflags_issuperset(secflagset_t a, secflagset_t b)
{
	return (secflags_issubset(b, a));
}

boolean_t
secflags_intersection(secflagset_t a, secflagset_t b)
{
	return (a & b);
}

void
secflags_union(secflagset_t *a, secflagset_t *b)
{
	*a |= *b;
}

void
secflags_difference(secflagset_t *a, secflagset_t *b)
{
	*a &= ~*b;
}

boolean_t
psecflags_validate_delta(const psecflags_t *sf, const secflagdelta_t *delta)
{
	if (delta->psd_ass_active) {
		/*
		 * If there's a bit in lower not in args, or a bit args not in
		 * upper
		 */
		if (!secflags_issubset(delta->psd_assign, sf->psf_upper) ||
		    !secflags_issuperset(delta->psd_assign, sf->psf_lower)) {
			return (B_FALSE);
		}

		if (!secflags_issubset(delta->psd_assign, PROC_SEC_MASK))
			return (B_FALSE);
	} else {
		/* If we're adding a bit not in upper */
		if (!secflags_isempty(delta->psd_add)) {
			if (!secflags_issubset(delta->psd_add, sf->psf_upper)) {
				return (B_FALSE);
			}
		}

		/* If we're removing a bit that's in lower */
		if (!secflags_isempty(delta->psd_rem)) {
			if (secflags_intersection(delta->psd_rem,
			    sf->psf_lower)) {
				return (B_FALSE);
			}
		}

		if (!secflags_issubset(delta->psd_add, PROC_SEC_MASK) ||
		    !secflags_issubset(delta->psd_rem, PROC_SEC_MASK))
			return (B_FALSE);
	}

	return (B_TRUE);
}

boolean_t
psecflags_validate(const psecflags_t *sf)
{
	if (!secflags_issubset(sf->psf_lower, PROC_SEC_MASK) ||
	    !secflags_issubset(sf->psf_inherit, PROC_SEC_MASK) ||
	    !secflags_issubset(sf->psf_effective, PROC_SEC_MASK) ||
	    !secflags_issubset(sf->psf_upper, PROC_SEC_MASK))
		return (B_FALSE);

	if (!secflags_issubset(sf->psf_lower, sf->psf_inherit))
		return (B_FALSE);
	if (!secflags_issubset(sf->psf_lower, sf->psf_upper))
		return (B_FALSE);
	if (!secflags_issubset(sf->psf_inherit, sf->psf_upper))
		return (B_FALSE);

	return (B_TRUE);
}

void
psecflags_default(psecflags_t *sf)
{
	secflags_zero(&sf->psf_effective);
	secflags_zero(&sf->psf_inherit);
	secflags_zero(&sf->psf_lower);
	secflags_fullset(&sf->psf_upper);
}
