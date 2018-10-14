# hob3l
Fast Slicing of SCAD Files for 3D Printing

## Replace 3D CSG by Fast 2D Polygon Clipping

Preparing a 3D model in CSG format (e.g., when using
[OpenSCAD](http://www.openscad.org/)) for
printing may take a long time and is often computationally instable.

So `hob3l` wants to replace a workflow 'apply 3D CSG, then slice, then print':
![3D CSG](img/csg1-old.png)

by a workflow 'slice, then apply 2D CSG, then print':

![2D CSG](img/csg1-new.png)

In the hope that the latter is faster.  First experiments indeed
indicate a huge speed-up for a non-trivial example, and much better
computational stability.

The idea is explained in more detail [in my
blog](http://www.theiling.de/cnc/date/2018-09-23.html).

And I definitely do not want to rant about OpenSCAD.  It is a great
tool that I am also using.  This is about a different technique for
rendering CSG into STL that is especially suited for 3D printing,
where individual flat slices from your model is all you need.  If you
really need a 3D solid from your CSG, then do use OpenSCAD's CGAL
based rendering.

## SCAD Input Format

This tool reads a subset of the SCAD format used by OpenSCAD -- I did
not want to invent another format.  This section describes which
subset is supported.

The general idea is that all basic polyhedra 3D objects are supported,
all basic transformations, and all boolean operations.  No operations
are supported that invoke the CGAL rendering, e.g. complex ones like
`minkowsky`.  I haven't used them anyway because they are so slow.

The following SCAD abstract syntax tree (AST) structures are supported:

```
    // line comments
    /* block comments */
    564           // integers
    56.3          // reals
    "hello\n"     // strings
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

Usually the default for a missing argument is '1', but `hob3l` may
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

Some additional items are parsed, but ignored: `linear_extrude`,
`text`.

### Broken SCAD Syntax

The most irritating detail about SCAD syntax that I found was the
interpretation of children elements of `difference()`: the first
non-empty child is interpreted as positive, all others are negative.

However, the syntax does not make it immediately clear which thing is
the 'first non-empty' child, because even an empty `group(){}` is
skipped, and even recursively so.  Essentially, you need semantics to
identify the children correctly, which is an ugly mix of meta levels.

The parser of `hob3l` spends quite some effort on determining which
one is the first non-empty child of `difference`.  Whether it does
that in the same way as OpenSCAD, I can only hope for.  I think this
part of the SCAD syntax is broken.

## Algorithmic Improvements

The triangulation algorithm of Hertel & Mehlhorn (1983) was
extended to support coincident vertices, because this is what the
polygon clipping outputs.  Also, sequences of collinear edges are
fully supported, because, again, this may happen.

The polygon clipping algorithm of Mart&iacute;nez, Rueda, Feito (2009)
was improved to have less computational instability by deriving
crossing points always from the original edge (although currently,
each 2D operation restarts this -- this could be improved).  Also,
computational stability was improved by rigorous use of epsilon-aware
arithmetics.  Further, a better polygon reassembling algorithm with
O(n log n) runtime was implemented -- the original paper and reference
implementation do not focus on this part.  Also, several additional
corner cases are handled.

For further speed-up, the polygon clipping algorithm was extended to
support processing more than two polygons at the same time, because
with a runtime of O(n log n), it benefits from larger n.  Currently,
it usually works with max. 10 polygons.  Even processing 3 polygons at
once speeds up some examples by a factor of 2 over processing 2
polygons at once.

## Status, Stability, Limitations, Future Work, TODO

Despite quite some testing and debugging, this will still assert-fail
occasionally or output rubbish.  The floating point algorithms are
somewhat brittle and hard to get right.

OTOH, the basic workflow is implemented and tested, i.e., the tool can
read the specified subset of SCAD (e.g. from OpenSCAD's CSG output),
it can slice the input object, it can apply the 2D boolean operations
(AKA polygon clipping), and it can triangulate the resulting
polgygons, and write STL.  Slic3r can read the STL files `hob3l`
produces.

Corner cases in the algorithms have been dealt with (except for
unknown bugs).  Because of the stability design goal that extends from
computational real number stability to corner cases, this was in focus
from the start.  Corner case handling took up most of the development
time and takes up a large portion of the code, because doing floating
point computations in a stable way is really tricky.

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

The output STL contains separate layers instead of a single solid.
This will be fixed in the future.  For now, if you hit `split` e.g. in
slic3r, you'll get hundreds of separate layer objects -- which is not
useful.

Memory management has leaks.  I admit I don't care enough, because
`hob3l` basically starts, allocates, exits, i.e., it does not run for
long, so the memory leaks do not build up.  The goal is to have a
proper, fast pool based allocation.  This is prepared, but incomplete.

There are not enough tests.

The tests that exist often only test for absence of a crash or assertion
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

The resulting executable is called 'hob3l.x'.

Parallel building should be fully supported using the `-j` option to
make.

### Different Build Variants

The makefile supports 'normal', 'release', and 'devel' build variants,
which can be switched using the `MODE=normal` (default),
`MODE=release`, or `MODE=devel` command line variables for make.  The
selection is stored in a file `.mode.d`, so next time you invoke
'make' without a `MODE` parameter, the previous build variant will be
chosen.

E.g.:

```
    make clean
    make MODE=release
    make test
```

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

After building, tests can be run, provided that the 'hob3l.x'
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

The package name `hob3l` can be changed during installation using the
`package_name` variable, but this only changes the executable name and
the library name, but not the include subdirectory, because this would
not work as the name is explicitly used in the header files.

## Using This Tool, Command Line Options

In general, use `hob3l --help`.

To convert a normal scad file into the subset this `hob3l` can read,
start by using OpenSCAD to convert to a flat 3D CSG structure with all
the syntactic sugar removed.  This conversion is fast.

```
    openscad thing.scad -o thing.csg
```

You can now use `hob3l` to slice this directly instead of applying
3D CSG:

```
    hob3l thing.csg -o thing.stl
```

This can then be used in your favorite tool for computing print paths.

```
    slic3r thing.stl
```

### Tweaking Command Line Settings

The underlying technique of `hob3l` is computationally difficult,
because it relies on floating point operations.  The goal was
stability, but it turned out to be really difficult to achieve, so
`hob3l` might still occasionally fail.  If this happens, the following
command line options change internal settings that might push the tool
back on track:

```
    --max-simultaneous=N    # decrease for better stability; min. is 2
```

The following options also have an influence, but neither large nor
small is really better -- changing them causes different perturbations
and thus different results, some of which might be more likely to
succeed.  A good heuristics is to keep `gran` larger than `eps` and
let `eps2` be about the square of `eps`.

```
    --gran=X
    --eps=X
    --eps2=X
```

## Speed comparison

Depending on the complexity of the model, `hob3l` may be much faster
than using OpenSCAD with CGAL rendering.

Some examples:

The x-carriage.scad part of my [Prusa](https://www.prusa3d.com/) i3
MK3 printer from the Prusa github repository: let's first convert it
to `.csg`.  This conversion is quickly done with OpenSCAD, and the
resulting flat SCAD format is what `hob3l` can read:

```
    time openscad x-carriage.scad -o x-carriage.csg
    0m0.034s
```

To convert to STL using openscad 3D CSG takes a while:

```
    time openscad x-carriage.csg -o x-carriage.stl
    0m45.208s
```

Doing the same with `hob3l` is about 50 times faster:

```
    time hob3l x-carriage.csg -o x-carriage.stl
    0m0.824s
```

The most complex part of the i3 MK3 printer, the `extruder-body.scad`,
before it was reimplemented as `step` file, takes 2m42s in openscad to
convert to STL, while `hob3l` takes 1.24s.  That is 130 times
faster.

For one of my own parts `useless-box+body`, which is less complex, but
does not care much about making rendering fast (I definitely set up
cylinders with too many polygon corners):

```
    time openscad uselessbox+body.scad -o uselessbox+body.stl
    0m53.433s

    time hob3l uselessbox+body.scad -o uselessbox+body.stl
    0m0.610s
```

This is 85 times faster.  Over half of the time is spent on writing
the STL file, which is 23MB -- STL is huge.  Loading and converting
only takes 0.23s.

## Rendering Differences

The difference of the conversion technique is visible in the model
view of the STL, where the 2D CSG slicing technique clearly shows the
layers:

![OpenSCAD model](img/useless-model-openscad.jpg)
![Hob3l model](img/useless-model-hob3l.jpg)

The final result of the slicer, however, is indistinguishable (I was
unable to replicate the exact same view, so the Moir&eacute; patterns are
different -- but the result is really the same):

![OpenSCAD preview](img/useless-preview-openscad.jpg)
![Hob3l preview](img/useless-preview-hob3l.jpg)

## Name

The name `hob3l` derives from the German word 'Hobel', which is a
'planer' (as in 'wood planer') in English.  The 'e' was turned to `3`
in recognition of the `slic3r' program.
