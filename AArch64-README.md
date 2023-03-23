# illumos for AArch64

This is the port of illumos to aarch64, mostly by:

- Hayashi Naoyuki <alpha@culzean.org>
- Michael van der Westhuizen <r1mikey@gmail.com>
- Jonathan Perkin <jonathan@perkin.org.uk>
- Andy Fiddaman <illumos@fiddaman.net>
- Dominik Hassler <hadfl@omnios.org>
- Marco van Wieringen <marco.van.wieringen@planets.elm.net>
- Richard Lowe <richlowe@richlowe.net>

## Building

A separate source tree that builds our dependencies and can be used to build
disk images is available from https://github.com/richlowe/arm64-gate

The primary difference when building this workspace like any other illumos
workspace is that `nightly` and `bldenv` now take a `-T <target>` option which
specifies the target ISA for which you're building (in this case, aarch64).

## A Note Regarding ABI

During the early illumos life of this port, none of the illumos ABI rules
apply.

*THIS INCLUDES LIBRARY VERSIONING*.

There are symbols missing from established library versions in this port,
particularly within `libm(3LIB)`, which is a separate port of the FreeBSD math
library with the SONAME `libm.so.0` at the present time.

Other ABI details may change prior to final integration into the main
illumos branch, without limit.

## A Note Regarding ksh(1)

The version of `ksh93(1)` in this port is incorrectly cross-compiled for the
sake of bootstrapping.

## Many Things Are Missing

Lot's of things you're used to are missing, improperly implemented, or both.
A great thing to help with is fixing this.

## Do Not Upstream Anything

Please, do not under any circumstances upstream things from this tree without
talking to anyone, a lot of code here is expedient rather than good.

## Find Things To Help With

Search for `XXXARM`, `NOT_AARCH64`, or fix whatever is broken for you!
Note that `not_aarch64` in packaging currently conveys multiple things, not
all of which represent bugs.

## See Also

* [IPD 24 Support for 64-bit ARM (AArch64)](https://github.com/illumos/ipd/blob/master/ipd/0024/README.md)
