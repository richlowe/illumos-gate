<!--
This file and its contents are supplied under the terms of the
Common Development and Distribution License ("CDDL"), version 1.0.
You may only use this file in accordance with the terms of version
1.0 of the CDDL.

A full copy of the text of the CDDL should have accompanied this
source.  A copy of the CDDL is also available via the Internet at
http://www.illumos.org/license/CDDL.

Copyright 2024 Richard Lowe
-->

# The Kernel Source Tree and Build Environment

## Layout

The kernel source tree, for historical reasons, is effectively two trees
layered one on top of the other.  The source tree and the build tree.

### The source tree

Sources are arranged per-*architecture* ('intel'), *machine* ('i86pc'),
potentially by *implementation* (individual boards), and by (loosely speaking)
function.  `usr/src/uts/common/io/foo/` is likely to be where the architecture
independent `foo` driver sources, and their common Makefile live.

This structure is arbitrarily deep, depending on the source concerned.  For
example: `intel/io/scsi/adapters/arcmsr/` is the intel-specific arcmsr(4D)
driver, which is arranged to keep SCSI drivers, and more specifically SCSI HBA
drivers, logically grouped.

It has historically been a problem for new developers to discover where kernel
sources may be, there is unfortunately no answer better than `git ls-files |
grep ...`.  A directory containing a module's `Makefile.com` will also contain
its sources.

### The build tree

The build tree is distinct from, but overlayed onto, the source tree.  It is
only two levels deep, the most-general of architecture/machine/implementation
a module can serve, and then the name of that module.  To carry on our
previous examples `usr/src/uts/intel/foo` would be where the common `foo`
driver would be built for all platforms using the intel architecture
`usr/src/uts/intel/arcmsr` where the arcmsr(4D) would be built.

## Structure

Each module has a build directory containing the Makefile that drives the
build for a given platform.

In the ideal case, `Makefile` is very small, containing only those
pieces not provided by the driver's `Makefile.com`, and some metadata that
cannot be derived.

- `UTSBASE`
  The relative path to `usr/src/uts`, used to keep source paths relative and
  generic, rather than workspace specific
