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
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 * Copyright (c) 1989, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/* Get the sparc version of the relocation engine */
#define	DO_RELOC_LIBLD_AARCH64

#include	<string.h>
#include	<stdio.h>
#include	<sys/elf_aarch64.h>
#include	<debug.h>
#include	<reloc.h>
#include	<aarch64/machdep_aarch64.h>

#include	"_libld.h"

#ifndef _ELF64
#error "You're trying to build a 32bit AArch64 somehow"
#endif


/*
 * Search the GOT index list for a GOT entry with a matching reference and the
 * proper addend.
 */
static Gotndx *
ld_find_got_ndx(Alist *alp, Gotref gref, Ofl_desc *ofl, Rel_desc *rdesc)
{
	Aliste	idx;
	Gotndx	*gnp;

	assert(rdesc != 0);

	if ((gref == GOT_REF_TLSLD) && ofl->ofl_tlsldgotndx)
		return (ofl->ofl_tlsldgotndx);

	for (ALIST_TRAVERSE(alp, idx, gnp)) {
		if ((rdesc->rel_raddend == gnp->gn_addend) &&
		    (gnp->gn_gotref == gref)) {
			return (gnp);
		}
	}
	return (NULL);
}

static Xword
ld_calc_got_offset(Rel_desc * rdesc, Ofl_desc * ofl)
{
	Os_desc		*osp = ofl->ofl_osgot;
	Sym_desc	*sdp = rdesc->rel_sym;
	Xword		gotndx;
	Gotref		gref;
	Gotndx		*gnp;

	if (rdesc->rel_flags & FLG_REL_DTLS)
		gref = GOT_REF_TLSGD;
	else if (rdesc->rel_flags & FLG_REL_MTLS)
		gref = GOT_REF_TLSLD;
	else if (rdesc->rel_flags & FLG_REL_STLS)
		gref = GOT_REF_TLSIE;
	else
		gref = GOT_REF_GENERIC;

	gnp = ld_find_got_ndx(sdp->sd_GOTndxs, gref, ofl, rdesc);
	assert(gnp);

	gotndx = (Xword)gnp->gn_gotndx;

	/*
	 * If this is the the offset part of a GD relocation, we need to
	 * modify the _next_ got entry to the one recorded.  We reserved it in
	 * `ld_assign_got_ndx`
	 */
	if ((rdesc->rel_flags & FLG_REL_DTLS) &&
	    (rdesc->rel_rtype == R_AARCH64_TLS_DTPREL)) {
		gotndx++;
	}

	return ((Xword)(osp->os_shdr->sh_addr + (gotndx * M_GOT_ENTSIZE)));
}

static Word
ld_init_rel(Rel_desc *reld, Word *typedata, void *reloc)
{
	Rela	*rela = (Rela *)reloc;

	/* LINTED */
	reld->rel_rtype = (Word)ELF_R_TYPE(rela->r_info, M_MACH);
	reld->rel_roffset = rela->r_offset;
	reld->rel_raddend = rela->r_addend;
	*typedata = (Word)ELF_R_TYPE_DATA(rela->r_info);

	reld->rel_flags |= FLG_REL_RELA;

	return ((Word)ELF_R_SYM(rela->r_info));
}

static void
ld_mach_eflags(Ehdr *ehdr, Ofl_desc *ofl)
{
	ofl->ofl_dehdr->e_flags |= ehdr->e_flags;
}

static void
ld_mach_make_dynamic(Ofl_desc *ofl, size_t *cnt)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		/*
		 * Create this entry if we are going to create a PLT table.
		 */
		if (ofl->ofl_pltcnt)
			(*cnt)++;		/* DT_PLTGOT */
	}
}

static void
ld_mach_update_odynamic(Ofl_desc *ofl, Dyn **dyn)
{
	if (((ofl->ofl_flags & FLG_OF_RELOBJ) == 0) && ofl->ofl_pltcnt) {
		(*dyn)->d_tag = DT_PLTGOT;
		if (ofl->ofl_osplt) {
			(*dyn)->d_un.d_ptr =
			    ofl->ofl_osgotplt->os_shdr->sh_addr;
		} else {
			(*dyn)->d_un.d_ptr = 0;
		}
		(*dyn)++;
	}
}

static Xword
ld_calc_plt_addr(Sym_desc *sdp, Ofl_desc *ofl)
{
	Xword	value;

	value = (Xword)(ofl->ofl_osplt->os_shdr->sh_addr) +
	    M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) * M_PLT_ENTSIZE);
	return (value);
}

/*
 * XXXARM: Note that BTI/PAC require special PLTs which we do not yet create
 * or support.
 */
static uchar_t plt0_entry[M_PLT_RESERVSZ] = {
	/* stp x16, x30, [sp,#-16]! */			0xf0, 0x7b, 0xbf, 0xa9,
	/* adrp x16, Page(&(.plt.got[2])) */		0x10, 0x00, 0x00, 0x90,
	/* ldr x17, [x16, Offset(&(.plt.got[2]))] */	0x11, 0x02, 0x40, 0xf9,
	/* add x16, x16, Offset(&(.plt.got[2])) */	0x10, 0x02, 0x00, 0x91,
	/* br x17 */					0x20, 0x02, 0x1f, 0xd6,
	/* nop */					0x1f, 0x20, 0x03, 0xd5,
	/* nop */					0x1f, 0x20, 0x03, 0xd5,
	/* nop */					0x1f, 0x20, 0x03, 0xd5,
};

static uchar_t pltn_entry[M_PLT_ENTSIZE] = {
	/* adrp x16, Page(&(.plt.got[n])) */		0x10, 0x00, 0x00, 0x90,
	/* ldr x17, [x16, Offset(&(.plt.got[n]))] */	0x11, 0x02, 0x40, 0xf9,
	/* add x16, x16, Offset(&(.plt.got[n])) */	0x10, 0x02, 0x00, 0x91,
	/* br x17 */					0x20, 0x02, 0x1f, 0xd6,
};

static const char *
syn_rdesc_sym_name(Rel_desc *rdesc)
{
	return (MSG_ORIG(MSG_SYM_PLTENT));
}

/*
 * Given an address, return the 4K page it's on.
 *
 * XXXARM: I thought we had a machine independent macro for this, but I can't
 * find it.
 */
#define	AARCH64_PAGE(x)	((x) & ~0xfff)

