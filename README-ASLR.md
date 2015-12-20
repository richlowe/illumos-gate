# Address Space Layout Randomisation

Briefly, see: `psecflags(1)`, `security-flags(5)`

## Administration

ASLR is implemented via process security-flags (which we introduce), there are
four sets of flags per-process: The effective, inheritable, upper, and lower
sets (see `security-flags(5)`).  The effective set is immutable, it can only
change when the process calls `exec(2)`, at which point the effective set is
replaced by the inheritable set (with one exception).  Security flags are
inherited upon `fork(2)` (but the inheritable set is not promoted until
`exec(2)`, as mentioned).  The upper and lower sets bound the permitted values
of the inheritable set.

This is such that a given execution of an executable has a constant set of
security-flags, which simplifies things for everyone.

This unfortunately means that to enable ASLR fully system-wide, requires a
reboot or at least restart of a majority of services.

The system-wide ASLR flag is an SMF property on the new service
`svc:/system/process-security`, which contains `default`, `upper`, and `lower`
property groups, with one boolean property per implemented flag (see, again,
`security-flags(5)`).  These will only affect services (and their children)
started via SMF after the values have been changed.

Per-process setting (and inspecting) of security-flags is done via
`psecflags(1)`.

Per-service setting of security-flags is achievable by the `security_flags`
property on the service `method_context`.  A `default` pseudo-flag specifies
the flags from `svc:/system/process-security`, and flags can be
added/subtracted from there much the same as with `privileges(5)`.

## Privilege

A process may change the security-flags of any process to which it could send
a signal with `kill(2)`, as long as the process also has the
`PRIV_PROC_SECFLAGS` privilege.  This privilege is granted by default, but is
not a `basic` privilege.  If you have configured custom privileges for certain
users or services, they will not automatically gain the new
`PRIV_PROC_SECFLAGS` (unfortunately).

## Executable tagging

There is a somewhat compatible property of dynamic executables, `DT_SUNW_ASLR`
which controls the ASLR behaviour of a given executable.  If this dynamic tag
has a value of 1, ASLR is always enabled for an execution of this process.  If
the tag has a value of 0, ASLR is never enabled for an execution of this
process.  The default is to inherit the ASLR flag as normal.

This allows a process for which ASLR is known to be problematic to explicitly
forbid it, and for processes of special sensitivity to mandate it.

This is controlled via the `-z aslr` flag to `ld(1)`.

 
## Per-zone configuration

The default security flags for a zone, and the upper and lower limits, may be
specified with the security-flags resource in zonecfg(1M).  These are applied
to every process in NGZs (unlike the GZ, where there are a small number of
processes we must miss)


## Missing bits/Problems/Worries

### The stack skewing may skew too much of the stack

At present, we skew the absolute base of the stack, which means the gap
between a user stack frame and the process environment is constant.  I have
this vague memory that that's sub-par, and that we want to skew the stack
_after_ the environment, etc.  I could be entirely wrong about that though.

### We skew each mapping separately

Some systems calculate a random skew for the various parts of a process at
execution time, and apply that same skew to each mapping.  That obviously
minimises the performance impact of this significantly for processes that
perform many mappings.

Due to some unfortunate aspects of how we manage the user address space and
mappings, we don't do that right now.  We calculate a separate random skew for
each mapping we attempt.

### The way we skew mappings is problematic

The user address space is currently managed somewhat unfortunately (from our
point of view, at least).  When attempting a mapping we first determine the
highest gap into which the requested mapping can fit, and then we place the
mapping at the highest address in that gap.  This is how we manage user
fragmentation.

The current ASLR implementation works in basically the same way.  We still
find the highest gap into which the given mapping may fit, and choose the
highest address at which it may fit.  The only difference is that we then slew
it backward by a random (but co-aligned), amount.

This is obviously not as random as it could be, but is the easiest way I've
found in the current code base for introducing uncertainty while also
preserving any attempt at preventing unbounded user fragmentation.  

It is not impossible to imagine a long-lived process that makes many mappings
being in a position where mappings later in its life are 100% predictable
given this implementation, however.  In fact, I think it is fairly likely.

I think the most at risk would be a process which makes many mappings, and
uses `dlopen(3c)` at unpredictable times (rather than, as is most common, during
setup).  Dynamic objects tend to have strict (and high) alignment requirements
which in such a process are likely to only be fulfillable at a single location
in the address space gap we choose, and thus be entirely predictable.

### Randomisation of executable base addresses requires PIE

To randomise the executable base address, we need position independent
executables, which appears like it would turn into a whole separate project
(though perhaps not too large a project).  That code isn't here, so the
executable base is fixed.

This means that rather than return-to-libc one could return-to-executable
trivially and successfully, I think.  (That would include using the
executable's PLT to vector yourself to libc.  Should the executable have a PLT
entry for something useful to you). 
