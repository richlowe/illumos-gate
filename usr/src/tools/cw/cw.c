
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
 * Copyright 2018, Richard Lowe.
 */
/*
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2019 Joyent, Inc.
 */

/*
 * Wrapper for the GNU C compiler to make it accept the Sun C compiler
 * arguments where possible.
 *
 * Since the translation is inexact, this is something of a work-in-progress.
 *
 */

/* If you modify this file, you must increment CW_VERSION */
#define	CW_VERSION	"5.0"

/*
 * -#		Verbose mode
 * -###		Show compiler commands built by driver, no compilation
 * -A<name[(tokens)]>	Preprocessor predicate assertion
 * -B<[static|dynamic]>	Specify dynamic or static binding
 * -C		Prevent preprocessor from removing comments
 * -c		Compile only - produce .o files, suppress linking
 * -cg92	Alias for -xtarget=ss1000
 * -D<name[=token]>	Associate name with token as if by #define
 * -d[y|n]	dynamic [-dy] or static [-dn] option to linker
 * -E		Compile source through preprocessor only, output to stdout
 * -erroff=<t>	Suppress warnings specified by tags t(%none, %all, <tag list>)
 * -errtags=<a>	Display messages with tags a(no, yes)
 * -errwarn=<t>	Treats warnings specified by tags t(%none, %all, <tag list>)
 *		as errors
 * -fast	Optimize using a selection of options
 * -fd		Report old-style function definitions and declarations
 * -fnonstd	Initialize floating-point hardware to non-standard preferences
 * -fns[=<yes|no>] Select non-standard floating point mode
 * -fprecision=<p> Set FP rounding precision mode p(single, double, extended)
 * -fround=<r>	Select the IEEE rounding mode in effect at startup
 * -fsimple[=<n>] Select floating-point optimization preferences <n>
 * -fsingle	Use single-precision arithmetic (-Xt and -Xs modes only)
 * -ftrap=<t>	Select floating-point trapping mode in effect at startup
 * -fstore	force floating pt. values to target precision on assignment
 * -G		Build a dynamic shared library
 * -g		Compile for debugging
 * -H		Print path name of each file included during compilation
 * -h <name>	Assign <name> to generated dynamic shared library
 * -I<dir>	Add <dir> to preprocessor #include file search path
 * -i		Passed to linker to ignore any LD_LIBRARY_PATH setting
 * -keeptmp	Keep temporary files created during compilation
 * -L<dir>	Pass to linker to add <dir> to the library search path
 * -l<name>	Link with library lib<name>.a or lib<name>.so
 * -mc		Remove duplicate strings from .comment section of output files
 * -mr		Remove all strings from .comment section of output files
 * -mr,"string"	Remove all strings and append "string" to .comment section
 * -mt		Specify options needed when compiling multi-threaded code
 * -native	Find available processor, generate code accordingly
 * -nofstore	Do not force floating pt. values to target precision
 *		on assignment
 * -norunpath	Do not build in a runtime path for shared libraries
 * -O		Use default optimization level (-xO2 or -xO3. Check man page.)
 * -o <outputfile> Set name of output file to <outputfile>
 * -P		Compile source through preprocessor only, output to .i  file
 * -p		Compile for profiling with prof
 * -Q[y|n]	Emit/don't emit identification info to output file
 * -R<dir[:dir]> Build runtime search path list into executable
 * -S		Compile and only generate assembly code (.s)
 * -s		Strip symbol table from the executable file
 * -t		Turn off duplicate symbol warnings when linking
 * -U<name>	Delete initial definition of preprocessor symbol <name>
 * -V		Report version number of each compilation phase
 * -v		Do stricter semantic checking
 * -W<c>,<arg>	Pass <arg> to specified component <c> (a,l,m,p,0,2,h,i,u)
 * -w		Suppress compiler warning messages
 * -Xa		Compile assuming ANSI C conformance, allow K & R extensions
 *		(default mode)
 * -Xs		Compile assuming (pre-ANSI) K & R C style code
 * -Xt		Compile assuming K & R conformance, allow ANSI C
 * -xarch=<a>	Specify target architecture instruction set
 * -xbuiltin[=<b>] When profitable inline, or substitute intrinisic functions
 *		for system functions, b={%all,%none}
 * -xCC		Accept C++ style comments
 * -xchip=<c>	Specify the target processor for use by the optimizer
 * -xcode=<c>	Generate different code for forming addresses
 * -xcrossfile[=<n>] Enable optimization and inlining across source files,
 *		n={0|1}
 * -xe		Perform only syntax/semantic checking, no code generation
 * -xF		Compile for later mapfile reordering or unused section
 *		elimination
 * -xhelp=<f>	Display on-line help information f(flags, readme, errors)
 * -xildoff	Cancel -xildon
 * -xildon	Enable use of the incremental linker, ild
 * -xinline=[<a>,...,<a>]  Attempt inlining of specified user routines,
 *		<a>={%auto,func,no%func}
 * -xlibmieee	Force IEEE 754 return values for math routines in
 *		exceptional cases
 * -xlibmil	Inline selected libm math routines for optimization
 * -xlic_lib=sunperf	Link in the Sun supplied performance libraries
 * -xlicinfo	Show license server information
 * -xmaxopt=[off,1,2,3,4,5] maximum optimization level allowed on #pragma opt
 * -xO<n>	Generate optimized code (n={1|2|3|4|5})
 * -xP		Print prototypes for function definitions
 * -xprofile=<p> Collect data for a profile or use a profile to optimize
 *		<p>={{collect,use}[:<path>],tcov}
 * -xregs=<r>	Control register allocation
 * -xs		Allow debugging without object (.o) files
 * -xsb		Compile for use with the WorkShop source browser
 * -xsbfast	Generate only WorkShop source browser info, no compilation
 * -xsfpconst	Represent unsuffixed floating point constants as single
 *		precision
 * -xspace	Do not do optimizations that increase code size
 * -xstrconst	Place string literals into read-only data segment
 * -xtemp=<dir>	Set directory for temporary files to <dir>
 * -xtime	Report the execution time for each compilation phase
 * -xunroll=n	Enable unrolling loops n times where possible
 * -Y<c>,<dir>	Specify <dir> for location of component <c> (a,l,m,p,0,h,i,u)
 * -YA,<dir>	Change default directory searched for components
 * -YI,<dir>	Change default directory searched for include files
 * -YP,<dir>	Change default directory for finding libraries files
 * -YS,<dir>	Change default directory for startup object files
 */

