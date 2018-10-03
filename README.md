# eins
Fast Slicing of SCAD Files for 3D Printing

## Replace 3D CSG by Fast 2D Polygon Clipping

Preparing a 3D model in CSG format (e.g., when using OpenSCAD) for
printing may take a long time and is often computationally instable.

So this tool wants to replace a workflow of '3D CSG--Slice--Print':
![3D CSG](img/csg1-old.png)

by a workflow of 'Slice--2D CSG--Print':

![2D CSG](img/csg1-new.png)

In the hope that the latter is faster.  First experiments indeed
indicate a huge speed-up for a non-trivial example, and much better
computational stability.

## SCAD Input Format

This tool reads a subset of the SCAD format used by OpenSCAD -- I did
not want to invent another format.  This section describes which
subset is supported.

The general idea is that all basic polyhedra 3D objects are supported,
all basic transformations, and all boolean operations.  No operations
are supported that invoke the CGAL rendering, e.g. complex ones like
`minkovsky`.  I haven't used them anyway because they are so slow.

The following SCAD abstract syntax tree (AST) structures are supported:

```
    // line comments
    /* block comments */
    564           // integers
    56.3          // reals
    true undef x  // identifiers
    [A:B:C]       // ranges
    [a, b, c]     // arrays
    foo()         // functors
    foo(x, y)     // functors with arguments
    foo(x, x=z)   // functors with named arguments (also mixed)
    foo();        // empty functor invocations
    foo() bar()   // single element functor invocations
    foo() { }     // block functor invocations
    * ! # %       // modifier characters
```

The biggest parts that are missing are constants/variables, functions,
and modules.

The following SCAD operators and identifiers are supported.  In each
functor's parentheses, the supported arguments are listed.  $fa and
$fs are ignored, but accepted in the input file for compatibility with
existing files.  Both named and positional arguments are supported
just like in OpenSCAD.

Usually the default for a missing argument is '1', but this tool may
be more restrictive than OpenSCAD: if the OpenSCAD documentation lists
an argument as mandatory, it will be rejected if missing, while
OpenSCAD may still accept it and assume '1'.

```
    true
    false
    PI

    group() { ... }
    union() { ... }          // interpreted the same as 'group'
    intersection() { ... }
    difference() { ... }

    sphere(r, d, $fa, $fs, $fn)
    cube(size, center)
    cylinder(h, r, r1, r2, d, d1, d2, center, $fa, $fs, $fn)
    polyhedron(points, faces, triangles)

    multmatrix(m)
    translate(v)
    mirror(v)
    scale(v)
    rotate(a,v)

    circle(r, d, $fa, $fs, $fn)
    square(size, center)
    polygon(points, paths)
```

### Broken SCAD Syntax

The most irritating detail about SCAD syntax that I found was the
interpretation of children elements of `difference()`: the first
non-empty child is interpreted as positive, all others are negative.

However, the syntax does not make it immediately clear which thing is
the 'first non-empty' child, because even an empty `group(){}` is
skipped, and even recursively so.  Essentially, you need semantics to
identify the children correctly, which is an ugly mix of meta levels.

So the parser of this tool spends quite some effort on determining
which one is the first non-empty child of `difference`.  Whether it
does that in the same way as OpenSCAD, I can only hope for.  I think
this part of the SCAD syntax is broken.

Whenever this tool prints SCAD format, it will state its intended
meaning by using comments `// add` and `// sub` to mark which parts
are positive and which ones are negative.

## Algorithmic Improvements

The polygon clipping algorithm of Mart&iacute;nez, Rueda, Feito (2009)
was improved to have less computational instability be deriving
crossing points always from the original edge (although currently,
each 2D operation restarts this -- this could be improved).  Also,
computational stability was improved by rigurous use of epsilon-aware
arithmetics.  Further, a better polygon reassembling algorithm with
O(n log n) runtime was implemented -- the original paper and reference
implementation do not focus on this part.

The triangulation algorithm of Hertel & Mehlhorn (1983) was
extended to support coincident vertices, because this is what the
polygon clipping outputs.  Also, sequences of collinear edges are
fully supported, because, again, this may happen.

## Status, Stability, Limitations, Future Work, TODO

The basic workflow is implemented and tested, i.e., the tool can read
the specified subset of SCAD, it can slice the input object, it can
apply the 2D boolean operations (AKA polygon clipping), and it can
triangulate the resulting polgygons as a preparation for writing STL.
Corner cases in the algorithms have been dealt with (except for
unknown bugs).  Because of the 'stability' design goal that extends
from computational real number stability to corner cases, this was in
focus from the start.

There is no actual STL output yet.

The input polyhedra must consist of only convex faces.  This will be
fixed in the future.