- `UTSMACH`
  The architecture/machine/implementation for which this is being built
  (often the basename of this Makefile's parent directory)
- Include the platform-independent `Makefile.com`
- Any platform specific additions and rules for extra objects etc.

As an example, here is the `uts/intel/bge/Makefile`, The Makefile for
bge(4D) on Intel:

```
UTSBASE = ../..
UTSMACH = intel
include $(SRC)/uts/common/io/bge/Makefile.com
```

The common Makefile (in this case `uts/common/io/bge/Makefile.com`) makes
use of `Makefile.kmod` in the common case a module is being built, and is
described below.

To build larger subsets of the kernel you may:
- `make` in `<platform>/<module>` (that is, `intel/bge` say) will build just
  that module.
- `make` in `<platform>/` (that is, `intel` or `i86pc`) will build all modules
  for that platform
- `make` in `usr/src/uts` will build the whole kernel.

## Makefile.kmod

`Makefile.kmod` (and `Makefile.kmod.targ`) are "library" Makefiles, which
provide defaults and facilities to make writing a modules common Makefile
easier and less error-prone.

The basic form of such a `Makefile.com` is to define the module-specific pieces,
include `Makefile.kmod`, modify any settings necessary for your module, and
then to include `Makefile.kmod.targ`

The simplest possible example would be something like amdnbtemp(4D):

```
MODULE		= amdnbtemp
MOD_SRCDIR	= $(UTSBASE)/intel/io/amdnbtemp

include $(UTSBASE)/Makefile.kmod
include $(UTSBASE)/Makefile.kmod.targ
```

Which specifies the name of the module, and the directory where the module's
sources live (the same directory as `Makefile.com`).  Makefile.kmod derives
the rest -- in this simple case -- from its defaults.

A more complete example is lofs(4D):

```
MODULE		= lofs
MOD_SRCDIR	= $(UTSBASE)/common/fs/lofs
OBJS		= lofs_subr.o	lofs_vfsops.o	lofs_vnops.o
ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

include $(UTSBASE)/Makefile.kmod

CERRWARN	+= -_gcc=-Wno-parentheses

include $(UTSBASE)/Makefile.kmod.targ
```

Which specifies the objects by name, and the directory in which the module
should be installed, and also amends `CERRWARN` which controls which compiler
warnings are disabled.

A full description of variables, their defaults, and their use, follows.

### Input variables

These are variables that should be configured _before_ including
`Makefile.kmod`.

#### `MODULE`

The name of the module, without any prefix. Often but not always the basename
of the current directory, it has no default and must always be set.

#### `MOD_SRCDIR`

The directory in which the module sources (and this Makefile.com) live, used
as the default for other things, most notably the implicit rule to build
sources from the current directory for this driver.  This cannot be
automatically determined, and must be always be specified.

#### `OBJS`

A list of objects that make up this driver.  Unless there are technical
reasons not to, these should be sorted and, if there are more than around 3,
phrased one per-line so that source code management as well as developers,
have the best possible chance to easily merge changes.

This defaults to `$(MODULE).o`.

#### `ROOTMODULE`

The installation target for this module, usually one of the `ROOT_..._DIR`
variables (defined in `Makefile.uts`, or `<platform>/Makefile.<platform>`)
followed by `$(MODULE)`.  Defaults to `/kernel/drv/...`

#### `CONF_SRCDIR`

The directory in which any module configuration file source can be found, defaults to
`$(MOD_SRCDIR)`.  If you have no configuration file (and most drivers do not
need one), don't worry about this.

### Configuration variables

These are variables that must be configured _after_ the include of
`Makefile.kmod` to further customize the defaults, and should almost always be
appended to not assigned to.

#### `DEPENDS_ON`

A list of modules on which this module depends, of the form `misc/foo` (that
is, fully specified, excluding platforms etc.).  These should be alphabetic
and, if more than around 3, one per-line in the same manner as `$(OBJS)` and
for the same reasons.

The core operating system kernel, made up of "unix", "genunix", etc, is
implicit and need not be specified.

#### `CERRWARN`

Controls the warnings emitted by the compiler.  Should be appended to each
time it is modified to gag a single specific kind of warning.

```
CERRWARN += -_gcc=-Wno-foo
CERRWARN += -_gcc=-Wno-bar
```

This can be specified with object-specific assignments, so that only a single
source file is excluded.

```
$(OBJS_DIR)/foo.o := CERRWARN += -_gcc=-Wno-thing
```

#### `SMOFF`

Controls warnings emitted by smatch(1), a comma separated list of warnings
which should be ignored.

As with `CERRWARN` this can be specified with object-specific assignments, so that only a single
source file is excluded.

```
$(OBJS_DIR)/foo.o := SMOFF += all_func_returns
```

#### `SMATCH`

A big hammer for smatch, if set to `off` smatch does not check this module at
all.

#### `INC_PATH`

Passed to the compiler and assembler to specify the include search path,
append to it, including the `-I`

`INC_PATH += -I$(UTSBASE)/foo`

#### `PRE_INC_PATH`

Like `INC_PATH`, but always first in the list before any defaults, should
defaults need to be overridden.

#### `ALL_DEFS`

Definitions passed to the compilers, append to it, include the `-D` or `-U`

`ALL_DEFS += -DFOO`

#### `AS_DEFS`

Like `ALL_DEFS` but only passed to the assembler.

#### `ALL_TARGET`

The make targets run by `make all`, defaults to building this module.  If you
deliver a configuration file it should often be amended to depend on the
source of that file.

`ALL_TARGET += $(SRC_CONFFILE)`

#### `INSTALL_TARGET`

The make targets run by `make install`, defaults to installing this module in
the directory specified by `$(ROOTMODULE)`.  If you deliver a configuration
file this should be amended to include installing that.

`INSTALL_TARGET += $(ROOT_CONFFILE)`

## Writing a new Makefile (and common mistakes)

Hopefully, this document and the numerous examples in the source tree give you
a great start on writing what you need for your own work, but there are
pitfalls.

The greatest pitfall is simply this: Do not copy and paste the Makefile from
another module without reading this document and understanding the Makefile
you are copying.  You should not blindly copy things like `CERRWARN` and
`SMOFF`, to exclude warnings not relevant to your module.

Some modules for historical reasons modify things like `CFLAGS` and `CPPFLAGS`
that are rarely, if ever, meant to be modified directly in the kernel build
(compare to `CERRWARN`, `INC_PATH`, etc.)  This is different from builds in
userland and can trap the unwary.

## Special sources of confusion

There are pieces of the kernel which are, for various reasons, not built
exactly in this fashion.

- Components which are not "kernel modules".  This includes some firmware files
  loaded by drivers.  These are inappropriate for Makefile.kmod and must be
  spelled out.

- `unix` is a special case of not being a kernel module, in that it is the
  initial kernel executable and is built differently (and in very platform
  specific ways).  Makefiles named `Makefile.unix.com` are provided at
  different levels of the tree to allow things like object lists to be shared.

- `genunix` due to the architecture of the system ('the kernel' is the union
  of `unix`, `genunix`, and `platmod`) this module varies somewhat
  considerably depending on target platform, depending on which pieces have to
  be in which binary.  `Makefile.genunix.com` holds the common pieces, but
  does not make use of Makefile.kmod

## I maintain extra drivers in a downstream fork, what's the absolute least I can do?

The bare minimum is to move your `FOO_OBJS` definition from `Makefile.files`
into your client driver Makefile before it defines `OBJECTS`, and insert your
build rules from `Makefile.rules` at the end.