/*
 * Translation table:
 */
/*
 * -#				-v
 * -###				error
 * -A<name[(tokens)]>		pass-thru
 * -B<[static|dynamic]>		pass-thru (syntax error for anything else)
 * -C				pass-thru
 * -c				pass-thru
 * -cg92			-m32 -mcpu=v8 -mtune=supersparc (SPARC only)
 * -D<name[=token]>		pass-thru
 * -dy or -dn			-Wl,-dy or -Wl,-dn
 * -E				pass-thru
 * -erroff=E_EMPTY_TRANSLATION_UNIT ignore
 * -errtags=%all		-Wall
 * -errwarn=%all		-Werror else -Wno-error
 * -fast			error
 * -fd				error
 * -fnonstd			error
 * -fns[=<yes|no>]		error
 * -fprecision=<p>		error
 * -fround=<r>			error
 * -fsimple[=<n>]		error
 * -fsingle[=<n>]		error
 * -ftrap=<t>			error
 * -fstore			error
 * -G				pass-thru
 * -g				pass-thru
 * -H				pass-thru
 * -h <name>			pass-thru
 * -I<dir>			pass-thru
 * -i				pass-thru
 * -keeptmp			-save-temps
 * -L<dir>			pass-thru
 * -l<name>			pass-thru
 * -mc				error
 * -mr				error
 * -mr,"string"			error
 * -mt				-D_REENTRANT
 * -native			error
 * -nofstore			error
 * -nolib			-nodefaultlibs
 * -norunpath			ignore
 * -O				-O1 (Check the man page to be certain)
 * -o <outputfile>		pass-thru
 * -P				-E -o filename.i (or error)
 * -p				pass-thru
 * -Q[y|n]			error
 * -R<dir[:dir]>		pass-thru
 * -S				pass-thru
 * -s				-Wl,-s
 * -t				-Wl,-t
 * -U<name>			pass-thru
 * -V				--version
 * -v				-Wall
 * -Wa,<arg>			pass-thru
 * -Wp,<arg>			pass-thru except -xc99=<a>
 * -Wl,<arg>			pass-thru
 * -W{m,0,2,h,i,u>		error/ignore
 * -xmodel=kernel		-ffreestanding -mcmodel=kernel -mno-red-zone
 * -Wu,-save_args		-msave-args
 * -w				pass-thru
 * -Xa				-std=iso9899:199409 or -ansi
 * -Xt				error
 * -Xs				-traditional -std=c89
 * -xarch=<a>			table
 * -xbuiltin[=<b>]		-fbuiltin (-fno-builtin otherwise)
 * -xCC				ignore
 * -xchip=<c>			table
 * -xcode=<c>			table
 * -xcrossfile[=<n>]		ignore
 * -xe				error
 * -xF				error
 * -xhelp=<f>			error
 * -xildoff			ignore
 * -xildon			ignore
 * -xinline			ignore
 * -xlibmieee			error
 * -xlibmil			error
 * -xlic_lib=sunperf		error
 * -xmaxopt=[...]		error
 * -xO<n>			-O<n>
 * -xP				error
 * -xprofile=<p>		error
 * -xregs=<r>			table
 * -xs				error
 * -xsb				error
 * -xsbfast			error
 * -xsfpconst			error
 * -xspace			ignore (-not -Os)
 * -xstrconst			ignore
 * -xtemp=<dir>			error
 * -xtime			error
 * -xtransition			-Wtransition
 * -xunroll=n			error
 * -W0,-xdbggen=no%usedonly	-fno-eliminate-unused-debug-symbols
 *				-fno-eliminate-unused-debug-types
 * -Y<c>,<dir>			error
 * -YA,<dir>			error
 * -YI,<dir>			-nostdinc -I<dir>
 * -YP,<dir>			error
 * -YS,<dir>			error
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#define	CW_F_CXX	0x01
#define	CW_F_SHADOW	0x02
#define	CW_F_EXEC	0x04
#define	CW_F_ECHO	0x08
#define	CW_F_XLATE	0x10
#define	CW_F_PROG	0x20

typedef enum cw_op {
	CW_O_NONE = 0,
	CW_O_PREPROCESS,
	CW_O_COMPILE,
	CW_O_LINK
} cw_op_t;

struct aelist {
	struct ae {
		struct ae *ae_next;
		char *ae_arg;
	} *ael_head, *ael_tail;
	int ael_argc;
};

typedef enum {
	GNU,
	SUN,
	SMATCH
} compiler_style_t;

typedef struct {
	char *c_name;
	char *c_path;
	compiler_style_t c_style;
} cw_compiler_t;

typedef struct cw_ictx {
	struct cw_ictx	*i_next;
	cw_compiler_t	*i_compiler;
	char		*i_linker;
	struct aelist	*i_ae;
	uint32_t	i_flags;
	int		i_oldargc;
	char		**i_oldargv;
	pid_t		i_pid;
	char		*i_tmpdir;
	char		*i_stderr;
} cw_ictx_t;

/*
 * Status values to indicate which Studio compiler and associated
 * flags are being used.
 */