static uintptr_t
plt_entry(Ofl_desc *ofl, Sym_desc *sdp)
{
	uchar_t		*plt0, *pltent, *gotent __unused;
	Sword		plt_off;
	Word		got_off;
	Addr		got = ofl->ofl_osgotplt->os_shdr->sh_addr;
	Addr		plt = ofl->ofl_osplt->os_shdr->sh_addr;
	int		bswap = (ofl->ofl_flags1 & FLG_OF1_ENCDIFF) != 0;

	got_off = (sdp->sd_aux->sa_PLTndx - 1 + 3) * M_GOT_ENTSIZE;
	plt_off = M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) *
	    M_PLT_ENTSIZE);
	plt0 = ofl->ofl_osplt->os_outdata->d_buf;
	pltent = plt0 + plt_off;
	gotent = ofl->ofl_osgotplt->os_outdata->d_buf + got_off;

	memcpy(pltent, pltn_entry, sizeof (pltn_entry));

	/*
	 * Fill in the got entry with the address of the next instruction.
	 */
	*(Word *)gotent = ofl->ofl_osplt->os_shdr->sh_addr;
	if (bswap)
		*(Word *)gotent = ld_bswap_Word(*(Word *)gotent);

	/*
	 * If '-z noreloc' is specified - skip the do_reloc_ld
	 * stage.
	 */
	if (!OFL_DO_RELOC(ofl))
		return (1);

	/* The page address of the .got.plt */
	static Rel_desc rdesc_r_prel_pg_hi21 = { NULL, NULL, NULL, 0, 0, 0,
		R_AARCH64_ADR_PREL_PG_HI21 };

	Xword val1 = AARCH64_PAGE(got + got_off) - AARCH64_PAGE(plt + plt_off);

	if (do_reloc_ld(&rdesc_r_prel_pg_hi21, &pltent[0], &val1,
	    syn_rdesc_sym_name, MSG_ORIG(MSG_SPECFIL_PLTENT), bswap,
	    ofl->ofl_lml) == 0) {
		ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_PLT_PLTNFAIL),
		    sdp->sd_aux->sa_PLTndx, demangle(sdp->sd_name));
		return (S_ERROR);
	}

	/* The offset into .got.plt */
	static Rel_desc rdesc_r_ldst64_abs_lo12_nc = { NULL, NULL, NULL,
		0, 0, 0, R_AARCH64_LDST64_ABS_LO12_NC };
	val1 = got + got_off;

	if (do_reloc_ld(&rdesc_r_ldst64_abs_lo12_nc, &pltent[4], &val1,
	    syn_rdesc_sym_name, MSG_ORIG(MSG_SPECFIL_PLTENT), bswap,
	    ofl->ofl_lml) == 0) {
		ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_PLT_PLTNFAIL),
		    sdp->sd_aux->sa_PLTndx, demangle(sdp->sd_name));
		return (S_ERROR);
	}

	/* The offset into .got.plt */
	static Rel_desc rdesc_r_add_abs_lo12_nc = { NULL, NULL, NULL, 0, 0, 0,
		R_AARCH64_ADD_ABS_LO12_NC };
	val1 = got + got_off;

	if (do_reloc_ld(&rdesc_r_add_abs_lo12_nc, &pltent[8], &val1,
	    syn_rdesc_sym_name, MSG_ORIG(MSG_SPECFIL_PLTENT), bswap,
	    ofl->ofl_lml) == 0) {
		ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_PLT_PLTNFAIL),
		    sdp->sd_aux->sa_PLTndx, demangle(sdp->sd_name));
		return (S_ERROR);
	}

	return (1);
}

static uintptr_t
ld_perform_outreloc(Rel_desc *orsp, Ofl_desc *ofl, Boolean *remain_seen)
{
	Os_desc		*relosp, *osp = NULL;
	Xword		value, roffset, ndx;
	Sxword		raddend;
	Sym_desc	 *sdp, *psym = NULL;
	int		sectmoved = 0;
	Rela		rea;
	char		*relbits;

	raddend = orsp->rel_raddend;
	sdp = orsp->rel_sym;

	/*
	 * If the section this relocation is against has been discarded
	 * (-zignore), then also discard (skip) the relocation itself.
	 */
	if (orsp->rel_isdesc && ((orsp->rel_flags &
	    (FLG_REL_GOT | FLG_REL_BSS | FLG_REL_PLT | FLG_REL_NOINFO)) == 0) &&
	    (orsp->rel_isdesc->is_flags & FLG_IS_DISCARD)) {
		DBG_CALL(Dbg_reloc_discard(ofl->ofl_lml, M_MACH, orsp));
		return (1);
	}

	/*
	 * If this is a relocation against a section then we need to adjust the
	 * raddend field to compensate for the new position of the input section
	 * within the new output section.
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) {
		if (ofl->ofl_parsyms &&
		    (sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
		    /* LINTED */
		    (psym = ld_am_I_partial(orsp, orsp->rel_raddend))) {
			DBG_CALL(Dbg_move_outsctadj(ofl->ofl_lml, psym));
			sectmoved = 1;
			if (ofl->ofl_flags & FLG_OF_RELOBJ)
				raddend = psym->sd_sym->st_value;
			else
				raddend = psym->sd_sym->st_value -
				    psym->sd_isc->is_osdesc->os_shdr->sh_addr;
			/* LINTED */
			raddend += (Off)_elf_getxoff(psym->sd_isc->is_indata);
			if (psym->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
				raddend +=
				    psym->sd_isc->is_osdesc->os_shdr->sh_addr;
		} else {
			/* LINTED */
			raddend += (Off)_elf_getxoff(sdp->sd_isc->is_indata);
			if (sdp->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
				raddend +=
				    sdp->sd_isc->is_osdesc->os_shdr->sh_addr;
		}
	}

	value = sdp->sd_sym->st_value;

	if (orsp->rel_flags & FLG_REL_GOT) {
		osp = ofl->ofl_osgot;
		roffset = ld_calc_got_offset(orsp, ofl);
	} else if (orsp->rel_flags & FLG_REL_PLT) {
		/*
		 * Note that relocations for PLT's actually
		 * cause a relocation againt the PLTGOT.
		 */
		osp = ofl->ofl_osplt;
		/* -1+3 for for the reserved entries */
		roffset = (ofl->ofl_osgotplt->os_shdr->sh_addr) +
		    (sdp->sd_aux->sa_PLTndx - 1 + 3) * M_GOT_ENTSIZE;
		raddend = 0;
		if (plt_entry(ofl, sdp) == S_ERROR)
			return (S_ERROR);

	} else if (orsp->rel_flags & FLG_REL_BSS) {
		/*
		 * This must be a R_AARCH64_COPY.  For these set the roffset to
		 * point to the new symbols location.
		 */
		osp = ofl->ofl_isbss->is_osdesc;
		roffset = value;

		/*
		 * The raddend doesn't mean anything in a R_AARCH64_COPY
		 * relocation.  Null it out because it can confuse people.
		 */
		raddend = 0;
	} else {
		osp = RELAUX_GET_OSDESC(orsp);

		/*
		 * Calculate virtual offset of reference point; equals offset
		 * into section + vaddr of section for loadable sections, or
		 * offset plus section displacement for nonloadable sections.
		 */
		roffset = orsp->rel_roffset +
		    (Off)_elf_getxoff(orsp->rel_isdesc->is_indata);
		if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
			roffset += orsp->rel_isdesc->is_osdesc->
			    os_shdr->sh_addr;
	}

	if ((osp == 0) || ((relosp = osp->os_relosdesc) == 0))
		relosp = ofl->ofl_osrel;

	/*
	 * XXXARM: We should that our output offset is correctly aligned.
	 */

	/*
	 * Assign the symbols index for the output relocation.  If the
	 * relocation refers to a SECTION symbol then it's index is based upon
	 * the output sections symbols index.  Otherwise the index can be
	 * derived from the symbols index itself.
	 */
	if (orsp->rel_rtype == R_AARCH64_RELATIVE)
		ndx = STN_UNDEF;
	else if ((orsp->rel_flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION)) {
		if (sectmoved == 0) {
			/*
			 * Check for a null input section. This can
			 * occur if this relocation references a symbol
			 * generated by sym_add_sym().
			 */
			if (sdp->sd_isc && sdp->sd_isc->is_osdesc)
				ndx = sdp->sd_isc->is_osdesc->os_identndx;
			else
				ndx = sdp->sd_shndx;
		} else
			ndx = ofl->ofl_parexpnndx;
	} else
		ndx = sdp->sd_symndx;

	/*
	 * Add the symbols 'value' to the addend field.
	 */
	if (orsp->rel_flags & FLG_REL_ADVAL)
		raddend += value;

	if ((orsp->rel_rtype != M_R_NONE) &&
	    (orsp->rel_rtype != M_R_RELATIVE)) {
		if (ndx == 0) {
			Conv_inv_buf_t	inv_buf;
			Is_desc *isp = orsp->rel_isdesc;

			ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_REL_NOSYMBOL),
			    conv_reloc_type(ofl->ofl_nehdr->e_machine,
			    orsp->rel_rtype, 0, &inv_buf),
			    isp->is_file->ifl_name, EC_WORD(isp->is_scnndx),
			    isp->is_name, EC_XWORD(roffset));
			return (S_ERROR);
		}
	}

	rea.r_info = ELF_R_INFO(ndx, orsp->rel_rtype);
	rea.r_offset = roffset;
	rea.r_addend = raddend;
	DBG_CALL(Dbg_reloc_out(ofl, ELF_DBG_LD, SHT_RELA, &rea, relosp->os_name,
	    ld_reloc_sym_name(orsp)));

	/*
	 * Assert we haven't walked off the end of our relocation table.
	 */
	assert(relosp->os_szoutrels <= relosp->os_shdr->sh_size);

	relbits = (char *)relosp->os_outdata->d_buf;

	(void) memcpy((relbits + relosp->os_szoutrels),
	    (char *)&rea, sizeof (Rela));
	relosp->os_szoutrels += (Xword)sizeof (Rela);

	/*
	 * Determine if this relocation is against a non-writable, allocatable
	 * section.  If so we may need to provide a text relocation diagnostic.
	 * Note that relocations against the .plt (R_AARCH64_JUMP_SLOT) actually
	 * result in modifications to the .got.plt
	 */
	if (orsp->rel_rtype == R_AARCH64_JUMP_SLOT)
		osp = ofl->ofl_osgotplt;

	ld_reloc_remain_entry(orsp, osp, ofl, remain_seen);
	return (1);
}

