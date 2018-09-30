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

The general idea is that every basic polygon-type

## Algorithmic Improvements

The polygon clipping from Mart&iacute;nez, Rueda, Feito (2009) was
improved to have less computational instability be deriving crossing
points always from the original edge (although currently, each 2D
operation restarts this -- this could be improved).  Also,
computational stability was improved by rigurous use of epsilon-aware
arithmetics.  Further, a better polygon reassembling algorithm with
O(n log n) runtime was implemented -- the original paper and reference
implementation does not focus on this part.

The triangulation algorithms from Hertel & Mehlhorn (1983) was
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

There is no STL output yet.  This is because the polygon clipping
algorithm does not output the correct path direction for deciding
inside and outside.

The input polyhedra must consist of only convex faces.

The input polyhedra must be 2-manifold.  This is because the slicing
algorithm is edge-driven and uses a notion of 'left and right face' at
an edge, so an edge must have a unique face on both its sides.

Spheres are not properly implemented yet.  I want to do them nicely
and delay their rendering until they are circles in the 2D world, so
there is no polyhedron approximation of spheres implemented yet.  (My
3D constructions rarely ever contain spheres -- I seem to build
everything from cubes and cylinders, and occasionally manually
constructed polyhedra.)

Cylinders fail to work if $fn is too large.  This has the same reason
as for circles: my intension is to make them nice and completely
round, but this part is not implemented yet.  The threshold setting for
'large $fn' should be a command line options, but is currently missing.

Memory management has leaks.  I admit don't care enough, because this
tool basically starts, allocates, exists, i.e., it does not run for
long, so the memory leaks do not build up.  The goal is to have a
proper, fast pool based allocation.  This is prepared, but incomplete.

No 'install' target has been added to the makefile yet.

## Building

This relies on gnumake and gcc, and uses no automake or other
meta-make process.  Make variables can be used to switch how the stuff
is compiled.  Since this is pure standard C (albeit with gcc
extensions), it should be compilable without too much effort.

E.g.:

```
    make clean
    make MODE=release
    make test
```

The resulting executable is called 'csg2plane.x'.

### Different Build Variants

The makefile supports 'normal', 'release', and 'devel' build variants, which can
be switch using the 'MODE=normal' (default), 'MODE=release', or 'MODE=devel'
command line variables for make.  The selection is stored in the file .mode.d,
so next time you invoke 'make', it uses the same build variant.

### Different Compiler Targets

To compile with the standard 'gcc' whatever it is, for x86:

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
yet, so reading the Makefile is necessary here.

## Running Tests

After building, tests can be run, provided that the 'csg2plane.x'
executable can actually be executed.  Use 'make test' for that.  This
runs both the unit tests as well as basic SCAD conversion tests.  For
full set of checks (asserts) during testing, the 'devel' build variant
should be used in addition to the actual build variant.

## Command Line Parameters

Use 'csg2plane.pl --help'.