#define	M32		0x01	/* -m32 - only on Studio 12 */
#define	M64		0x02	/* -m64 - only on Studio 12 */
#define	SS11		0x100	/* Studio 11 */
#define	SS12		0x200	/* Studio 12 */

#define	TRANS_ENTRY	5

static void
nomem(void)
{
	errx(1, "out of memory");
}

static void
newae(struct aelist *ael, const char *arg)
{
	struct ae *ae;

	if ((ae = calloc(1, sizeof (*ae))) == NULL)
		nomem();
	ae->ae_arg = strdup(arg);
	if (ael->ael_tail == NULL)
		ael->ael_head = ae;
	else
		ael->ael_tail->ae_next = ae;
	ael->ael_tail = ae;
	ael->ael_argc++;
}

static cw_ictx_t *
newictx(void)
{
	cw_ictx_t *ctx = calloc(1, sizeof (cw_ictx_t));
	if (ctx)
		if ((ctx->i_ae = calloc(1, sizeof (struct aelist))) == NULL) {
			free(ctx);
			return (NULL);
		}

	return (ctx);
}

static void
error(const char *arg)
{
	errx(2, "error: mapping failed at or near arg '%s'", arg);
}

static void
usage(void)
{
	extern char *__progname;
	(void) fprintf(stderr,
	    "usage: %s [-C] [--versions] --primary <compiler> "
	    "[--shadow <compiler>]... -- cflags...\n",
	    __progname);
	(void) fprintf(stderr, "compilers take the form: name,path,style\n"
	    " - name: a unique name usable in flag specifiers\n"
	    " - path: path to the compiler binary\n"
	    " - style: the style of flags expected: either sun or gnu\n");
	exit(2);
}

/*
 * The compiler wants the output file to end in appropriate extension.  If
 * we're generating a name from whole cloth (path == NULL), we assume that
 * extension to be .o, otherwise we match the extension of the caller.
 */
