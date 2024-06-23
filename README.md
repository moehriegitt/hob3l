# Hob3l
100x Faster Slicing of SCAD Files for 3D Printing

## What is This?

Hob3l is a command line tool for reading SCAD files and writing STL
files for 3D printing.  The focus is on speed and robustness.

OpenSCAD can convert SCAD to STL, too, but it is very slow, because it
first produces a 3D object.  And the CGAL library used by OpenSCAD is
not very robust: I often get spurious error messages due to unstable
3D arithmetics: 'object may not be a valid 2-manifold'.

Instead, Hob3l uses stable arithmetics to produce an STL file suitable
for 3D printing.  It first pre-slices the basic 3D objects from the
SCAD file into layers and then uses 2D operations on each layer.  The
2D operations are much faster than 3D operations, and arithmetically
much simpler, and thus easier to get stable.

Hob3l is very robust -- the 2D base library was fuzzed to get rid of
numeric instability problems.  Hob3l uses integer arithmetics and a
snap rounding algorithm to stay within the input coordinate precision.
It reads and writes normal floating point numbers, and the float<->int
conversions are exact within `float` precision (the native STL binary
number format).  If necessary, the precision can be scaled (by
default, the unit is 1/8192mm).

## How Is It Fast?

To be faster than OpenSCAD, Hob3l replaces 3D operations by faster 2D
operations.  For this, Hob3l first cuts slices and then applies the
boolean operations.

