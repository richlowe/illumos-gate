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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2013 by Delphix. All rights reserved.
 */

#include	<errno.h>
#include	<dt_link.h>
#include	"msg.h"
#include	"_libld.h"

#ifndef _NATIVE_BUILD
static const char *dt_symprefix = MSG_ORIG(MSG_DTRACE_PROBE_PREFIX);
static const char *dt_symfmt = MSG_ORIG(MSG_DTRACE_PROBE_FMT);


/*
 * Note that we don't just look up in our own symbol table, because we want to
 * use the specific symtab mentioned by a relocation section, not a general
 * one.
 */
static int
symtab_match_symbol(Is_desc *symtab, uintptr_t addr, uint_t shn,
    GElf_Sym *sym)
{
	int i, ret = -1;
	GElf_Sym s;
	size_t nsym = symtab->is_indata->d_size / sizeof (Sym);

	for (i = 0; i < nsym && gelf_getsym(symtab->is_indata, i,
	    sym) != NULL; i++) {
		if (ELF_ST_TYPE(sym->st_info) == STT_FUNC &&
		    shn == sym->st_shndx &&
		    sym->st_value <= addr &&
		    addr < sym->st_value + sym->st_size) {
			if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL)
				return (0);

			ret = 0;
			s = *sym;
		}
	}

	if (ret == 0)
		*sym = s;
	return (ret);
}

static GElf_Rela *
read_reloc(Is_desc *relsec, int ndx, GElf_Rela *rel)
{
	GElf_Rel r;

	if (relsec->is_shdr->sh_type == SHT_RELA) {
		return (gelf_getrela(relsec->is_indata, ndx, rel));
	} else {
		if (gelf_getrel(relsec->is_indata, ndx, &r) == NULL)
			return (NULL);

		rel->r_offset = r.r_offset;
		rel->r_info = r.r_info;
		rel->r_addend = 0;
		return (rel);
	}
}

static int
make_is_writable(Ofl_desc *ofl, Is_desc *isp)
{
	void *newtext;

	if (isp->is_newdata == TRUE)
		return (0);

	if ((newtext = libld_malloc(isp->is_indata->d_size)) == NULL) {
		(void) ld_eprintf(ofl, ERR_FATAL, MSG_INTL(MSG_SYS_MALLOC),
		    strerror(errno));
		return (1);
	}

	(void) memcpy(newtext, isp->is_indata->d_buf, isp->is_indata->d_size);
	isp->is_indata->d_buf = newtext;
	isp->is_newdata = TRUE;
	return (0);
}

static char *
create_surrogate_sym(Ofl_desc *ofl, Ifl_desc *ifl, Aliste objkey,
    GElf_Sym *fsym, const char *s)
{
	Sym lsym;
	char *r;

	lsym.st_value = fsym->st_value;
	lsym.st_size = fsym->st_size;
	lsym.st_info = ELF_ST_INFO(STB_GLOBAL,
	    STT_FUNC);
	lsym.st_other =
	    ELF_ST_VISIBILITY(STV_ELIMINATE);
	lsym.st_shndx = fsym->st_shndx;

	if (libld_asprintf(&r, dt_symfmt, dt_symprefix,
	    objkey, s) == -1) {
		(void) ld_eprintf(ofl, ERR_FATAL,
		    MSG_INTL(MSG_SYS_MALLOC),
		    strerror(errno));
	}

	/*
	 * The symbol may already exist from a
	 * previous probe
	 */
	if (ld_sym_find(r, elf_hash(r), NULL, ofl) ==
	    NULL) {
		if (ld_sym_enter(r, &lsym, elf_hash(r),
		    ifl, ofl, 0, lsym.st_shndx,
		    FLG_SY_ELIM, 0) ==
		    (Sym_desc *)S_ERROR) {
			(void) ld_eprintf(ofl, ERR_FATAL,
			    MSG_INTL(MSG_DTRACE_SURR_FAILED),
			    ifl->ifl_name, r, s);
			return (NULL);
		}
	}

	return (r);
}