static char *
discard_file_name(cw_ictx_t *ctx, const char *path)
{
	char *ret, *ext;
	char tmpl[] = "cwXXXXXX";

	if (path == NULL) {
		ext = ".o";
	} else {
		ext = strrchr(path, '.');
	}

	/*
	 * We need absolute control over where the temporary file goes, since
	 * we rely on it for cleanup so tempnam(3C) and tmpnam(3C) are
	 * inappropriate (they use TMPDIR, preferentially).
	 *
	 * mkstemp(3C) doesn't actually help us, since the temporary file
	 * isn't used by us, only its name.
	 */
	if (mktemp(tmpl) == NULL)
		nomem();

	(void) asprintf(&ret, "%s/%s%s", ctx->i_tmpdir, tmpl,
	    (ext != NULL) ? ext : "");

	if (ret == NULL)
		nomem();

	return (ret);
}

static boolean_t
is_source_file(const char *path)
{
	char *ext = strrchr(path, '.');

	if ((ext == NULL) || (*(ext + 1) == '\0'))
		return (B_FALSE);

	ext += 1;

	if ((strcasecmp(ext, "c") == 0) ||
	    (strcmp(ext, "cc") == 0) ||
	    (strcmp(ext, "i") == 0) ||
	    (strcasecmp(ext, "s") == 0) ||
	    (strcmp(ext, "cpp") == 0)) {
		return (B_TRUE);
	}

	return (B_FALSE);
}

static void
do_gcc(cw_ictx_t *ctx)
{
	int c;
	int nolibc = 0;
	int in_output = 0, seen_o = 0, c_files = 0;
	cw_op_t op = CW_O_LINK;
	char *nameflag;

	if (ctx->i_flags & CW_F_PROG) {
		newae(ctx->i_ae, "--version");
		return;
	}

	if (asprintf(&nameflag, "-_%s=", ctx->i_compiler->c_name) == -1)
		nomem();

	/*
	 * Walk the argument list, translating as we go ..
	 */
	while (--ctx->i_oldargc > 0) {
		char *arg = *++ctx->i_oldargv;
		size_t arglen = strlen(arg);

		if (*arg == '-') {
			arglen--;
		} else {
			/*
			 * Discard inline files that gcc doesn't grok
			 */
			if (!in_output && arglen > 3 &&
			    strcmp(arg + arglen - 3, ".il") == 0)
				continue;

			if (!in_output && is_source_file(arg))
				c_files++;

			/*
			 * Otherwise, filenames and partial arguments
			 * are passed through for gcc to chew on.  However,
			 * output is always discarded for the secondary
			 * compiler.
			 */
			if ((ctx->i_flags & CW_F_SHADOW) && in_output) {
				newae(ctx->i_ae, discard_file_name(ctx, arg));
			} else {
				newae(ctx->i_ae, arg);
			}
			in_output = 0;
			continue;
		}

		if (ctx->i_flags & CW_F_CXX) {
			if (strcmp(arg, "-nolib") == 0) {
				/* -nodefaultlibs is on by default */
				nolibc = 1;
				continue;
			}
		}

		switch ((c = arg[1])) {
		case '_':
			if ((strncmp(arg, nameflag, strlen(nameflag)) == 0) ||
			    (strncmp(arg, "-_gcc=", 6) == 0) ||
			    (strncmp(arg, "-_gnu=", 6) == 0)) {
				newae(ctx->i_ae, strchr(arg, '=') + 1);
			}
			break;
		case 'f':
			if ((strcmp(arg, "-fpic") == 0) ||
			    (strcmp(arg, "-fPIC") == 0)) {
				newae(ctx->i_ae, arg);
				break;
			}
			error(arg);
			break;
		case 'E':
			if (arglen == 1) {
				op = CW_O_PREPROCESS;
				nolibc = 1;
				newae(ctx->i_ae, arg);
				break;
			}
			error(arg);
			break;
		case 'c':
		case 'S':
			if (arglen == 1) {
				op = CW_O_COMPILE;
				nolibc = 1;
			}
			/* FALLTHROUGH */
		case 'C':
		case 'H':
		case 'p':
			if (arglen == 1) {
				newae(ctx->i_ae, arg);
				break;
			}
			error(arg);
			break;
		case 'A':
		case 'D':
		case 'g':
		case 'h':
		case 'I':
		case 'i':
		case 'L':
		case 'l':
		case 'R':
		case 'U':
		case 'u':
		case 'w':
			newae(ctx->i_ae, arg);
			break;
		case 's':
			if (strcmp(arg, "-shared") == 0) {
			    newae(ctx->i_ae, "-shared");
			    nolibc = 1;
			    break;
			}
			error(arg);
			break;
		case 'G':
			newae(ctx->i_ae, "-shared");
			nolibc = 1;
			break;
		case 'o':
			seen_o = 1;
			if (arglen == 1) {
				in_output = 1;
				newae(ctx->i_ae, arg);
			} else if (ctx->i_flags & CW_F_SHADOW) {
				newae(ctx->i_ae, "-o");
				newae(ctx->i_ae, discard_file_name(ctx, arg));
			} else {
				newae(ctx->i_ae, arg);
			}
			break;
		case 'm':
			if (strcmp(arg, "-m64") == 0) {
				newae(ctx->i_ae, "-m64");
				break;
			}
			if (strcmp(arg, "-m32") == 0) {
				newae(ctx->i_ae, "-m32");
				break;
			}
			error(arg);
			break;
		case 'O':
			if (arglen == 1) {
				newae(ctx->i_ae, "-O");
				break;
			}
			error(arg);
			break;
		case 'V':
			if (arglen == 1) {
				ctx->i_flags &= ~CW_F_ECHO;
				newae(ctx->i_ae, "--version");
				break;
			}
			error(arg);
			break;
		case 'W':
			if (strncmp(arg, "-Wa,", 4) == 0 ||
			    strncmp(arg, "-Wp,", 4) == 0 ||
			    strncmp(arg, "-Wl,", 4) == 0) {
				newae(ctx->i_ae, arg);
				break;
			}
			error(arg);
			break;
		case 'Y':
			if (arglen == 1) {
				if ((arg = *++ctx->i_oldargv) == NULL ||
				    *arg == '\0')
					error("-Y");
				ctx->i_oldargc--;
				arglen = strlen(arg + 1);
			} else {
				arg += 2;
			}
			/* Just ignore -YS,... for now */
			if (strncmp(arg, "S,", 2) == 0)
				break;
			if (strncmp(arg, "I,", 2) == 0) {
				char *s = strdup(arg);
				s[0] = '-';
				s[1] = 'I';
				newae(ctx->i_ae, "-nostdinc");
				newae(ctx->i_ae, s);
				free(s);
				break;
			}
			error(arg);
			break;
		default:
			error(arg);
			break;
		}
	}

	free(nameflag);

	/*
	 * When compiling multiple source files in a single invocation some
	 * compilers output objects into the current directory with
	 * predictable and conventional names.
	 *
	 * We prevent any attempt to compile multiple files at once so that
	 * any such objects created by a shadow can't escape into a later
	 * link-edit.
	 */
	if (c_files > 1 && op != CW_O_PREPROCESS) {
		errx(2, "multiple source files are "
		    "allowed only with -E or -P");
	}

	if (ctx->i_flags & CW_F_SHADOW) {
		if (op == CW_O_PREPROCESS)
			exit(0);
		else if (op == CW_O_LINK && c_files == 0)
			exit(0);
	}

	if (!nolibc)
		newae(ctx->i_ae, "-lc");
	if (!seen_o && (ctx->i_flags & CW_F_SHADOW)) {
		newae(ctx->i_ae, "-o");
		newae(ctx->i_ae, discard_file_name(ctx, NULL));
	}
}