/* XXXARM: None on ARM just yet */
#if 0
static Fixupret
tls_fixups(Ofl_desc *ofl, Rel_desc *arsp)
{
	assert(0 && "Not implemented");
	return (0);
}
#endif

static uintptr_t
ld_do_activerelocs(Ofl_desc *ofl)
{
	Rel_desc	*arsp;
	Rel_cachebuf	*rcbp;
	Aliste		idx;
	ofl_flag_t	flags = ofl->ofl_flags;
	uintptr_t	return_code = 1;

	if (aplist_nitems(ofl->ofl_actrels.rc_list) != 0)
		DBG_CALL(Dbg_reloc_doact_title(ofl->ofl_lml));

	/*
	 * Process active relocations.
	 */
	REL_CACHE_TRAVERSE(&ofl->ofl_actrels, idx, rcbp, arsp) {
		uchar_t		*addr;
		Xword		value;
		Os_desc		*osp;
		const char	*ifl_name;
		Gotref		gref;
		Sym_desc	*sdp;
		Xword		refaddr;
		int		moved = 0;

		/*
		 * If the section this relocation is against has been discarded
		 * (-zignore), then discard (skip) the relocation itself.
		 */
		if ((arsp->rel_isdesc->is_flags & FLG_IS_DISCARD) &&
		    ((arsp->rel_flags & (FLG_REL_GOT | FLG_REL_BSS |
		    FLG_REL_PLT | FLG_REL_NOINFO)) == 0)) {
			DBG_CALL(Dbg_reloc_discard(ofl->ofl_lml, M_MACH, arsp));
			continue;
		}

		/*
		 * We determine what the 'got reference' model (if required)
		 * is at this point.  This needs to be done before tls_fixups()
		 * since it may 'transition' our instructions.
		 *
		 * The got table entries have already been assigned,
		 * and we bind to those initial entries.
		 */
		if (arsp->rel_flags & FLG_REL_DTLS)
			gref = GOT_REF_TLSGD;
		else if (arsp->rel_flags & FLG_REL_MTLS)
			gref = GOT_REF_TLSLD;
		else if (arsp->rel_flags & FLG_REL_STLS)
			gref = GOT_REF_TLSIE;
		else
			gref = GOT_REF_GENERIC;

		/*
		 * Perform any required TLS fixups.
		 * XXXARM: No optimizations yet
		 */
#if 0
		if (arsp->rel_flags & FLG_REL_TLSFIX) {

			Fixupret	ret;

			if ((ret = tls_fixups(ofl, arsp)) == FIX_ERROR)
				return (S_ERROR);
			if (ret == FIX_DONE)
				continue;
		}
#endif

		/*
		 * If this is a relocation against a move table, or
		 * expanded move table, adjust the relocation entries.
		 */
		if (RELAUX_GET_MOVE(arsp))
			ld_adj_movereloc(ofl, arsp);

		sdp = arsp->rel_sym;
		refaddr = arsp->rel_roffset +
		    (Off)_elf_getxoff(arsp->rel_isdesc->is_indata);

		if ((arsp->rel_flags & FLG_REL_CLVAL) ||
		    (arsp->rel_flags & FLG_REL_GOTCL))
			value = 0;
		else if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) {
			Sym_desc	*sym;

			/*
			 * The value for a symbol pointing to a SECTION
			 * is based off of that sections position.
			 */
			if ((sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
			    /* LINTED */
			    (sym = ld_am_I_partial(arsp, arsp->rel_raddend))) {
				/*
				 * The symbol was moved, so adjust the value
				 * relative to the new section.
				 */
				value = sym->sd_sym->st_value;
				moved = 1;

				/*
				 * The original raddend covers the displacement
				 * from the section start to the desired
				 * address. The value computed above gets us
				 * from the section start to the start of the
				 * symbol range. Adjust the old raddend to
				 * remove the offset from section start to
				 * symbol start, leaving the displacement
				 * within the range of the symbol.
				 */
				arsp->rel_raddend -= sym->sd_osym->st_value;
			} else {
				value = _elf_getxoff(sdp->sd_isc->is_indata);
				if (sdp->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
					value += sdp->sd_isc->is_osdesc->
					    os_shdr->sh_addr;
			}

			/*
			 * If this is a TLS reference, value is the offset
			 * into the TLS block.
			 */
			if (sdp->sd_isc->is_shdr->sh_flags & SHF_TLS)
				value -= ofl->ofl_tlsphdr->p_vaddr;

		} else if (IS_SIZE(arsp->rel_rtype)) {
			/*
			 * Size relocations require the symbols size.
			 */
			value = sdp->sd_sym->st_size;
		} else if ((sdp->sd_flags & FLG_SY_CAP) &&
		    sdp->sd_aux && sdp->sd_aux->sa_PLTndx) {
			/*
			 * If relocation is against a capabilities symbol, we
			 * need to jump to an associated PLT, so that at runtime
			 * ld.so.1 is involved to determine the best binding
			 * choice. Otherwise, the value is the symbols value.
			 */
			value = ld_calc_plt_addr(sdp, ofl);
		} else
			value = sdp->sd_sym->st_value;

		/*
		 * Relocation against the GLOBAL_OFFSET_TABLE.
		 */
		if ((arsp->rel_flags & FLG_REL_GOT) &&
		    !ld_reloc_set_aux_osdesc(ofl, arsp, ofl->ofl_osgot))
			return (S_ERROR);
		osp = RELAUX_GET_OSDESC(arsp);

		/*
		 * If loadable and not producing a relocatable object add the
		 * sections virtual address to the reference address.
		 */
		if ((arsp->rel_flags & FLG_REL_LOAD) &&
		    ((flags & FLG_OF_RELOBJ) == 0))
			refaddr += arsp->rel_isdesc->is_osdesc->
			    os_shdr->sh_addr;

		/*
		 * If this entry has a PLT assigned to it, its value is actually
		 * the address of the PLT (and not the address of the function).
		 */
		if (IS_PLT(arsp->rel_rtype)) {
			if (sdp->sd_aux && sdp->sd_aux->sa_PLTndx)
				value = ld_calc_plt_addr(sdp, ofl);
		}

		/*
		 * Add relocations addend to value.  Add extra
		 * relocation addend if needed.
		 */
		value += arsp->rel_raddend;

		/*
		 * Determine whether the value needs further adjustment. Filter
		 * through the attributes of the relocation to determine what
		 * adjustment is required.  Note, many of the following cases
		 * are only applicable when a .got is present.  As a .got is
		 * not generated when a relocatable object is being built,
		 * any adjustments that require a .got need to be skipped.
		 */
		if ((arsp->rel_flags & FLG_REL_GOT) &&
		    ((flags & FLG_OF_RELOBJ) == 0)) {
			Xword		R1addr;
			uintptr_t	R2addr;
			Word		gotndx;
			Gotndx		*gnp;

			/*
			 * Perform relocation against GOT table. Since this
			 * doesn't fit exactly into a relocation we place the
			 * appropriate byte in the GOT directly
			 *
			 * Calculate offset into GOT at which to apply
			 * the relocation.
			 */
			gnp = ld_find_got_ndx(sdp->sd_GOTndxs, gref, ofl, arsp);
			assert(gnp);

			/*
			 * If this is the DTPREL portion of a GD relocation,
			 * we need to write into the next word in the GOT.  It
			 * was reserved in `ld_assign_got_ndx`
			 */
			if (arsp->rel_rtype == R_AARCH64_TLS_DTPREL)
				gotndx = gnp->gn_gotndx + 1;
			else
				gotndx = gnp->gn_gotndx;

			R1addr = (Xword)(gotndx * M_GOT_ENTSIZE);

			/*
			 * Add the GOTs data's offset.
			 */
			R2addr = R1addr + (uintptr_t)osp->os_outdata->d_buf;

			DBG_CALL(Dbg_reloc_doact(ofl->ofl_lml, ELF_DBG_LD_ACT,
			    M_MACH, SHT_RELA, arsp, R1addr, value,
			    ld_reloc_sym_name));

			/*
			 * And do it.
			 */
			if (ofl->ofl_flags1 & FLG_OF1_ENCDIFF)
				*(Xword *)R2addr = ld_bswap_Xword(value);
			else
				*(Xword *)R2addr = value;
			continue;
		} else if (IS_GOTPAGEPC_BASED(arsp->rel_rtype)) {
			Gotndx *gnp;

			gnp = ld_find_got_ndx(sdp->sd_GOTndxs, gref, ofl, arsp);
			assert(gnp);

			value = (Xword)(ofl->ofl_osgot->os_shdr->sh_addr) +
			    ((Xword)gnp->gn_gotndx * M_GOT_ENTSIZE);

			value = AARCH64_PAGE(value);
			value -= AARCH64_PAGE(refaddr);
		} else if (IS_PAGEPC_BASED(arsp->rel_rtype)) {
			/* XXXARM: No RELOBJ check, but probably should be */

			/* PAGE(S+A)-PAGE(P) */
			if ((sdp->sd_sym->st_shndx == SHN_UNDEF) &&
			    (ELF_ST_BIND(sdp->sd_sym->st_info) == STB_WEAK)) {
				assert(0 && "UNDEF WEAK PAGEPC");
			} else {
				value = AARCH64_PAGE(value) -
				    AARCH64_PAGE(refaddr);
			}
		} else if (IS_GOTPAGE_BASED(arsp->rel_rtype) &&
		    ((flags & FLG_OF_RELOBJ) == 0)) {
			Gotndx *gnp;

			gnp = ld_find_got_ndx(sdp->sd_GOTndxs, gref, ofl, arsp);
			assert(gnp);

			value = (Xword)(ofl->ofl_osgot->os_shdr->sh_addr) +
			    ((Xword)gnp->gn_gotndx * M_GOT_ENTSIZE);

			value -= AARCH64_PAGE(ofl->ofl_osgot->os_shdr->sh_addr);
		} else if (IS_GOT_BASED(arsp->rel_rtype) &&
		    ((flags & FLG_OF_RELOBJ) == 0)) {
			value -= ofl->ofl_osgot->os_shdr->sh_addr;

		} else if (IS_GOTPCREL(arsp->rel_rtype) &&
		    ((flags & FLG_OF_RELOBJ) == 0)) {
			Gotndx *gnp;
			/*
			 * Calculation:
			 *	G + GOT + A - P
			 */
			gnp = ld_find_got_ndx(sdp->sd_GOTndxs, gref, ofl, arsp);
			assert(gnp);

			value = (Xword)(ofl->ofl_osgot->os_shdr-> sh_addr) +
			    ((Xword)gnp->gn_gotndx * M_GOT_ENTSIZE) +
			    arsp->rel_raddend - refaddr;

		} else if (IS_GOT_PC(arsp->rel_rtype) &&
		    ((flags & FLG_OF_RELOBJ) == 0)) {
			assert(0 && "GOTPC Not ported");
			value = (Xword)(ofl->ofl_osgot->os_shdr->
			    sh_addr) - refaddr + arsp->rel_raddend;

		} else if ((IS_PC_RELATIVE(arsp->rel_rtype)) &&
		    (((flags & FLG_OF_RELOBJ) == 0) ||
		    (osp == sdp->sd_isc->is_osdesc))) {
			value -= refaddr;

		} else if (IS_GOT_RELATIVE(arsp->rel_rtype) &&
		    ((flags & FLG_OF_RELOBJ) == 0)) {
			Gotndx	*gnp;
			gnp = ld_find_got_ndx(sdp->sd_GOTndxs, gref, ofl, arsp);
			assert(gnp);
			value = (Xword)gnp->gn_gotndx * M_GOT_ENTSIZE;

			/* Value in the GOT should be absolute */
			if (IS_GOT_ABS(arsp->rel_rtype)) {
				value += ofl->ofl_osgot->os_shdr->sh_addr;
			}
		} else if ((arsp->rel_flags & FLG_REL_STLS) &&
		    ((flags & FLG_OF_RELOBJ) == 0)) {
			/*
			 * This is the LE TLS reference model.  The static
			 * offset is hard-coded, we just have to amend it to
			 * avoid the ABI-specified TCB, which is two pointers
			 * in size.
			 */
			value += sizeof (uintptr_t) * 2;
		}

		if (arsp->rel_isdesc->is_file)
			ifl_name = arsp->rel_isdesc->is_file->ifl_name;
		else
			ifl_name = MSG_INTL(MSG_STR_NULL);

		/*
		 * Make sure we have data to relocate.  Compiler and assembler
		 * developers have been known to generate relocations against
		 * invalid sections (normally .bss), so for their benefit give
		 * them sufficient information to help analyze the problem.
		 * End users should never see this.
		 */
		if (arsp->rel_isdesc->is_indata->d_buf == 0) {
			Conv_inv_buf_t inv_buf;

			ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_REL_EMPTYSEC),
			    conv_reloc_aarch64_type(arsp->rel_rtype, 0,
			    &inv_buf),
			    ifl_name, ld_reloc_sym_name(arsp),
			    EC_WORD(arsp->rel_isdesc->is_scnndx),
			    arsp->rel_isdesc->is_name);
			return (S_ERROR);
		}

		/*
		 * Get the address of the data item we need to modify.
		 */
		addr = (uchar_t *)((uintptr_t)arsp->rel_roffset +
		    (uintptr_t)_elf_getxoff(arsp->rel_isdesc->is_indata));

		DBG_CALL(Dbg_reloc_doact(ofl->ofl_lml, ELF_DBG_LD_ACT,
		    M_MACH, SHT_RELA, arsp, EC_NATPTR(addr), value,
		    ld_reloc_sym_name));
		addr += (uintptr_t)osp->os_outdata->d_buf;

		if ((((uintptr_t)addr - (uintptr_t)ofl->ofl_nehdr) >
		    ofl->ofl_size) || (arsp->rel_roffset >
		    osp->os_shdr->sh_size)) {
			int		class;
			Conv_inv_buf_t inv_buf;

			if (((uintptr_t)addr - (uintptr_t)ofl->ofl_nehdr) >
			    ofl->ofl_size)
				class = ERR_FATAL;
			else
				class = ERR_WARNING;

			ld_eprintf(ofl, class, MSG_INTL(MSG_REL_INVALOFFSET),
			    conv_reloc_aarch64_type(arsp->rel_rtype, 0,
			    &inv_buf),
			    ifl_name, EC_WORD(arsp->rel_isdesc->is_scnndx),
			    arsp->rel_isdesc->is_name, ld_reloc_sym_name(arsp),
			    EC_ADDR((uintptr_t)addr -
			    (uintptr_t)ofl->ofl_nehdr));

			if (class == ERR_FATAL) {
				return_code = S_ERROR;
				continue;
			}
		}

		/*
		 * The relocation is additive.  Ignore the previous symbol
		 * value if this local partial symbol is expanded.
		 */
		if (moved)
			value -= *addr;

		/*
		 * If '-z noreloc' is specified - skip the do_reloc_ld stage.
		 */
		if (OFL_DO_RELOC(ofl)) {
			/*
			 * If this is a PROGBITS section and the running linker
			 * has a different byte order than the target host,
			 * tell do_reloc_ld() to swap bytes.
			 */
			if (do_reloc_ld(arsp, addr, &value, ld_reloc_sym_name,
			    ifl_name, OFL_SWAP_RELOC_DATA(ofl, arsp),
			    ofl->ofl_lml) == 0) {
				ofl->ofl_flags |= FLG_OF_FATAL;
				return_code = S_ERROR;
			}
		}

	}

	return (return_code);
}