The input polyhedra must be 2-manifold.  This is because the slicing
algorithm is edge driven and uses a notion of 'left and right face' at
an edge, so an edge must have a unique face on each of its sides.
This restriction will probably not be fixed soon.

Spheres are not properly implemented yet.  I want to do them nicely
and delay their rendering until they are circles/ellipses in the 2D
world, so there is no polyhedron approximation of spheres implemented
yet.  (My 3D constructions rarely ever contain spheres -- I seem to
build everything from cubes and cylinders, and occasionally from
manually constructed polyhedra.)

Cylinders fail to work if `$fn` is too large.  This has the same
reason as for circles: my intension is to make them nice and
completely round, but this part is not implemented yet.  The threshold
setting for 'large $fn' should be a command line options, but is
currently missing.

Memory management has leaks.  I admit I don't care enough, because
this tool basically starts, allocates, exits, i.e., it does not run
for long, so the memory leaks do not build up.  The goal is to have a
proper, fast pool based allocation.  This is prepared, but incomplete.

There are not enough tests.

The tests that exist often only test a lack of a crash or assertion
failure, but whether the algorithm works correctly then needs to be
inspected by a human.

## Building

Building relies on GNU make and gcc, and uses no automake or other
meta-make layer.  Some Perl scripts are used to generate C code, but
all generated C code is also checked in, so the scripts are only
invoked when changes are made.

Make variables can be used to switch how the stuff is compiled.  Since
I tried not to overdo with gcc extensions (`({...})` and `__typeof__`
are used frequently, though), it should be compilable without too much
effort.

E.g.:

```
    make clean
    make
    make test
```

The resulting executable is called 'csg2plane.x'.

Parallel building should be fully supported using the `-j` option to
make.

### Different Build Variants

The makefile supports 'normal', 'release', and 'devel' build variants,
which can be switched using the `MODE=normal` (default),
`MODE=release`, or `MODE=devel` command line variables for make.  The
selection is stored in a file `.mode.d`, so next time you invoke
'make' without a `MODE` parameter, the previous build variant will be
chosen.

### Different Compiler Targets

To compile with the standard 'gcc', whatever that is, for x86:

```
    make
```

To compile with gcc for x86_64 (e.g., 64 bit x86 Linux):

```
    make TARGET=nix64
```

To compile with gcc for i686 (e.g., 32 bit x86 Linux):

```
    make TARGET=nix32
```

To cross compile for Windows 64 using mingw32:

```
    make TARGET=win64
```

To cross compile for Windows 32 using mingw32:

```
    make TARGET=win32
```

### Tweaking Compiler Settings

The Makefile has more settings that can be used to switch to other compilers
like clang, or to other target architectures.  This is not properly documented
yet, so reading the Makefile may be necessary here.

The most likely ones you may want to change are the following (listed
with their default setting):

```
CFLAGS_ARCH  := -march=core2 -mfpmath=sse
```

## Running Tests

After building, tests can be run, provided that the 'csg2plane.x'
executable can actually be executed.  It may be (e.g. under Windows)
that the local executable does not have the necessary file extension
(e.g. `.exe`), so `make test` will not work.  On systems where it
works, use

```
    make test
```

for that.  This runs both the unit tests as well as basic SCAD
conversion tests.  For full set of checks (asserts) during testing,
the 'devel' build variant should be used in addition to the actual
build variant.

After installation, the SCAD conversion tests can be run with the
installed binary by using

```
    make check
```

On some systems, this will work better than `make test`, because the
installed executable will have the correct file extension
(e.g. `.exe`).

Each time `make check` is invoked, it will first remove the old test
output files to make sure that the check is actually run.  `make
check` also honours the `DESTDIR` variable to construct the path to the
installed executable in the same way as `make install`.

## Installation

The usual installation ceremony is implemented, hopefully according to
the GNU Coding Standard.  I.e., you have `make install` with `prefix`,
and all `*dir` options and also `DESTDIR` support as well as
`$(NORMAL_INSTALL)` markers, and also `make uninstall`.

```
    make DESTDIR=./install-root prefix=/usr install
```

For better package separation, the `install` target is split into
`install-bin`, `install-lib`, `install-include` (e.g. to have a
separate `-dev` package as in Debian distributions).

Unfortunately, there is no `install-doc` yet.  FIXME.

The package and the binary are currently called `csg2plane` -- the
name is work in progress.  The github repository is called `eins` and,
thus, not really more imaginative.  The package name can be changed
during installation using the `package_name` variable, but this only
changes the executable name and the library name, but not the include
subdirectory, because this would not work as the name is explicitly
used in the header files.

## Command Line Parameters

Use `csg2plane --help`.