static void
do_smatch(cw_ictx_t *ctx)
{
	if (ctx->i_flags & CW_F_PROG) {
		newae(ctx->i_ae, "--version");
		return;
	}

	/*
	 * Some sources shouldn't run smatch at all.
	 */
	for (int i = 0; i < ctx->i_oldargc; i++) {
		char *arg = ctx->i_oldargv[i];

		if (strcmp(arg, "-_smatch=off") == 0) {
			ctx->i_flags &= ~ (CW_F_EXEC | CW_F_ECHO);
			return;
		}
	}

	/*
	 * smatch can handle gcc's options.
	 */
	do_gcc(ctx);
}

static void
do_cc(cw_ictx_t *ctx)
{
	int in_output = 0, seen_o = 0, c_files = 0;
	cw_op_t op = CW_O_LINK;
	char *nameflag;

	if (ctx->i_flags & CW_F_PROG) {
		newae(ctx->i_ae, "-V");
		return;
	}

	if (asprintf(&nameflag, "-_%s=", ctx->i_compiler->c_name) == -1)
		nomem();

	while (--ctx->i_oldargc > 0) {
		char *arg = *++ctx->i_oldargv;

		if (strncmp(arg, "-_CC=", 5) == 0) {
			newae(ctx->i_ae, strchr(arg, '=') + 1);
			continue;
		}

		if (*arg != '-') {
			if (!in_output && is_source_file(arg))
				c_files++;

			if (in_output == 0 || !(ctx->i_flags & CW_F_SHADOW)) {
				newae(ctx->i_ae, arg);
			} else {
				in_output = 0;
				newae(ctx->i_ae, discard_file_name(ctx, arg));
			}
			continue;
		}
		switch (*(arg + 1)) {
		case '_':
			if ((strncmp(arg, nameflag, strlen(nameflag)) == 0) ||
			    (strncmp(arg, "-_cc=", 5) == 0) ||
			    (strncmp(arg, "-_sun=", 6) == 0)) {
				newae(ctx->i_ae, strchr(arg, '=') + 1);
			}
			break;

		case 'V':
			ctx->i_flags &= ~CW_F_ECHO;
			newae(ctx->i_ae, arg);
			break;
		case 'o':
			seen_o = 1;
			if (strlen(arg) == 2) {
				in_output = 1;
				newae(ctx->i_ae, arg);
			} else if (ctx->i_flags & CW_F_SHADOW) {
				newae(ctx->i_ae, "-o");
				newae(ctx->i_ae, discard_file_name(ctx, arg));
			} else {
				newae(ctx->i_ae, arg);
			}
			break;
		case 'c':
		case 'S':
			if (strlen(arg) == 2)
				op = CW_O_COMPILE;
			newae(ctx->i_ae, arg);
			break;
		case 'E':
		case 'P':
			if (strlen(arg) == 2)
				op = CW_O_PREPROCESS;
		/*FALLTHROUGH*/
		default:
			newae(ctx->i_ae, arg);
		}
	}

	free(nameflag);

	/* See the comment on this same code in do_gcc() */
	if (c_files > 1 && op != CW_O_PREPROCESS) {
		errx(2, "multiple source files are "
		    "allowed only with -E or -P");
	}

	if (ctx->i_flags & CW_F_SHADOW) {
		if (op == CW_O_PREPROCESS)
			exit(0);
		else if (op == CW_O_LINK && c_files == 0)
			exit(0);
	}

	if (!seen_o && (ctx->i_flags & CW_F_SHADOW)) {
		newae(ctx->i_ae, "-o");
		newae(ctx->i_ae, discard_file_name(ctx, NULL));
	}
}