static Boolean
is_surrogate_sym(char *name)
{
	return (strncmp(name, dt_symprefix, strlen(dt_symprefix) == 0));
}

static char *
surrogate_orig_name(char *surrogate)
{
	char *ret = surrogate;

	if ((ret = strchr(ret, '.')) == NULL)
		return (NULL);
	else
		return (ret++);
}

static int
dprov_process_object(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, Ofl_desc *ofl,
    Ifl_desc *ifl, Aliste objkey)
{
	int ndx, eprobe;

	for (int i = 0; i < ifl->ifl_shnum; i++) {
		Is_desc *rel = ifl->ifl_isdesc[i];
		Is_desc *symtab;
		Is_desc *strtab;
		Is_desc *tgt;
		GElf_Rela rela;
		GElf_Sym rsym, fsym;
		char *symname = NULL;
		char *relocname = NULL;
		dt_provider_t *pvp;
		dt_probe_t *prp;
		uint32_t off;

		if ((rel == NULL) ||
		    ((rel->is_shdr->sh_type != SHT_RELA) &&
		    (rel->is_shdr->sh_type != SHT_REL)))
			continue;

		symtab = ifl->ifl_isdesc[rel->is_shdr->sh_link];
		strtab = ifl->ifl_isdesc[symtab->is_shdr->sh_link];
		tgt = ifl->ifl_isdesc[rel->is_shdr->sh_info];

		/*
		 * We're looking for relocations to symbols
		 * matching this form:
		 *
		 *   __dtrace[enabled]_<prov>___<probe>
		 *
		 * For the generated object, we need to record
		 * the location identified by the relocation,
		 * and create a new relocation in the
		 * generated object that will be resolved at
		 * link time to the location of the function
		 * in which the probe is embedded. In the
		 * target object, we change the matched symbol
		 * so that it will be ignored at link time,
		 * and we modify the target (text) section to
		 * replace the call instruction with one or
		 * more nops.
		 *
		 * If the function containing the probe is
		 * locally scoped (static), we create an alias
		 * used by the relocation in the generated
		 * object. The alias, a new symbol, will be
		 * global (so that the relocation from the
		 * generated object can be resolved), and
		 * hidden (so that it is converted to a local
		 * symbol at link time). Such aliases have
		 * this form:
		 *
		 *   $dtrace<key>.<function>
		 *
		 * We take a first pass through all the
		 * relocations to populate our string table
		 * and count the number of extra symbols we'll
		 * require.
		 */

		for (int i = 0; i < rel->is_shdr->sh_size /
		    rel->is_shdr->sh_entsize; i++) {
			dtrace_probedesc_t pd;

			if (read_reloc(rel, i, &rela) == NULL)
				continue;

			ndx = (Word) GELF_R_SYM(rela.r_info);

			if ((gelf_getsym(symtab->is_indata, ndx, &rsym)
			    == NULL) ||
			    (rsym.st_name > strtab->is_indata->d_size)) {
				(void) ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_ERR_SYMTAB),
				    ifl->ifl_name);
				return (1);
			}

			symname = (char *)strtab->is_indata->d_buf +
			    rsym.st_name;

			switch (dtrace_parse_symbol(symname, &pd, &eprobe)) {
			case -1: /* Error */
				(void) ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_NOT_DSYM),
				    ifl->ifl_name, symname);
				return (1);
			case 1:	/* non-DTrace symbol */
				continue;
			default:
				break;
			}

			if (eprobe != 0) {
				dtrace_program_setversion(dtp, pgp,
				    DOF_VERSION_2);
			}

			/*
			 * If the provider doesn't exist, pass on, we may
			 * encounter a future definition of it.
			 */
			if ((pvp = dt_provider_lookup(dtp, pd.dtpd_provider)) ==
			    NULL)
				continue;

			/* If we know the provider, the probe _must_ exist */
			if ((prp = dt_probe_lookup(pvp, pd.dtpd_name)) ==
			    NULL) {
				(void) ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_NO_PROBE),
				    ifl->ifl_name, pd.dtpd_name,
				    pd.dtpd_provider);
				return (1);
			}

			if (symtab_match_symbol(symtab, rela.r_offset,
			    rel->is_shdr->sh_info, &fsym) == 0) {
				if (fsym.st_name >= strtab->is_indata->d_size) {
					(void) ld_eprintf(ofl, ERR_FATAL,
					    MSG_INTL(MSG_DTRACE_ERR_SYMTAB),
					    ifl->ifl_name);
					return (1);
				}
				symname = (char *)strtab->is_indata->d_buf +
				    fsym.st_name;
			} else {
				(void) ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_OFFSET_NO_SYMBOL),
				    ifl->ifl_name, rela.r_offset);
				return (1);
			}

			assert(ELF_ST_TYPE(fsym.st_info) == STT_FUNC);

			/*
			 * If a NULL relocation name is passed
			 * to dt_probe_define(), the function
			 * name is used for the
			 * relocation. The relocation needs to
			 * use a mangled name if the symbol is
			 * locally scoped.
			 */
			relocname = NULL;

			if (ELF_ST_BIND(fsym.st_info) == STB_LOCAL) {
				relocname = create_surrogate_sym(ofl, ifl,
				    objkey, &fsym, symname);
			} else if (is_surrogate_sym(symname)) {
				relocname = symname;
				if ((symname = surrogate_orig_name(symname)) ==
				    NULL) {
					(void) ld_eprintf(ofl, ERR_FATAL,
					    MSG_INTL(MSG_DTRACE_BAD_SURSYM),
					    ifl->ifl_name, relocname);
					return (1);
				}
			}

			assert(fsym.st_value <= rela.r_offset);

			/*
			 * If the section data is still mapped from the input
			 * object, allocate a modifiable copy.
			 */
			if (make_is_writable(ofl, tgt) != 0)
				return (1);

			off = rela.r_offset - fsym.st_value;
			if (dt_modtext(dtp, tgt->is_indata->d_buf,
			    eprobe, &rela, &off) != 0) {
				(void) ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_NO_MODDTEXT),
				    ifl->ifl_name, pd.dtpd_provider,
				    pd.dtpd_name);
				return (1);
			}

			if (dt_probe_define(pvp, prp, symname, relocname,
			    off, eprobe) != 0) {
				(void) ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_NO_PROBEDEF),
				    ifl->ifl_name, pd.dtpd_provider,
				    pd.dtpd_name);
				return (1);
			}

			/*
			 * For the flags set, see ld_sym_enter's treatment of
			 * SHN_SUNW_IGNORE
			 */
			if ((ifl->ifl_oldndx[ndx]->sd_flags &
			    FLG_SY_IGNORE) == 0) {
				ifl->ifl_oldndx[ndx]->sd_flags |=
				    (FLG_SY_REDUCED | FLG_SY_HIDDEN |
				    FLG_SY_IGNORE | FLG_SY_ELIM);
				ifl->ifl_oldndx[ndx]->sd_shndx =
				    SHN_SUNW_IGNORE;
				ifl->ifl_oldndx[ndx]->sd_sym->st_shndx =
				    SHN_SUNW_IGNORE;
			}
		}
	}
	return (0);
}

