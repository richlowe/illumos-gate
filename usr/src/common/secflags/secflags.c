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
secflag_isempty(secflagset_t flags)
{
	return (flags == 0);
}

void
secflag_zero(secflagset_t *flags)
{
	*flags = 0;
}

void
secflag_fullset(secflagset_t *flags)
{
	*flags = PROC_SEC_MASK;
}


void
secflag_copy(secflagset_t *dst, const secflagset_t *src)
{
	*dst = *src;
}

boolean_t
secflag_issubset(secflagset_t a, secflagset_t b)
{
	return (!(a & ~b));
}

boolean_t
secflag_issuperset(secflagset_t a, secflagset_t b)
{
	return (secflag_issubset(b, a));
}

boolean_t
secflag_intersection(secflagset_t a, secflagset_t b)
{
	return (a & b);
}

void
secflag_union(secflagset_t *a, secflagset_t *b)
{
	*a |= *b;
}

void
secflag_difference(secflagset_t *a, secflagset_t *b)
{
	*a &= ~*b;
}

boolean_t
secflag_validate_delta(const psecflags_t *sf, const psecflagdelta_t *delta)
{
	if (delta->psd_ass_active) {
		/*
		 * If there's a bit in lower not in args, or a bit args not in
		 * upper
		 */
		if (!secflag_issubset(delta->psd_assign, sf->psf_upper) ||
		    !secflag_issuperset(delta->psd_assign, sf->psf_lower)) {
			return (B_FALSE);
		}

		if (!secflag_issubset(delta->psd_assign, PROC_SEC_MASK))
			return (B_FALSE);
	} else {
		/* If we're adding a bit not in upper */
		if (!secflag_isempty(delta->psd_add)) {
			if (!secflag_issubset(delta->psd_add, sf->psf_upper)) {
				return (B_FALSE);
			}
		}

		/* If we're removing a bit that's in lower */
		if (!secflag_isempty(delta->psd_rem)) {
			if (secflag_intersection(delta->psd_rem,
			    sf->psf_lower)) {
				return (B_FALSE);
			}
		}

		if (!secflag_issubset(delta->psd_add, PROC_SEC_MASK) ||
		    !secflag_issubset(delta->psd_rem, PROC_SEC_MASK))
			return (B_FALSE);
	}

	return (B_TRUE);
}

boolean_t
secflags_validate(const psecflags_t *sf)
{
	if (!secflag_issubset(sf->psf_lower, PROC_SEC_MASK) ||
	    !secflag_issubset(sf->psf_inherit, PROC_SEC_MASK) ||
	    !secflag_issubset(sf->psf_effective, PROC_SEC_MASK) ||
	    !secflag_issubset(sf->psf_upper, PROC_SEC_MASK))
		return (B_FALSE);

	if (!secflag_issubset(sf->psf_lower, sf->psf_inherit))
		return (B_FALSE);
	if (!secflag_issubset(sf->psf_lower, sf->psf_upper))
		return (B_FALSE);
	if (!secflag_issubset(sf->psf_inherit, sf->psf_upper))
		return (B_FALSE);

	return (B_TRUE);
}

void
secflags_default(psecflags_t *sf)
{
	secflag_zero(&sf->psf_effective);
	secflag_zero(&sf->psf_inherit);
	secflag_zero(&sf->psf_lower);
	secflag_fullset(&sf->psf_upper);
}