static uintptr_t
ld_add_outrel(Word flags, Rel_desc *rsp, Ofl_desc *ofl)
{
	Rel_desc	*orsp;
	Sym_desc	*sdp = rsp->rel_sym;

	/*
	 * Static executables *do not* want any relocations against them.
	 * Since our engine still creates relocations against a WEAK UNDEFINED
	 * symbol in a static executable, it's best to disable them here
	 * instead of through out the relocation code.
	 */
	if (OFL_IS_STATIC_EXEC(ofl))
		return (1);

	/*
	 * If the symbol will be reduced, we can't leave outstanding
	 * relocations against it, as nothing will ever be able to satisfy them
	 * (and the symbol won't be in .dynsym)
	 */
	if ((sdp != NULL) &&
	    (sdp->sd_sym->st_shndx == SHN_UNDEF) &&
	    (rsp->rel_rtype != M_R_NONE) &&
	    (rsp->rel_rtype != M_R_RELATIVE)) {
		if (ld_sym_reducable(ofl, sdp)) {
			return (1);
		}
	}

	if (sdp && (rsp->rel_rtype != M_R_RELATIVE) &&
	    ((flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION))) {

		/*
		 * If this is a COMMON symbol - no output section
		 * exists yet - (it's created as part of sym_validate()).
		 * So - we mark here that when it's created it should
		 * be tagged with the FLG_OS_OUTREL flag.
		 */
		if ((sdp->sd_flags & FLG_SY_SPECSEC) &&
		    (sdp->sd_sym->st_shndx == SHN_COMMON)) {
			if (ELF_ST_TYPE(sdp->sd_sym->st_info) != STT_TLS)
				ofl->ofl_flags1 |= FLG_OF1_BSSOREL;
			else
				ofl->ofl_flags1 |= FLG_OF1_TLSOREL;
		} else {
			Os_desc *osp;
			Is_desc *isp = sdp->sd_isc;

			if (isp && ((osp = isp->is_osdesc) != NULL) &&
			    ((osp->os_flags & FLG_OS_OUTREL) == 0)) {
				ofl->ofl_dynshdrcnt++;
				osp->os_flags |= FLG_OS_OUTREL;
			}
		}
	}

	/* Enter it into the output relocation cache */
	if ((orsp = ld_reloc_enter(ofl, &ofl->ofl_outrels, rsp, flags)) == NULL)
		return (S_ERROR);

	if (flags & FLG_REL_GOT)
		ofl->ofl_relocgotsz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_PLT)
		ofl->ofl_relocpltsz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_BSS)
		ofl->ofl_relocbsssz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_NOINFO)
		ofl->ofl_relocrelsz += (Xword)sizeof (Rela);
	else
		RELAUX_GET_OSDESC(orsp)->os_szoutrels += (Xword)sizeof (Rela);

	if (orsp->rel_rtype == M_R_RELATIVE)
		ofl->ofl_relocrelcnt++;

	/*
	 * We don't perform sorting on PLT relocations because
	 * they have already been assigned a PLT index and if we
	 * were to sort them we would have to re-assign the plt indexes.
	 */
	if (!(flags & FLG_REL_PLT))
		ofl->ofl_reloccnt++;

	/*
	 * Insure a GLOBAL_OFFSET_TABLE is generated if required.
	 */
	if (IS_GOT_REQUIRED(orsp->rel_rtype))
		ofl->ofl_flags |= FLG_OF_BLDGOT;

	/*
	 * Identify and possibly warn of a displacement relocation.
	 */
	if (orsp->rel_flags & FLG_REL_DISP) {
		ofl->ofl_dtflags_1 |= DF_1_DISPRELPND;

		if (ofl->ofl_flags & FLG_OF_VERBOSE)
			ld_disp_errmsg(MSG_INTL(MSG_REL_DISPREL4), orsp, ofl);
	}
	DBG_CALL(Dbg_reloc_ors_entry(ofl->ofl_lml, ELF_DBG_LD, SHT_RELA,
	    M_MACH, orsp));
	return (1);
}

