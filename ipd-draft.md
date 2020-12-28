---
author: Richard Lowe
state: draft
---

# IPD XXX User-focused (source) user-level debugging for illumos

## Introduction

Currently the debugging facitilies of the illumos system are poor in the area
of traditional debugging, we provide the `mdb(1)` symbolic debugger but the
vast majority of users are unfamiliar both with the tool and its facilities,
and with such low-level debugging in general.

illumos would benefit from providing a functional source-level debugging
environment allowing end-user visibility into at least the system libraries.
illumos developers would also benefit from such facilities when debugging our
own code, but to a real extent they are a secondary audience for this work.

## Proposal

Modern debuggers (`gdb(1)`, `lldb(1)`) support references to debug data in
alternate objects referenced either by a special `.gnu_debuglink` `PROGBITS`
section, or GNU `NT_BUILDID` note.

We propose to deliver these debug objects for all user-focused shared objects
delivered by the system, and other objects as user or developer demand
suggests.

User-focused shared objects are taken to be those shared libraries used by
end-user code commonly debugged using the binary debuggers, as opposed to
plugins to system applications, or native modules for dynamic languages.
Though this view is necessarily vague.

### Delivery and Packaging

We deliver debug objects to `/usr/lib/debug` which needs to be configured when
`gdb(1)` is built (`./configure --with-separate-debug-dir`, the default is
undesirable in at least some versions).  This mirrors the structure of the
system, such that the debug object for `/lib/libc.so.1` is
`/usr/lib/debug/lib/libc.so.1`.  Both 32bit and 64bit debug objects are
delivered into this same hierarchy, such that the amd64 libc debug object is
`/usr/lib/debug/lib/amd64/libc.so.1`.  There is no `/usr/lib/debug/amd64`.



_XXX: Should there be an extra token after `debug` so there's an option beyond
"per-package" and "all debug facilities" exists, if use of `debug` expands?
`debug.elf.` perhaps?_

It is assumed that distributions will deliver these objects in a fashion to
make them optional.  On systems using pkg(5) and the default metadata, they
are delivered behind a per-package facet `debug.<package name>` (i.e.,
`debug.system/library`) that can be toggled to install the debug objects for
only that package, or toggle `debug.*` to toggle all debugging support.  On
other systems it is presumed that delivery will be done similar to on Linux
distributions, where packages with a common suffix such as `-dbg` are used.


### Creation of debug objects

_XXX: All this is subject to change, or rather, I haven't yet found an option
I'm particularly happy with_

We create debug objects at link-edit time using a new argument to the
link-editor `ld(1)` `-zdebuglink=`.  This specifies the path to which the
debug object will be saved during the build (e.g., `debugobj/libfoo.so.1`),
the file component of this path is used as the link name in the
`.gnu_debuglink` section (eg, `libfoo.so.1`).  This means that during the
build we create `debugobj/libfoo.so.1` but the debugger will look for
`/usr/lib/debug/.../libfoo.so.1`.

`-zdebuglink` operates by creating a new object based on the main output
object, but with non-relevant sections transformed to `SHT_NOBITS` so that
they consume no space.  The input objects and output object are presumed to
contain any desired debugging sections, and the main output object is not
stripped by default.  It is assumed that `strip -x` will be used as
appropriate on that object.

### Inclusion in the illumos build

`usr/src/Makefile.lib` gains a variable `DEBUGLIBS` which controls the build
of debug libraries.  By default it is set to `DEBUGLIB`, which has the same
value of `DYNLIB`.  This means most libraries build a debug object by default.  To
disable this, set `DEBUGLIBS` to an empty value.  C++ libraries will need to
set `DEBUGLIBS` to `DEBUGLIBCCC`, symmetrically with `DYNLIBCCC`.

Debug objects are placed in `debugobj/` subdirectory by default, such that
they can have the same name as their main library object.

Most libraries should not need to do anything and the above should work, if
you have a complicated build you may need to arrange passing `-zdebuglink`
yourself, and to install the resulting objects yourself.  Ideally, you would
be using the library Makefiles conventionally however.

### Inclusion in other builds

One of the reasons behind implementing this in the link-editor is the goal to
be easy to pass through foreign build systems.  Doing this in `ld(1)` we can
pass it in via either `LDFLAGS` or `LD_OPTIONS` to create debug objects for
3rd party software, as opposed to having to make alterations to software or
take special steps during the build.

The `-zdebuglink` option supports expansion of a variable token, `$OUTPUT`,
such that 3rd party builds producing multiple libraries can still use this
approach to create debug objects, via `-zdebuglink=debug/$OUTPUT`.