static void
prepctx(cw_ictx_t *ctx)
{
	newae(ctx->i_ae, ctx->i_compiler->c_path);

	if (ctx->i_flags & CW_F_PROG) {
		(void) printf("%s: %s\n", (ctx->i_flags & CW_F_SHADOW) ?
		    "shadow" : "primary", ctx->i_compiler->c_path);
		(void) fflush(stdout);
	}

	/*
	 * If LD_ALTEXEC is already set, the expectation would be that that
	 * link-editor is run, as such we need to leave it the environment
	 * alone and let that happen.
	 */
	if ((ctx->i_linker != NULL) && (getenv("LD_ALTEXEC") == NULL))
		setenv("LD_ALTEXEC", ctx->i_linker, 1);

	if (!(ctx->i_flags & CW_F_XLATE))
		return;

	switch (ctx->i_compiler->c_style) {
	case SUN:
		do_cc(ctx);
		break;
	case GNU:
		do_gcc(ctx);
		break;
	case SMATCH:
		do_smatch(ctx);
		break;
	}
}

static int
invoke(cw_ictx_t *ctx)
{
	char **newargv;
	int ac;
	struct ae *a;

	if ((newargv = calloc(sizeof (*newargv), ctx->i_ae->ael_argc + 1)) ==
	    NULL)
		nomem();

	if (ctx->i_flags & CW_F_ECHO)
		(void) fprintf(stderr, "+ ");

	for (ac = 0, a = ctx->i_ae->ael_head; a; a = a->ae_next, ac++) {
		newargv[ac] = a->ae_arg;
		if (ctx->i_flags & CW_F_ECHO)
			(void) fprintf(stderr, "%s ", a->ae_arg);
		if (a == ctx->i_ae->ael_tail)
			break;
	}

	if (ctx->i_flags & CW_F_ECHO) {
		(void) fprintf(stderr, "\n");
		(void) fflush(stderr);
	}

	if (!(ctx->i_flags & CW_F_EXEC))
		return (0);

	/*
	 * We must fix up the environment here so that the dependency files are
	 * not trampled by the shadow compiler. Also take care of GCC
	 * environment variables that will throw off gcc. This assumes a primary
	 * gcc.
	 */
	if ((ctx->i_flags & CW_F_SHADOW) &&
	    (unsetenv("SUNPRO_DEPENDENCIES") != 0 ||
	    unsetenv("DEPENDENCIES_OUTPUT") != 0 ||
	    unsetenv("GCC_ROOT") != 0)) {
		(void) fprintf(stderr, "error: environment setup failed: %s\n",
		    strerror(errno));
		return (-1);
	}

	(void) execv(newargv[0], newargv);
	warn("couldn't run %s", newargv[0]);

	return (-1);
}

