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

/*
 * XXX: uint_t should be an enum? (but beware robert's concerns about enum
 * ABI?
 */

secflagset_t
secflag_to_bit(uint_t secflag)
{
	return (1 << secflag);
}

boolean_t
secflag_isset(secflagset_t flags, uint_t sf)
{
	return ((flags & secflag_to_bit(sf)) != 0);
}

/* XXX: Make these pure?, or return the before, or...? */
void
secflag_clear(secflagset_t *flags, uint_t sf)
{
	*flags &= ~secflag_to_bit(sf);
}

void
secflag_set(secflagset_t *flags, uint_t sf)
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