/*
 * Many AArch64 relocations are in pairs where the first selects the page (or
 * the like), and the second selects an offset into that page.
 *
 * return true if we're (conventionally) the 2nd of such a pair, and thus
 * don't compromise PIC.
 */
static inline Boolean
reloc_is_low_bits(Rel_desc *rsp)
{
	switch (rsp->rel_rtype) {
	case R_AARCH64_ADD_ABS_LO12_NC:
	case R_AARCH64_LDST8_ABS_LO12_NC:
	case R_AARCH64_LDST16_ABS_LO12_NC:
	case R_AARCH64_LDST32_ABS_LO12_NC:
	case R_AARCH64_LDST64_ABS_LO12_NC:
	case R_AARCH64_LDST128_ABS_LO12_NC:
		return (TRUE);
	default:
		return (FALSE);
	}
}

/*
 * process relocation for a LOCAL symbol
 */
static uintptr_t
ld_reloc_local(Rel_desc *rsp, Ofl_desc *ofl)
{
	ofl_flag_t	flags = ofl->ofl_flags;
	Sym_desc	*sdp = rsp->rel_sym;
	Word		shndx = sdp->sd_sym->st_shndx;
	Word		ortype = rsp->rel_rtype;

	/*
	 * if ((shared object) and (not pc relative relocation) and
	 *    (not against ABS symbol))
	 * then
	 *	build R_AARCH64_RELATIVE
	 * fi
	 */
	if ((flags & FLG_OF_SHAROBJ) && (rsp->rel_flags & FLG_REL_LOAD) &&
	    !(IS_PC_RELATIVE(rsp->rel_rtype)) && !(IS_SIZE(rsp->rel_rtype)) &&
	    !(IS_GOT_BASED(rsp->rel_rtype)) &&
	    !(reloc_is_low_bits(rsp)) &&
	    !(rsp->rel_isdesc != NULL &&
	    (rsp->rel_isdesc->is_shdr->sh_type == SHT_SUNW_dof)) &&
	    (((sdp->sd_flags & FLG_SY_SPECSEC) == 0) ||
	    (shndx != SHN_ABS) || (sdp->sd_aux && sdp->sd_aux->sa_symspec))) {
		if (reloc_table[ortype].re_fsize != sizeof (Addr)) {
			return (ld_add_outrel(0, rsp, ofl));
		}

		rsp->rel_rtype = R_AARCH64_RELATIVE;
		if (ld_add_outrel(FLG_REL_ADVAL, rsp, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;

		return (1);
	}

	/*
	 * If the relocation is against a 'non-allocatable' section
	 * and we can not resolve it now - then give a warning
	 * message.
	 *
	 * We can not resolve the symbol if either:
	 *	a) it's undefined
	 *	b) it's defined in a shared library and a
	 *	   COPY relocation hasn't moved it to the executable
	 *
	 * Note: because we process all of the relocations against the
	 *	text segment before any others - we know whether
	 *	or not a copy relocation will be generated before
	 *	we get here (see reloc_init()->reloc_segments()).
	 */
	if (!(rsp->rel_flags & FLG_REL_LOAD) &&
	    ((shndx == SHN_UNDEF) ||
	    ((sdp->sd_ref == REF_DYN_NEED) &&
	    ((sdp->sd_flags & FLG_SY_MVTOCOMM) == 0)))) {
		Conv_inv_buf_t	inv_buf;
		Os_desc		*osp = RELAUX_GET_OSDESC(rsp);

		/*
		 * If the relocation is against a SHT_SUNW_ANNOTATE
		 * section - then silently ignore that the relocation
		 * can not be resolved.
		 */
		if (osp && (osp->os_shdr->sh_type == SHT_SUNW_ANNOTATE))
			return (0);
		ld_eprintf(ofl, ERR_WARNING, MSG_INTL(MSG_REL_EXTERNSYM),
		    conv_reloc_aarch64_type(rsp->rel_rtype, 0, &inv_buf),
		    rsp->rel_isdesc->is_file->ifl_name,
		    ld_reloc_sym_name(rsp), osp->os_name);
		return (1);
	}

	/*
	 * Perform relocation.
	 */
	return (ld_add_actrel(0, rsp, ofl));
}

static uintptr_t
ld_reloc_TLS(Boolean local, Rel_desc *rsp, Ofl_desc *ofl)
{
	Word		rtype = rsp->rel_rtype;
	Sym_desc	*sdp = rsp->rel_sym;
	ofl_flag_t	flags = ofl->ofl_flags;
	Gotndx		*gnp;

	/*
	 * If we're building an executable - use either the IE or LE access
	 * model.  If we're building a shared object process the IE model.
	 */
	if ((flags & FLG_OF_EXEC) || (IS_TLS_IE(rtype))) {
		/*
		 * Set the DF_STATIC_TLS flag.
		 */
		ofl->ofl_dtflags |= DF_STATIC_TLS;

		if (!local || ((flags & FLG_OF_EXEC) == 0)) {
			/*
			 * Assign a GOT entry for static TLS references.
			 */
			if ((gnp = ld_find_got_ndx(sdp->sd_GOTndxs,
			    GOT_REF_TLSIE, ofl, rsp)) == NULL) {
				if (ld_assign_got_TLS(local, rsp, ofl, sdp,
				    gnp, GOT_REF_TLSIE, FLG_REL_STLS,
				    rtype, R_AARCH64_TLS_TPREL, 0) == S_ERROR)
					return (S_ERROR);
			}

			/*
			 * IE access model.
			 */
			if (IS_TLS_IE(rtype))
				return (ld_add_actrel(FLG_REL_STLS, rsp, ofl));

			return (ld_add_actrel((FLG_REL_TLSFIX | FLG_REL_STLS),
			    rsp, ofl));
		}

		/*
		 * LE access model.
		 */
		if (IS_TLS_LE(rtype)) {
			return (ld_add_actrel(FLG_REL_STLS, rsp, ofl));
		}

		/* IE access model. */
		if (IS_TLS_IE(rtype)) {
			/*
			 * If this is the first relocation of an IE,
			 * add a GOT entry.
			 */
			if (rtype == R_AARCH64_TLSIE_ADR_GOTTPREL_PAGE21) {
				if ((gnp = ld_find_got_ndx(sdp->sd_GOTndxs,
				    GOT_REF_TLSIE, ofl, rsp)) == NULL) {
					if (ld_assign_got_TLS(local, rsp, ofl,
					    sdp, gnp, GOT_REF_TLSIE,
					    FLG_REL_STLS, rtype,
					    R_AARCH64_TLS_TPREL, 0) == S_ERROR)
						return (S_ERROR);
				}
			}

			return (ld_add_actrel(FLG_REL_TLSFIX | FLG_REL_STLS,
			    rsp, ofl));
		}
	}

	/*
	 * Building a shared object.
	 *
	 * Assign a GOT entry for a dynamic TLS reference.
	 */
	if (IS_TLS_LD(rtype) && ((gnp = ld_find_got_ndx(sdp->sd_GOTndxs,
	    GOT_REF_TLSLD, ofl, rsp)) == NULL)) {
		if (ld_assign_got_TLS(local, rsp, ofl, sdp, gnp, GOT_REF_TLSLD,
		    FLG_REL_MTLS, rtype, R_AARCH64_TLS_DTPMOD, 0) == S_ERROR)
			return (S_ERROR);
	} else if (IS_TLS_GD(rtype) &&
	    ((gnp = ld_find_got_ndx(sdp->sd_GOTndxs, GOT_REF_TLSGD,
	    ofl, rsp)) == NULL)) {
		if (ld_assign_got_TLS(local, rsp, ofl, sdp, gnp, GOT_REF_TLSGD,
		    FLG_REL_DTLS, rtype, R_AARCH64_TLS_DTPMOD,
		    R_AARCH64_TLS_DTPREL) == S_ERROR)
			return (S_ERROR);
	}

	if (IS_TLS_LD(rtype))
		return (ld_add_actrel(FLG_REL_MTLS, rsp, ofl));

	return (ld_add_actrel(FLG_REL_DTLS, rsp, ofl));
}

static uintptr_t
ld_assign_got_ndx(Alist **alpp, Gotndx *pgnp, Gotref gref, Ofl_desc *ofl,
    Rel_desc *rsp, Sym_desc *sdp)
{
	Gotndx		gn, *gnp;
	Aliste		idx;
	uint_t		gotents;
	Xword		raddend = rsp->rel_raddend;

	if (pgnp && (pgnp->gn_addend == raddend) && (pgnp->gn_gotref == gref))
		return (1);

	if ((gref == GOT_REF_TLSGD) || (gref == GOT_REF_TLSLD)) {
		gotents = 2;
	} else {
		gotents = 1;
	}

	gn.gn_addend = raddend;
	gn.gn_gotndx = ofl->ofl_gotcnt;
	gn.gn_gotref = gref;

	ofl->ofl_gotcnt += gotents;

	if (gref == GOT_REF_TLSLD) {
		assert(0 && "TLS LD GOT entries not implemented");
		if (ofl->ofl_tlsldgotndx == NULL) {
			if ((gnp = libld_malloc(sizeof (Gotndx))) == NULL)
				return (S_ERROR);
			(void) memcpy(gnp, &gn, sizeof (Gotndx));
			ofl->ofl_tlsldgotndx = gnp;
		}
		return (1);
	}

	idx = 0;
	for (ALIST_TRAVERSE(*alpp, idx, gnp)) {
		if (gnp->gn_addend > raddend)
			break;
	}

	/*
	 * GOT indexes are maintained on an Alist, where there is typically
	 * only one index.  The usage of this list is to scan the list to find
	 * an index, and then apply that index immediately to a relocation.
	 * Thus there are no external references to these GOT index structures
	 * that can be compromised by the Alist being reallocated.
	 */
	if (alist_insert(alpp, &gn, sizeof (Gotndx),
	    AL_CNT_SDP_GOT, idx) == NULL)
		return (S_ERROR);

	return (0);
}

static void
ld_assign_plt_ndx(Sym_desc * sdp, Ofl_desc *ofl)
{
	sdp->sd_aux->sa_PLTndx = 1 + ofl->ofl_pltcnt++;
	ofl->ofl_flags |= FLG_OF_BLDGOT;
}

/*
 * Initializes .got[0] with the _DYNAMIC symbol value.
 */
static uintptr_t
ld_fillin_gotplt(Ofl_desc *ofl)
{
	int	bswap = (ofl->ofl_flags1 & FLG_OF1_ENCDIFF) != 0;

	if (ofl->ofl_osgot) {
		Sym_desc	*sdp;

		/*
		 * XXARM:
		 * Linux has
		 * .got[0] -> _DYNAMIC
		 * .got[1] -> __gmon_start__@GLOB_DAT
		 * .got.plt[2] -> NULL
		 * .got.plt[3] -> NULL
		 * .got.plt[4] -> NULL
		 * .got.plt[5] -> __libc_start_main@JUMP_SLOT
		 * .got.plt[6] -> __gmon__start@JUMP_SLOT
		 * .got.plt[7] -> abort@JUMP_SLOT
		 */

		if ((sdp = ld_sym_find(MSG_ORIG(MSG_SYM_DYNAMIC_U),
		    SYM_NOHASH, NULL, ofl)) != NULL) {
			uchar_t	*genptr;

			genptr = ((uchar_t *)ofl->ofl_osgot->os_outdata->d_buf +
			    (M_GOT_XDYNAMIC * M_GOT_ENTSIZE));
			/* LINTED */
			*(Xword *)genptr = sdp->sd_sym->st_value;
			if (bswap)
				/* LINTED */
				*(Xword *)genptr =
				    /* LINTED */
				    ld_bswap_Xword(*(Xword *)genptr);
		}
	}

	/*
	 * Fill in the reserved slot in the procedure linkage table the first.
	 */
	if ((ofl->ofl_flags & FLG_OF_DYNAMIC) && ofl->ofl_osplt) {
		uchar_t *pltent = ofl->ofl_osplt->os_outdata->d_buf;
		Addr	got = ofl->ofl_osgotplt->os_shdr->sh_addr;
		Addr	plt = ofl->ofl_osplt->os_shdr->sh_addr;

		memcpy(ofl->ofl_osplt->os_outdata->d_buf, plt0_entry,
		    sizeof (plt0_entry));

		/*
		 * If '-z noreloc' is specified - skip the do_reloc_ld
		 * stage.
		 */
		if (!OFL_DO_RELOC(ofl))
			return (1);

		static Rel_desc rdesc_prel_pg_hi21 = { NULL, NULL, NULL,
			0, 0, 0, R_AARCH64_ADR_PREL_PG_HI21 };
		Xword		val1;

		val1 = AARCH64_PAGE(got + 16) - AARCH64_PAGE(plt + 4);

		if (do_reloc_ld(&rdesc_prel_pg_hi21, &pltent[4], &val1,
		    syn_rdesc_sym_name, MSG_ORIG(MSG_SPECFIL_PLTENT), bswap,
		    ofl->ofl_lml) == 0) {
			ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_PLT_PLT0FAIL));
			return (S_ERROR);
		}

		static Rel_desc rdesc_ldst64_abs_lo12_nc = { NULL, NULL, NULL,
			0, 0, 0, R_AARCH64_LDST64_ABS_LO12_NC};
		val1 = got + 16;
		if (do_reloc_ld(&rdesc_ldst64_abs_lo12_nc, &pltent[8], &val1,
		    syn_rdesc_sym_name, MSG_ORIG(MSG_SPECFIL_PLTENT), bswap,
		    ofl->ofl_lml) == 0) {
			ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_PLT_PLT0FAIL));
			return (S_ERROR);
		}

		static Rel_desc rdesc_add_abs_lo12_nc = { NULL, NULL, NULL,
			0, 0, 0, R_AARCH64_ADD_ABS_LO12_NC};
		val1 = got + 16;
		if (do_reloc_ld(&rdesc_add_abs_lo12_nc, &pltent[12], &val1,
		    syn_rdesc_sym_name, MSG_ORIG(MSG_SPECFIL_PLTENT), bswap,
		    ofl->ofl_lml) == 0) {
			ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_PLT_PLT0FAIL));
			return (S_ERROR);
		}
	}

	return (1);
}