static char *
dof_gensym(void)
{
	static int i = 0;
	char *ret = NULL;

	if (libld_asprintf(&ret, MSG_ORIG(MSG_SYM_SUNW_DOF), i++) == -1)
		return (NULL);
	return (ret);
}

static int
register_dof_syms(Ofl_desc *ofl, Ifl_desc *ifl)
{
	for (int ndx = 0; ndx < ifl->ifl_symscnt; ndx++) {
		Sym_desc *sdp = ifl->ifl_oldndx[ndx];

		if (sdp == NULL)
			continue;

		/*
		 * The DOF elf will contain references to
		 * other symbols which, unfortunately, are
		 * already resolved at this point.  Any symbol
		 * whose actual definition is not from this
		 * file should be ignored.
		 */
		if (sdp->sd_isc->is_file != ifl)
			continue;

		if (sdp->sd_shndx >= ifl->ifl_shnum)
			continue;

		if ((ifl->ifl_isdesc[sdp->sd_shndx]->is_shdr->sh_type ==
		    SHT_SUNW_dof)) {
			sdp->sd_flags |= FLG_SY_HIDDEN;

			if (aplist_append(&ofl->ofl_dofarray,
			    sdp, AL_CNT_OFL_ARRAYS) == NULL) {
				ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_DOFREG_FAILED),
				    ofl->ofl_name,
				    sdp->sd_name);
				return (1);
			}
		}
	}

	return (0);
}