static int
reap(cw_ictx_t *ctx)
{
	int status, ret = 0;
	char buf[1024];
	struct stat s;

	/*
	 * Only wait for one specific child.
	 */
	if (ctx->i_pid <= 0)
		return (-1);

	do {
		if (waitpid(ctx->i_pid, &status, 0) < 0) {
			warn("cannot reap child");
			return (-1);
		}
		if (status != 0) {
			if (WIFSIGNALED(status)) {
				ret = -WTERMSIG(status);
				break;
			} else if (WIFEXITED(status)) {
				ret = WEXITSTATUS(status);
				break;
			}
		}
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));

	if (stat(ctx->i_stderr, &s) < 0) {
		warn("stat failed on child cleanup");
		return (-1);
	}
	if (s.st_size != 0) {
		FILE *f;

		if ((f = fopen(ctx->i_stderr, "r")) != NULL) {
			while (fgets(buf, sizeof (buf), f))
				(void) fprintf(stderr, "%s", buf);
			(void) fflush(stderr);
			(void) fclose(f);
		}
	}
	(void) unlink(ctx->i_stderr);
	free(ctx->i_stderr);

	/*
	 * cc returns an error code when given -V; we want that to succeed.
	 */
	if (ctx->i_flags & CW_F_PROG)
		return (0);

	return (ret);
}

static int
exec_ctx(cw_ictx_t *ctx, int block)
{
	if ((ctx->i_stderr = tempnam(ctx->i_tmpdir, "cw")) == NULL) {
		nomem();
		return (-1);
	}

	if ((ctx->i_pid = fork()) == 0) {
		int fd;

		(void) fclose(stderr);
		if ((fd = open(ctx->i_stderr, O_WRONLY | O_CREAT | O_EXCL,
		    0666)) < 0) {
			err(1, "open failed for standard error");
		}
		if (dup2(fd, 2) < 0) {
			err(1, "dup2 failed for standard error");
		}
		if (fd != 2)
			(void) close(fd);
		if (freopen("/dev/fd/2", "w", stderr) == NULL) {
			err(1, "freopen failed for /dev/fd/2");
		}

		prepctx(ctx);
		exit(invoke(ctx));
	}

	if (ctx->i_pid < 0) {
		err(1, "fork failed");
	}

	if (block)
		return (reap(ctx));

	return (0);
}

static void
parse_compiler(const char *spec, cw_compiler_t *compiler)
{
	char *tspec, *token;

	if ((tspec = strdup(spec)) == NULL)
		nomem();

	if ((token = strsep(&tspec, ",")) == NULL)
		errx(1, "Compiler is missing a name: %s", spec);
	compiler->c_name = token;

	if ((token = strsep(&tspec, ",")) == NULL)
		errx(1, "Compiler is missing a path: %s", spec);
	compiler->c_path = token;

	if ((token = strsep(&tspec, ",")) == NULL)
		errx(1, "Compiler is missing a style: %s", spec);

	if ((strcasecmp(token, "gnu") == 0) ||
	    (strcasecmp(token, "gcc") == 0)) {
		compiler->c_style = GNU;
	} else if ((strcasecmp(token, "sun") == 0) ||
	    (strcasecmp(token, "cc") == 0)) {
		compiler->c_style = SUN;
	} else if ((strcasecmp(token, "smatch") == 0)) {
		compiler->c_style = SMATCH;
	} else {
		errx(1, "unknown compiler style: %s", token);
	}

	if (tspec != NULL)
		errx(1, "Excess tokens in compiler: %s", spec);
}

static void
cleanup(cw_ictx_t *ctx)
{
	DIR *dirp;
	struct dirent *dp;
	char buf[MAXPATHLEN];

	if ((dirp = opendir(ctx->i_tmpdir)) == NULL) {
		if (errno != ENOENT) {
			err(1, "couldn't open temp directory: %s",
			    ctx->i_tmpdir);
		} else {
			return;
		}
	}

	errno = 0;
	while ((dp = readdir(dirp)) != NULL) {
		(void) snprintf(buf, MAXPATHLEN, "%s/%s", ctx->i_tmpdir,
		    dp->d_name);

		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0) {
			continue;
		}

		if (unlink(buf) == -1)
			err(1, "failed to unlink temp file: %s", dp->d_name);
		errno = 0;
	}

	if (errno != 0) {
		err(1, "failed to read temporary directory: %s",
		    ctx->i_tmpdir);
	}

	(void) closedir(dirp);
	if (rmdir(ctx->i_tmpdir) != 0) {
		err(1, "failed to unlink temporary directory: %s",
		    ctx->i_tmpdir);
	}
}