/*
 * Template for generating "void (*)(void)" function
 * XXXARM
 */
static const uchar_t nullfunc_tmpl[] = {
/* 0x00 */	0x00,
};

/*
 * Return the ld_targ definition for this target.
 */
const Target *
ld_targ_init_aarch64(void)
{
	static const Target _ld_targ = {
		.t_m = {
			.m_mach = M_MACH,
			.m_machplus = M_MACHPLUS,
			.m_flagsplus = M_FLAGSPLUS,
			.m_class = M_CLASS,
			.m_data = M_DATA,

			.m_segm_align = M_SEGM_ALIGN,
			.m_segm_origin = M_SEGM_ORIGIN,
			.m_segm_aorigin = M_SEGM_AORIGIN,
			.m_dataseg_perm = M_DATASEG_PERM,
			.m_stack_perm = M_STACK_PERM,
			.m_word_align = M_WORD_ALIGN,
			.m_def_interp = MSG_ORIG(MSG_PTH_RTLD_AARCH64),

			.m_needs_gotplt = TRUE,

			/* Relocation type codes */
			.m_r_arrayaddr = M_R_ARRAYADDR,
			.m_r_copy = M_R_COPY,
			.m_r_glob_dat = M_R_GLOB_DAT,
			.m_r_jmp_slot = M_R_JMP_SLOT,
			.m_r_num = M_R_NUM,
			.m_r_none = M_R_NONE,
			.m_r_relative = M_R_RELATIVE,
			.m_r_register = M_R_REGISTER,

			/* Relocation related constants */
			.m_rel_dt_count = M_REL_DT_COUNT,
			.m_rel_dt_ent = M_REL_DT_ENT,
			.m_rel_dt_size = M_REL_DT_SIZE,
			.m_rel_dt_type = M_REL_DT_TYPE,
			.m_rel_sht_type = M_REL_SHT_TYPE,

			/* GOT related constants */
			.m_got_entsize = M_GOT_ENTSIZE,
			.m_got_xnumber = M_GOT_XNumber,

			/* PLT related constants */
			.m_plt_align = M_PLT_ALIGN,
			.m_plt_entsize = M_PLT_ENTSIZE,
			.m_plt_reservsz = M_PLT_RESERVSZ,
			.m_plt_shf_flags = M_PLT_SHF_FLAGS,

			/* Section type of .eh_frame/.eh_frame_hdr sections */
			.m_sht_unwind = SHT_PROGBITS,

			.m_dt_register = M_DT_REGISTER,
		},
		.t_id = {
			.id_array = M_ID_ARRAY,
			.id_bss = M_ID_BSS,
			.id_cap = M_ID_CAP,
			.id_capinfo = M_ID_CAPINFO,
			.id_capchain = M_ID_CAPCHAIN,
			.id_data = M_ID_DATA,
			.id_dynamic = M_ID_DYNAMIC,
			.id_dynsort = M_ID_DYNSORT,
			.id_dynstr = M_ID_DYNSTR,
			.id_dynsym = M_ID_DYNSYM,
			.id_dynsym_ndx = M_ID_DYNSYM_NDX,
			.id_got = M_ID_GOT,
			.id_gotdata = M_ID_UNKNOWN, /* unused */
			.id_hash = M_ID_HASH,
			.id_interp = M_ID_INTERP,
			.id_lbss = M_ID_UNKNOWN, /* unused */
			.id_ldynsym = M_ID_LDYNSYM,
			.id_note = M_ID_NOTE,
			.id_null = M_ID_NULL,
			.id_plt = M_ID_PLT,
			.id_rel = M_ID_REL,
			.id_strtab = M_ID_STRTAB,
			.id_syminfo = M_ID_SYMINFO,
			.id_symtab = M_ID_SYMTAB,
			.id_symtab_ndx = M_ID_SYMTAB_NDX,
			.id_text = M_ID_TEXT,
			.id_tls = M_ID_TLS,
			.id_tlsbss = M_ID_TLSBSS,
			.id_unknown = M_ID_UNKNOWN,
			.id_unwind = M_ID_UNWIND,
			.id_unwindhdr = M_ID_UNWINDHDR,
			.id_user = M_ID_USER,
			.id_version = M_ID_VERSION,
		},
		.t_nf = {
			.nf_template = nullfunc_tmpl,
			.nf_size = sizeof (nullfunc_tmpl),
		},
		.t_ff = {
			.ff_execfill = NULL,
		},
		.t_mr = {
			.mr_reloc_table = reloc_table,
			.mr_init_rel = ld_init_rel,
			.mr_mach_eflags = ld_mach_eflags,
			.mr_mach_make_dynamic = ld_mach_make_dynamic,
			.mr_mach_update_odynamic = ld_mach_update_odynamic,
			.mr_calc_plt_addr = ld_calc_plt_addr,
			.mr_perform_outreloc = ld_perform_outreloc,
			.mr_do_activerelocs = ld_do_activerelocs,
			.mr_add_outrel = ld_add_outrel,
			.mr_reloc_register = NULL,
			.mr_reloc_local = ld_reloc_local,
			.mr_reloc_GOTOP = NULL,
			.mr_reloc_TLS = ld_reloc_TLS,
			.mr_assign_got = NULL,
			.mr_find_got_ndx = ld_find_got_ndx,
			.mr_calc_got_offset = ld_calc_got_offset,
			.mr_assign_got_ndx = ld_assign_got_ndx,
			.mr_assign_plt_ndx = ld_assign_plt_ndx,
			.mr_allocate_got = NULL,
			.mr_fillin_gotplt = ld_fillin_gotplt,
		},
		.t_ms = {
			.ms_reg_check = NULL,
			.ms_mach_sym_typecheck = NULL,
			.ms_is_regsym = NULL,
			.ms_reg_find = NULL,
			.ms_reg_enter = NULL	/* ms_reg_enter */
		}
	};

	return (&_ld_targ);
}