int
ld_process_dprov_syms(Ofl_desc *ofl)
{
	Aliste		idx1, idx2;
	Ifl_desc	*ifl;
	int		err;
	int		dtflags = DTRACE_O_NODEV;
	avl_tree_t	script_sym_avl;
	Sym_avlnode *sav;

	if (ld_targ.t_m.m_class == ELFCLASS32)
		dtflags |= DTRACE_O_ILP32;
	else
		dtflags |= DTRACE_O_LP64;

	avl_create(&script_sym_avl, &ld_sym_avl_comp, sizeof (Sym_avlnode),
	    SGSOFFSETOF(Sym_avlnode, sav_node));

	for (APLIST_TRAVERSE(ofl->ofl_objs, idx1, ifl)) {
		for (int i = 0; i < ifl->ifl_symscnt; i++) {
			Sym_desc *sdp = ifl->ifl_oldndx[i];
			char *script = NULL;
			dtrace_prog_t *pgp;
			dof_hdr_t *dof;
			Ifl_desc *difl;
			dtrace_hdl_t *dtp;
			Rej_desc rej = { 0 };
			Elf *elf;
			avl_index_t where;
			char *pseudoname, *dofsym;

			if (sdp == NULL)
				continue;

			if (strncmp(MSG_ORIG(MSG_SYM_DTRACE_SCRIPT),
			    sdp->sd_name, MSG_SYM_DTRACE_SCRIPT_SIZE) != 0)
				continue;

			sav = libld_malloc(sizeof (Sym_avlnode));
			sav->sav_sdp = sdp;
			sav->sav_hash = elf_hash(sdp->sd_name);
			sav->sav_name = sdp->sd_name;

			if (avl_find(&script_sym_avl, sav, &where) != NULL) {
				libld_free(sav);
				continue;
			} else {
				avl_insert(&script_sym_avl, sav, where);
			}

			if (!(sdp->sd_flags & FLG_SY_ISDISC)) {
				sdp->sd_flags |= (FLG_SY_HIDDEN |
				    FLG_SY_IGNORE | FLG_SY_REDUCED |
				    FLG_SY_ELIM);
				sdp->sd_shndx = SHN_SUNW_IGNORE;
				sdp->sd_sym->st_shndx = SHN_SUNW_IGNORE;
			}

			script = dt_header_script_decode(
			    (char *)sdp->sd_isc->is_indata->d_buf +
			    sdp->sd_sym->st_value);

			if ((dtp = dtrace_open(DTRACE_VERSION, dtflags,
			    &err)) == NULL) {
				ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_OPEN_FAILED),
				    ofl->ofl_name, dtrace_errmsg(NULL, err));
				return (1);
			}

			/*
			 * enable -xlink=dynamic and -xunodefs to permit
			 * undefined references
			 */
			(void) dtrace_setopt(dtp, "linkmode", "dynamic");
			(void) dtrace_setopt(dtp, "unodefs", NULL);

			if ((pgp = dtrace_program_strcompile(dtp, script,
			    DTRACE_PROBESPEC_NAME,  DTRACE_C_ZDEFS,
			    0, NULL)) == NULL) {
				ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_COMPILE_FAILED),
				    ofl->ofl_name, sdp->sd_name,
				    dtrace_errmsg(dtp, dtrace_errno(dtp)));
				dtrace_close(dtp);
				return (1);
			}

			for (APLIST_TRAVERSE(ofl->ofl_objs, idx2, ifl)) {
				/*
				 * dtrace(1M) would use ftok(3C) to generate
				 * the objkey used when generating surrogate
				 * global symbols for local functions.
				 * Because we have perfect knowledge (the
				 * symbols never escape the link edit), we
				 * don't need even the pretence of a GUID, and
				 * can use a simple counter.
				 */
				if (dprov_process_object(dtp, pgp, ofl,
				    ifl, idx2) != 0) {
					(void) ld_eprintf(ofl, ERR_FATAL,
					    MSG_INTL(MSG_DTRACE_OBJMOD_FAILED),
					    ofl->ofl_name, ifl->ifl_name);
					dtrace_close(dtp);
					return (1);
				}
			}

			if ((dof = dtrace_dof_create(dtp, pgp,
			    DTRACE_D_PROBES)) == NULL) {
				ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_DOF_FAILED),
				    ofl->ofl_name,  sdp->sd_name,
				    dtrace_errmsg(dtp, dtrace_errno(dtp)));
				dtrace_close(dtp);
				return (1);
			}

			/*
			 * DOF generated in the link-header has no dedicated
			 * drti
			 */
			dof->dofh_rtflags |= DOF_RTFL_NODRTI;

			/*
			 * XXX: The support for symbol naming, etc, is
			 * super-ungraceful both here and in libdtrace.
			 */
			if ((dofsym = dof_gensym()) == NULL) {
				ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_SYS_MALLOC),
				    strerror(errno));
			}

			if ((elf = dtrace_dof_elf(dtp, dof,
			    dofsym)) == NULL) {
				ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_OBJ_FAILED),
				    ofl->ofl_name, sdp->sd_name,
				    dtrace_errmsg(dtp, dtrace_errno(dtp)));
				dtrace_dof_destroy(dtp, dof);
				dtrace_close(dtp);
				return (1);
			}

			if (libld_asprintf(&pseudoname,
			    MSG_INTL(MSG_DTRACE_STR_EMBSCRIPT),
			    sdp->sd_name) == -1)
				ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_SYS_MALLOC), strerror(errno));

			switch (ld_process_ifl(pseudoname,
			    NULL, 0, elf, FLG_IF_EXTRACT, ofl,
			    &rej, &difl)) {
			case 0:	{	/* Rejection */
				Conv_reject_desc_buf_t rej_buf;

				assert(rej.rej_type != 0);

				ld_eprintf(ofl, ERR_WARNING,
				    MSG_INTL(reject[rej.rej_type]),
				    rej.rej_name ? rej.rej_name :
				    MSG_INTL(MSG_STR_UNKNOWN),
				    conv_reject_desc(&rej, &rej_buf,
				    ld_targ.t_m.m_mach));
				dtrace_dof_destroy(dtp, dof);
				dtrace_close(dtp);
				return (1);
			}
			case S_ERROR:	/* File is completely unintelligible */
				ld_eprintf(ofl, ERR_FATAL,
				    MSG_INTL(MSG_DTRACE_OBJ_BAD),
				    ofl->ofl_name, sdp->sd_name);
				dtrace_dof_destroy(dtp, dof);
				dtrace_close(dtp);
				return (1);
			case 1:		/* Everything is fine */
				break;
			}

			/* Bind the DOF object directly */
			difl->ifl_flags |= FLG_IF_DIRECT;

			/*
			 * Add any DOF symbols we just created to the dof_array,
			 * and make them local
			 */

			if (register_dof_syms(ofl, difl) != 0) {
				dtrace_dof_destroy(dtp, dof);
				dtrace_close(dtp);
				return (1);
			}

			dtrace_dof_destroy(dtp, dof);
			dtrace_close(dtp);
		}
	}

	void *cookie = NULL;
	while ((sav = avl_destroy_nodes(&script_sym_avl, &cookie)) != NULL)
		libld_free(sav);
	avl_destroy(&script_sym_avl);

	return (0);
}

#endif	/* _NATIVE_BUILD */