Instead, [OpenSCAD](http://www.openscad.org/) applies 3D operations to
compute a single 3D object.  This is very expensive math, and often
arithmetically unstable.  But for 3D printing, that single 3D object
is not needed: it is cut into slices anyway.

So hob3l reverses the internal workflow.  Instead of 'compute in 3D, then slice':

![3D CSG](img/csg1-old.png)

it does 'slice, then compute in 2D':

![2D CSG](img/csg1-new.png)

The idea is explained in more detail
[in my blog](http://www.theiling.de/cnc/date/2018-09-23.html).

Hob3l's main output formats are

  * STL for printing
  * JavaScript/WebGL for viewing and prototyping

Hob3l reads OpenSCAD's native SCAD format, it can import STL files
(for 3D imports), and SVG files (for 2D imports).

## SCAD Input Format

[My SCAD format documentation](doc/scadformat.md) defines what exactly
is supported by Hob3l, and what is different from OpenSCAD.

For the full SCAD format syntax, OpenSCAD can be used as a
preprocessor: OpenSCAD can read the full SCAD format and write a
simplified version, with only structurs that Hob3l supports.  This is
a fast preprocessing step, and still avoids OpenSCAD's expensive 3D
operations:

```
    openscad thing.scad -o thing.csg
    hob3l thing.csg -o thing.stl
```

See [Using This Tool](#using-this-tool-command-line-options).

## Valid 2-Manifolds

Hob3l produces only valid 2-manifolds.

Well, if the input polyhedra are really bad, like missing faces, then
Hob3l may fail to produce a valid output.  But you need blatantly
broken input polyhedra for this.  This cannot happen unless you use
`polyhedron()` manually in SCAD.  Hob3l is not supposed to fail just
because you subtract an object from another object and the two share a
part of a face (when you get flickering in
[OpenSCAD](http://www.openscad.org/)): Hob3l either subtracts
everything properly, or it leaves a small (valid) polyhedron -- but it
does not become unstable and fail on you.

If Hob3l shows unstable behaviour, then that is a bug.  Getting it
stable took the majority of the development time, because I found this
the most annoying problem with OpenSCAD (or the underlying CGAL
library).

## Status, Stability, Limitations, Future Work, TODO

This tool has been tested very thoroughly for stability and arithmetic
robustness, in order to get rid of any floating point instabilities,
using a fuzzer and many millions of tests.

The tool can read the [specified subset of SCAD](doc/scadformat.md)
(possibly from a preprocessing step by OpenSCAD's to resolve
unsupported syntax).  Hob3l then slices the input objects, applies the
2D boolean operations (AKA polygon clipping), and then triangulates
the resulting polgygons.  Then it writes STL format or WebGL/JS.

After that, Slic3r (and probably PrusaSlicer and Cura) can read the
STL files Hob3l produces.  This step is still necessary, although
Hob3l slices the input file, too, because the slicer also does the
path planning and G-code generation, which Hob3l does not do.

The input polyhedra must be 2-manifold.  However, Hob3l accepts quite
a few non-2-manifold input polyhedra.  Polyhedra with holes (i.e.,
missing faces), however, will not work.  OpenSCAD (or the CGAL
library) probably now has more constraints on well-formedness than
Hob3l.  E.g., Hob3l's algorithms are robust against wrong handedness
of faces.

Hob3l can import STL files (both text and binary formats) for 3D
objects and SVG files for 2D objects.  Because SVG is a very complex
format, only a useful subset is supported, e.g., no CSS styling is
implemented.  E.g, SVG files written by Inkscape can be read by Hob3l.

The output STL contains separate layers instead of a single solid.  In
the future, Hob3l may generate one contiguous object. It would be more
processing and is not strictly necessary.  But if you hit `split` in
Slic3r on the current output of Hob3l, you'll get many separate layer
objects -- which is not useful.

Memory management has leaks.  I admit I don't care enough, because
Hob3l basically starts, allocates, exits, i.e., it does not run for
long, so the memory leaks do not build up.

There are never enough tests.  However, Hob3l's core algorithms have
survived many millions of fuzzing tests with
[afl](https://lcamtuf.coredump.cx/afl/).

## Supported Output Formats

`STL`: This is the main output format of Hob3l for which it was first
developed.  The input SCAD files can be converted to STL and then used
as input to a slicer for 3D printing.  Both ASCII STL (more precise)
and binary STL (smaller) are supported.

`PS`: For debugging and documentation, including algorithm
visualisation, Hob3l can output in PostScript.  This is how the
overview images on this page where generated: by using single-page PS
output, converted to `PNG` using `GraphicsMagick`.  For debugging,
mainly multi-page debug PS output was used, which allows easy browsing
(I used `gv` for its speed and other nice features). Also, this allows
to compare different runs and do a step-by-step analysis of what is
going on during the algorithm runs.  The PS modules has a large number
of command line options to customise the output.

`WEBGL/JS`: For prototyping SCAD files, a web browser can be used as a
3D model viewer by using the WebGL/JavaScript output format.  The SCAD
file can be edited in your favourite editor, then for visualisation,
Hob3l can generate WebGL data (possibly with an intermediate step to
let OpenSCAD simplify the input file using its `.csg` output), and a
reload in the web browser will show the new model.  This package
contains auxiliary files to make it immediately usable, e.g. the
surrounding .html file with the WebGL viewer that loads the generated
data.  See the `hob3l-js-copy-aux` script.

`SCAD`: For debugging intermediate steps in the parser and converter,
Hob3l can write SCAD format of several of its processing stages.  In
intermediate stages, however, Hob3l's polyhedra may not be strictly
correct when printed in SCAD debug output (they may use wrong
handedness of polyhedra faces) and then loaded into OpenSCAD for
inspection.  (But STL and WebGL/JS output do produce correctly oriented
faces.)

## JavaScript/WebGL Output

Here's a screenshot of my browser with a part of the
[Prusa i3 MK3](https://github.com/prusa3d/Original-Prusa-i3)
3D printer rendered by Hob3l:

![Mk3 Part](img/curryjswebgl.png)

There is an [online version available
here](http://www.theiling.de/cnc/gl-curryhob3l/curry.html) to play
with.

The conversion from `.scad` to `.js` takes about 0.7s on my machine,
so this is very well suited for prototyping: write the `.scad` in a
text editor, run 'make', reload in browser.  To run this conversion
yourself, after building, run:

```
    make clean-test
    time make test-out/curry.js
```

This should print something like:

```
./hob3l.exe scad-test/curry.scad -o test-out/curry.js.new.js
Info: Z: min=0.1, step=0.2, layer_cnt=75, max=14.9
mv test-out/curry.js.new.js test-out/curry.js

real  0m0.650s
user  0m0.592s
sys   0m0.044s
```

## Building

Building relies on GNU make and gcc, and uses no automake or other
meta-make layer.  Both Linux native and the MinGW Windows cross
compiler have been tested.

Make variables can be used to switch how the stuff is compiled.  Some
GCC extensions are used, but I tried not to overdo it (`({...})` and
`__typeof__` are used frequently, though), it should be compilable
without too much effort.

Compilation is straight-forward:

```
    make clean
    make
    make test
```

Parallel building is supported using the `-j` option to make.

Some Perl scripts are used to generate C code during compilation.

The resulting executable is called 'hob3l.x'.  It is renamed during
installation (`hob3l` on Linux, `hob3l.exe` on Windows).

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
    make TARGET=gcc64
```

To compile with gcc for i686 (e.g., 32 bit x86 Linux):

```
    make TARGET=gcc32
```

To compile with Clang:

```
    make TARGET=clang
```

To cross compile for Windows 64 using MinGW:

```
    make TARGET=win64
```

To cross compile for Windows 32 using MinGW:

```
    make TARGET=win32
```

You can set the exact compiler name by overriding `CC`:

```
    make TARGET=win32 CC=my-funny-mingw-gcc
```

### Tweaking Compiler Settings

The Makefile has more settings that can be used to switch to other compilers
like clang, or to other target architectures.  This is not properly documented
yet, so reading the Makefile may be necessary here.

The most likely ones you may want to change are the following (listed
with their default setting):

```
CFLAGS_ARCH  := -march=native
```

## Running Tests

After building, tests can be run, provided that the 'hob3l.x'
executable can actually be executed (hopefully).  On systems where it
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

Each time `make check` is invoked, it will first remove the old test
output files to make sure that the check is actually run.  `make
check` also honours the `DESTDIR` variable to construct the path to the
installed executable in the same way as `make install`.

## Installation

The usual installation ceremony is implemented, according to the GNU
Coding Standard.  I.e., you have `make install` with `prefix`, and all
`*dir` options and also `DESTDIR` support as well as
`$(NORMAL_INSTALL)` markers, and also `make uninstall`.

```
    make DESTDIR=./install-root prefix=/usr install
```

For better package separation, the `install` target is split into
`install-bin`, `install-data`, `install-lib`, `install-include`
(e.g. to compile a separate `-dev` package as in Debian
distributions).

Unfortunately, there is no `install-doc` yet.  FIXME.

## Using This Tool, Command Line Options

When in doubt, use `hob3l --help`.

To convert a complex SCAD file into the subset that Hob3l can read,
start by using OpenSCAD to convert to a flat 3D CSG structure with all
the syntactic sugar removed.  This conversion is fast.

```
    openscad thing.scad -o thing.csg
```

You can now use Hob3l to process it:

```
    hob3l thing.csg -o thing.stl
```

The result can then be used in your favorite tool for computing print
paths.

```
    slic3r thing.stl
```

## Speed comparison

Depending on the complexity of the model, Hob3l may be much faster
than using OpenSCAD with CGAL rendering.

Some examples:

The x-carriage.scad part of my
[Prusa i3 MK3](https://github.com/prusa3d/Original-Prusa-i3)
printer from the Prusa github repository: let's first convert it
to `.csg`.  This conversion is quickly done with OpenSCAD, and the
resulting flat SCAD format is what Hob3l can read:

```
    time openscad x-carriage.scad -o x-carriage.csg
    0m0.034s
```

To convert to STL using openscad 3D CSG takes a while:

```
    time openscad x-carriage.csg -o x-carriage.stl
    0m45.208s
```

Doing the same with Hob3l in 0.2mm layers is about 50 times faster:

```
    time hob3l x-carriage.csg -o x-carriage.stl
    0m0.824s
```

The most complex part of the i3 MK3 printer, the `extruder-body.scad`,
before it was reimplemented as `step` file, takes 2m42s in openscad to
convert to STL, while Hob3l takes 1.24s, again with 0.2mm layers.
That is 130 times faster.

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

You can push the difference in speed by making the model more complex,
particularly when using high detail levels.  E.g., the test31b.scad
example uses `$fn=99` for a few ellipsoids, causing openscad to slow
down:

```
time openscad scad-test/test31b.scad -o test31b.stl
4m30.198s
```

In contrast, the different algorithms used by Hob3l do not slow down
much:

```
time ./hob3l.exe scad-test/test31b.scad -o test31b.stl
0m0.748s
```

This is 350 times faster.  The difference is of course that with
Hob3l, the result is sliced into layers, as the following image
demonstrates.  The top is the OpenSCAD F6 view, the bottom is Hob3l's
WebGL output in my web browser.

![OpenSCAD output](img/test31b-openscad.jpg)

![Hob3l output](img/test31b-hob3l.jpg)

## Rendering Differences

The difference of the conversion technique is visible in the model
view of the STL, where the 2D CSG slicing technique clearly shows the
layers, e.g. for a real-life example sliced a 0.2mm with Hob3l.  The
top is OpenSCAD's output in Slic3r, the bottom is Hob3l's output in
Slic3r:

![OpenSCAD model](img/useless-model-openscad.jpg)
![Hob3l model](img/useless-model-hob3l.jpg)

The final result of the slicer, however, is indistinguishable (I was
unable to replicate the exact same view, so the Moir&eacute; patterns
are different -- but the result is really the same), again OpenSCAD
output top, Hob3l bottom:

![OpenSCAD preview](img/useless-preview-openscad.jpg)
![Hob3l preview](img/useless-preview-hob3l.jpg)

## Algorithms

The polyhedra (from SCAD input files) are processed using IEEE double
precision floating point coordinates.  The 2D algorithms, however, now
use 32-bit integer coordinates for exact math (and can handle 31-bit
signed values without overflow).  Therefore, the coordinates in a
polygon slice from a polyhedron are converted from `double` to `int`
by multiplying by a power of two -- this way, the upper bits of the
floating point mantissa (53 bits for `double`) can be used directly as
ints with minimum rounding error.  When converting back from `int` to
`double`, the integer coordinates are divided by the same power of
two, meaning that no rounding error occurs: the integer is used
directly as the upper mantissa bits for the floating point number (the
lower bits are 0).  A round trip from int to double to int is then
loss-less.  As binary STL uses `float` coordinates (with a 24 bit
mantissa, smaller than 32-bit integers), care was taken to scale in
such a way that a wide range of float coordinates also convert to STL
with no rounding error.  And the ASCII STL is printed with many
significant digits to ensure that the information gets into the slicer
without any loss of precision.  All integer operations check for
overflow so that the scale value can be adjusted if necessary for
weird input files.

The slice algorithm to cut a polygon slice from a polyhedron is a
simple ad-hoc algorithm that works by iterating each face, making a
cut at a given z height, sorting the cut points, and interpreting them
as line segments.  The subsequent algorithms need no particular order
of edges, so a very simple algorithm is enough here.

The polygon clipping algorithm is a Bentley-Ottmann 1979 (Algorithms
for reporting and counting geometric intersections) plane sweep
algorithm using exact fractional math for the intersections.  Ideas
from Mart&iacute;nez, Rueda, Feito 2009 (A new algorithm for computing
Boolean operations on polygons) were used to extend Bentley-Ottmann to
corner cases like overlapping edges.  Also, the inside/outside
information is tracked in a way similar to that paper, extended by
ideas from Sean Conelly's polybooljs project.  The input/output
information was then extended to handle more than two polygons at
once, by using a boolean function represented by a bit array.  This
speeds up the 2D processing.

The ideas from Boissonnat and Preparata 2000 (Robust Plane Sweep for
Intersecting Segments) helped examine the complexity of the numeric
problems and to construct a data type for storing intersection points
exactly: with a 160 bit fractional (32 bit integer + 64 bit
numerator + 64 bit denominator).  This avoids overheads from generic
exact math libraries and it is quite fast.

After the intersection algorithm, the snap rounding algorithm by de
Berg 2007 (An Intersection-Sensitive Algorithm for Snap Rounding) is
run to fit the intersection coordinates back into the input bit width
(32-bit integer coordinates).

To get a triangulation (fro the output polyhedron in STL format), the
triangulation algorithm of Hertel & Mehlhorn 1983 (Fast Triangulation
of the Plane with Respect to Simple Polygons) was used and extended to
support coincident vertices, because these cannot be avoided.  Also,
sequences of collinear edges are resolved.

The same algorithm was adapted also for constructing a polygon outline
from the set of edges produced by the preceding algorithm, if no
triangulation is needed.  This is used in the SCAD language
processing, e.g., with operations like `extrude` or `project`, where
the result of the 2D boolean algorithm is fed back into the CSG tree.

## Development

This is a project for me to relax and have fun, to be a distraction
and to be different from my day job (which also involves programming).
The project follows some policies to avoid stress. for me to continue
to have fun.

### XNIH: Exclude What's Not Implemented Here

No external libraries or tools are used for this project, except a C
compiler, Perl, and libc/POSIX.  All functionality is either
implemented here, or not at all.

This policy helps me focus on programming, instead of battling APIs.
Every API incompatibility will be my own fault.  There will be no
stress when upgrading to a newer version of a library, because there
are none.

### C and Perl

This project is implemented in C with gcc extensions, for a reasonably
modern C standard.  Perl is used for scripts.  GnuMake is used for
building.

### Linux

My development platform is Linux.  Other platforms are not excluded,
and the Makefile has direct support for Clang and for MinGW
compilation.  But I cannot debug problems specific to platforms I do
not use.

## Name

The name Hob3l derives from the German word 'Hobel', meaning 'plane'
(as in 'wood plane').  The 'e' was turned to `3` in recognition of the
`slic3r' program.