int
main(int argc, char **argv)
{
	int ch;
	cw_compiler_t primary = { NULL, NULL, 0 };
	cw_compiler_t shadows[10];
	int nshadows = 0;
	int ret = 0;
	boolean_t do_serial = B_FALSE;
	boolean_t do_exec = B_FALSE;
	boolean_t vflg = B_FALSE;
	boolean_t Cflg = B_FALSE;
	boolean_t cflg = B_FALSE;
	boolean_t nflg = B_FALSE;
	char *tmpdir;

	cw_ictx_t *main_ctx;

	static struct option longopts[] = {
		{ "compiler", no_argument, NULL, 'c' },
		{ "linker", required_argument, NULL, 'l' },
		{ "noecho", no_argument, NULL, 'n' },
		{ "primary", required_argument, NULL, 'p' },
		{ "shadow", required_argument, NULL, 's' },
		{ "versions", no_argument, NULL, 'v' },
		{ NULL, 0, NULL, 0 },
	};


	if ((main_ctx = newictx()) == NULL)
		nomem();

	while ((ch = getopt_long(argc, argv, "C", longopts, NULL)) != -1) {
		switch (ch) {
		case 'c':
			cflg = B_TRUE;
			break;
		case 'C':
			Cflg = B_TRUE;
			break;
		case 'l':
			if ((main_ctx->i_linker = strdup(optarg)) == NULL)
				nomem();
			break;
		case 'n':
			nflg = B_TRUE;
			break;
		case 'p':
			if (primary.c_path != NULL) {
				warnx("Only one primary compiler may "
				    "be specified");
				usage();
			}

			parse_compiler(optarg, &primary);
			break;
		case 's':
			if (nshadows >= 10)
				errx(1, "May only use 10 shadows at "
				    "the moment");
			parse_compiler(optarg, &shadows[nshadows]);
			nshadows++;
			break;
		case 'v':
			vflg = B_TRUE;
			break;
		default:
			(void) fprintf(stderr, "Did you forget '--'?\n");
			usage();
		}
	}

	if (primary.c_path == NULL) {
		warnx("A primary compiler must be specified");
		usage();
	}

	do_serial = (getenv("CW_SHADOW_SERIAL") == NULL) ? B_FALSE : B_TRUE;
	do_exec = (getenv("CW_NO_EXEC") == NULL) ? B_TRUE : B_FALSE;

	/* Leave room for argv[0] */
	argc -= (optind - 1);
	argv += (optind - 1);

	main_ctx->i_oldargc = argc;
	main_ctx->i_oldargv = argv;
	main_ctx->i_flags = CW_F_XLATE;
	if (nflg == 0)
		main_ctx->i_flags |= CW_F_ECHO;
	if (do_exec)
		main_ctx->i_flags |= CW_F_EXEC;
	if (Cflg)
		main_ctx->i_flags |= CW_F_CXX;
	main_ctx->i_compiler = &primary;

	if (cflg) {
		(void) fputs(primary.c_path, stdout);
	}

	if (vflg) {
		(void) printf("cw version %s\n", CW_VERSION);
		(void) fflush(stdout);
		main_ctx->i_flags &= ~CW_F_ECHO;
		main_ctx->i_flags |= CW_F_PROG | CW_F_EXEC;
		do_serial = 1;
	}

	tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL)
		tmpdir = "/tmp";

	if (asprintf(&main_ctx->i_tmpdir, "%s/cw.XXXXXX", tmpdir) == -1)
		nomem();

	if ((main_ctx->i_tmpdir = mkdtemp(main_ctx->i_tmpdir)) == NULL)
		errx(1, "failed to create temporary directory");

	ret |= exec_ctx(main_ctx, do_serial);

	for (int i = 0; i < nshadows; i++) {
		int r;
		cw_ictx_t *shadow_ctx;

		if ((shadow_ctx = newictx()) == NULL)
			nomem();

		(void) memcpy(shadow_ctx, main_ctx, sizeof (cw_ictx_t));

		shadow_ctx->i_flags |= CW_F_SHADOW;
		shadow_ctx->i_compiler = &shadows[i];

		r = exec_ctx(shadow_ctx, do_serial);
		if (r == 0) {
			shadow_ctx->i_next = main_ctx->i_next;
			main_ctx->i_next = shadow_ctx;
		}
		ret |= r;
	}

	if (!do_serial) {
		cw_ictx_t *next = main_ctx;
		while (next != NULL) {
			cw_ictx_t *toreap = next;
			next = next->i_next;
			ret |= reap(toreap);
		}
	}

	cleanup(main_ctx);
	return (ret);
}
