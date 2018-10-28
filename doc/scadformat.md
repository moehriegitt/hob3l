# Hob3l's Dialect Of SCAD

## Table Of Contents

  * [Table Of Contents](#table-of-contents)
  * [Introduction](#introduction)
  * [Informal Overview](#informal-overview)
      * [Broken SCAD Syntax](#broken-scad-syntax)
  * [Syntax](#syntax)
      * [Notation](#notation)
      * [Morphology](#morphology)
      * [Syntax](#syntax)
  * [Special Value Identifiers](#special-value-identifiers)
  * [Dimensions](#dimensions)
  * [Coordinate Matrix](#coordinate-matrix)
  * [Functor Calls](#functor-calls)
  * [Functors](#functors)
      * [circle](#circle)
      * [color](#color)
      * [cube](#cube)
      * [cylinder](#cylinder)
      * [difference](#difference)
      * [group](#group)
      * [intersection](#intersection)
      * [linear_extrude](#linear-extrude)
      * [mirror](#mirror)
      * [multmatrix](#multmatrix)
      * [polygon](#polygon)
      * [polyhedron](#polyhedron)
      * [rotate](#rotate)
          * [If `v` is not specified](#if-v-is-not-specified)
          * [If `v` is specified](#if-v-is-specified)
      * [scale](#scale)
      * [sphere](#sphere)
      * [square](#square)
      * [text](#text)
      * [translate](#translate)
      * [union](#union)

## Introduction

Hob3l reads SCAD files as its native input format in order to avoid
defining yet another file format.  The SCAD format is originally
defined for the OpenSCAD tool, and Hob3l tries to read the files in a
compatible way.  Hob3l does not read the full SCAD format, but a
subset, which is described in this document.  Up to now, there are no
extensions introduced by Hob3l, so any file Hob3l reads should also be
a valid input for OpenSCAD.

This document tries to be more formal about SCAD syntax than the
OpenSCAD documentation and to handle all corner cases and answer all
questions about what syntax is accepted by Hob3l.  For a more
intuitive and visual introduction to the file format, with graphical
examples, the OpenSCAD documentation should be consulted.

To make Hob3l compatible, when writing Hob3l, it was necessary to run
OpenSCAD in experiments to find out how it exactly parses and
interprets the input file.  This document tries to avoid this kind of
underspecificity.

That said, this document is probably incomplete anyway, but that is a
bug.

In general and in the spirit of parsing a subset of SCAD, Hob3l is
stricter than OpenSCAD about mandatory parameters and parameter
values, because it was felt that error messages are better than
silently assuming a default, particularly for finding bugs.

## Informal Overview

The general idea is that all basic polyhedra 3D objects of SCAD are
supported, all basic transformations, and all boolean operations.  No
operations are supported that invoke the CGAL rendering, e.g. complex
ones like `minkowsky`.  I haven't used them anyway because they are so
slow.

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

    color(c, alpha)
```

The 2D objects are parsed, but poorly supported (since `linear_extrude` is not
supported yet, there is little point to have 2D objects):

```
    circle(r, d, $fa, $fs, $fn);
    square(size, center);
    polygon(points, paths);
```

Some additional items are parsed, but currently ignored:

```
    linear_extrude(...)
    text(...)
```

### Broken SCAD Syntax

One irritating detail about SCAD syntax that I found was the
interpretation of children elements of `difference()`: the first
non-empty child is interpreted as positive, all others are negative.

However, neither the OpenSCAD syntax nor the syntax description make
it immediately clear which thing is the 'first non-empty' child,
because even an empty `group(){}` is skipped, and even recursively so.
Essentially, you need semantics to identify the children correctly,
which I find is an ugly mix of meta levels.

The parser of `hob3l` spends quite some effort on determining which
one is the first non-empty child of `difference`.  Whether it does
that in the same way as OpenSCAD, I can only hope for.  I think this
part of the SCAD syntax is broken.

## Syntax

### Notation

This file uses a modified Wirth Syntax Notation (WSN) to define the
syntax.  The following differences exist with standard WSN:

  * Instead of double quotes, backticks are used so that the text
    looks better in .md format.

  * A negation X `!` Y is introduced to signify 'X but not Y'.

  * A range operator `...` for characters is introduced to abbreviate
    the syntax definitions.

  * A comment after `:` is introduced for restrictions in plain text.

  * C syntax string escaping is used for special characters.

The normal WSN notation includes the following constructions:

  * `(` ..X.. `)`: grouping, makes priority explicit
  * `[` ..X.. `]`: 0 or 1 times ...X...
  * `{` ..X.. `}`: 0, 1, or more times ...X...
  * A `|` B: either A or B

### Morphology

The input file is processed as a stream of bytes, generally
interpreted as US-ASCII character set, with exceptionally allowing
uninterpreted 8-bit characters when listed explicitly.  In particular,
input files with paths and strings encoded in UTF-8 are trivially
supported, but not rejected for invalid UTF-8 encoding, nor converted
to a different output character set, e.g. in error messages.

The sequence of bytes is separated into a sequence of TOKENs according
to the morphology description in this section.  Based on this token
disassembly, the syntax will be defined.

In this description, ANY is any US-ASCII printable character.  ANY8 is
any byte value, including 8-bit characters outside of US-ASCII.  No
character set is assumed for 8-bit characters, i.e., they are
processed as is, e.g., inside comments and strings.

  * File = {TOKEN} .
  * TOKEN = IDENT | INTEGER | FLOAT | STRING | LINECOM | MULTICOM | WHITE | SYMBOL .
  * ALPHA = `a` ... `z` | `A` ... `Z` .
  * DIGIT = `0` ... `9` .
  * ID_START = `$` | `_` | ALPHA .
  * ID_CONT = `_` | ALPHA | DIGIT .
  * IDENT = ID_START { ID_CONT } .
  * SIGN = `+` | `-` .
  * E = `e` | `E` .
  * NUMX = [SIGN] {DIGIT} [ `.` {DIGIT} ] [ E [SIGN] {DIGIT} ] .
  * NUM = NUMX : if NUMX starts with SIGN, `.` or DIGIT .
  * INTEGER = NUM : if NUM contains no E or `.` .
  * FLOAT = NUM : if NUM is not an INTEGER .
  * STRCHAR = ANY8 ! ( `"` | `\` ) .
  * STRSEQ = STRCHAR | `\` ANY .
  * STRING = `"` {STRSEQ} `"` .
  * LCCHAR = ANY8 ! `\n`
  * LINECOM = `//` {LCCHAR} .
  * MCCHAR = {ANY8} : if the sequence does not contain `*/` .
  * MULTICOM = `/*` {MCCHAR} `*/`
  * WHITE = ` ` | `\n` | `\r` | `\t`
  * SYMBOL = ANY : if no other token matches .

Example IDENT: `a`, `xyz`, `$fs`

Example NUM: `77`, `2.8`, `+5e-9`

Example STRING: `"abc"`, `"a\"b\\c"`

Example SYMBOL: `+`, `/`, `#`

### Syntax

The syntax is defined as follows, ignoring any WHITE, LINECOM and
MULTICOM tokens.

  * File = {Stmt} .
  * Stmt = Use | Item .
  * PathChar = ANY8 ! `<` .
  * Use = `use` `<` {PathChar} `>` .
  * Item = Item0 | Item1 | ItemN | ItemBlock .
  * Item0 = Func `;` .
  * Item1 = Func Item .
  * ItemN = Func ItemBlock .
  * ItemBlock = `{` { Item } `}` .
  * MOD = `*` |  `!` |  `#` | `%` .
  * Func = [MOD] IDENT `(` [ Arg { `,` Arg } ] `)` .
  * Arg = NamedArg | Value .
  * NamedArg = IDENT `=` Value .
  * Value = Integer | Float | IDENT | STRING | Array | Range .
  * Integer = INTEGER : interpreted as int64 using strtoll() .
  * Float = FLOAT : interpreted as ieeefloat64 using strtod() .
  * Array = `[` [ Value { `,` Value } ]  `]` .
  * Range = `[` Value `:` Value [ `:` Value ] `]` .

## Special Value Identifiers

  * `true` :: boolean, numeric value = 1,
  * `false` :: boolean, numeric value = 0,
  * `undef`: undefined value
  * `PI` :: float, numeric value = 3.14159265358979...

## Dimensions

Dimensions are generally interpreted as millimeters [mm].

## Coordinate Matrix

The positioning, scaling, and rotation of an object in 3D space is
realised by computing a coordinate matrix for each object.  Functors
`rotate`, `translate`, `scale`, `mirror`, and `multmatrix` modify this
coordinate matrix for its child structures.  Each 3D object is mapped
into the object coordinate system by multiplying each point of its
surface with the coordinate matrix.

The coordinate matrix is a 4x4 matrix where the last row is fixed at
[0,0,0,1].

If the determinant of an object's coordinate matrix is negative, it
means that the object is mirrored.  Internally, to implement this, all
polyhedra face paths are reversed to correctly reflect the mirroring,
i.e., so that even after mirroring, the order is again clockwise when
viewed from the outside.  This means that hob3l correctly handles
mirroring in all `mirror`, `scale`, and `multmatrix` transformations
(`translate` and `rotate` matrices have a determinant of 1, so no
mirroring can happen).

It was decided that it is an error if the coordinate matrix's
determinant becomes 0.  This is to find bugs in the model.  In the
future, we might have a command line option to just ignore those
objects (they collapse into emptiness if the determinant is 0).

## Functor Calls

Functors are defined individually in the following sections.

Functor arguments are all named.  Each functor defines the parameter
names by listing them, and defines which parameters are optional by
putting them in [] or {} brackets, and gives a default value for the
optional ones.  {} brackets indicate optional parameters that are only
recognised by name, not by position.

For each parameter, also the possible types are listed after a `::`.
Alternatives are possible, separated by `||`.

## Functors

### circle

2D object: ellipse.

```
circle([r]{,d,$fa,$fs,$fn});
```

Currently, 2D objects are not fully supported.

### color

Set the colour of substructures.

```
color(c[,alpha]) { ... }
```

  * `c` :: (array[3..4] of (0.0...0.1)) || string || undef
  * `alpha` :: (0.0...0.1), default=1.0

If `c` is a string, it is parsed as a case-insensitive
[CSS3 colour name](http://www.w3.org/TR/css3-color/).

If `c` is an array, the first 3 entries are the R,G, and B colour
components of the colour in the range 0..1, e.g. `[0,0,0]` is black and
`[1,1,1]` is white.

If `c` is `undef`, then the R,G, and B colour components are kept as is
for the children of this functor.  The alpha component is always reset
by this functor.

For `alpha`, 0 means fully transparent and 1 means fully opaque.

If `alpha` is given, the `c` array must have 3 entries.

If `alpha` is not given, the optoinal 4th component of `c` defines the
alpha component.

### cube

3D object: cuboid.

```
cube([size,center])
```

  * `size` :: (array[3] of (float != 0)) || (float != 0), default=1.0
  * `center` :: boolean, default=false

`size` defines the dimensions of the cube in X, Y, and Z dimensions.
An array value defines each dimension individually (`[x,y,z]`), a
single float defines all dimensions equally.

If `center` is non-false, this will center the cube at [0,0,0],
otherwise, the cube will extend into the positive X, Y, and Z axes.

### cylinder

3D object: cylinder.

```
cylinder([h,r1,r2,center]{,d,d1,d2,r,$fa,$fs,$fn})
```

  * `h` :: float > 0, default=1
  * `r1` :: float >= 0, default=1
  * `r2` :: float >= 0, default=1
  * `center` :: boolean, default=false
  * `r` :: float
  * `d1` :: float
  * `d2` :: float
  * `d` :: float
  * `$fn` :: integer, default=0
  * `$fa` :: integer, default=12, ignored
  * `$fs` :: integer, default=2, ignored

`h` is the height of the cylinder, i.e., the extension on the Z axis.

`r1` is the bottom radius of the circle around [0,0] in the XY plane.

`r2` is the top radius of the circle around [0,0] in the XY plane.

`center` defines how the cylinder is positioned on the Z axis: if
`center` is false, the cylinder extends into the positive Z axis.
Otherwise, it is centered at [0,0,0].

Some parameters are mutually exclusive:

  * `r` and `r1` must not both be specified.
  * `r` and `r2` must not both be specified.
  * `d` and `d1` must not both be specified.
  * `d` and `d2` must not both be specified.
  * `d` and `r` must not both be specified.
  * `r1` and `d1` must not both be specified.
  * `r2` and `d2` must not both be specified.

`d`, `d1`, `d2`, `r`, `r1`, `r2` define diameter or radius of the
cylinder.  The two radii `r1` and `r2` will be determined from
whatever is specified of `r1`, `r2`, `r`, `d1`, `d2`, `d`:

  * If `r` is specified, both `r1` and `r2` will be set to `r`.
  * If `d` is specified, `r1` and `r2` will be set to `d`/2.
  * If `d1` is specified, `r1` will be set to `d1`/2.
  * If `d2` is specified, `r2` will be set to `d2`/2.

The cylinder's circular shape in the XY plane is realised as a
polygon.  The number of vertices in this polygon is set by the `$fn`
parameter.  If `$fn` is smaller than 3, then `$fn` will be assumed to
be a large value instead.  One of the vertices of the polygon shape is
at y=0 in the positive x axis.

The cylinder becomes a cone if `r1` or `r2` are set to 0.

`r1` and `r2` must not both be 0.

### difference

Combine substructures by subtracting from the first non-empty
substructure.  This is the CSG 'SUB' operation, also referred to as
the Boolean 'AND NOT' operation.

```
difference() { ... }
```

The first non-empty substructure is the basis from which all other
substructures are subtracted.

_Caution_: SCAD has a complex definition of 'non-empty substructure':

The non-empty substructure is searched recursively starting at the
first child of this functor, skipping any empty substructures like
`group(){}` etc., and recursing into any recursive operation or module
or modifier, until the first functor is found that represents a 2D or
3D object.  Whether the object functor actually represents an empty
object is irrelevant for the search: any object functor is a non-empty
structure for this definition.

### group

Combine substructures by uniting them.  This is the CSG 'ADD'
operation, also referred to as the Boolean 'OR' operation.

```
group() { ... }
```

### intersection

Combine substructures by intersecting them.  This is the CSG 'CUT'
operation, also referred to as the Boolean 'AND' operation.

```
intersection() { ... }
```

### linear_extrude

Ignored. (Not yet implemented.)

### mirror

Mirror substructures.

```
mirror(v) { ... }
```

  * `v` :: (array[3] of float) != [0,0,0]

`v` is the direction vector of the plane at which to mirror the
substructures.

`mirror([X,Y,Z])` causes the coordinate matrix to be multiplied by:

```
| 1-2*x*x  -2*x*y   -2*x*z   0 |
| -2*x*y   1-2*y*y  -2*y*z   0 |
| -2*x*z   -2*y*z   1-2*z*z  0 |
| 0        0        0        1 |
```

where `[x,y,z]` is the unit vector of `[X,Y,Z]`.

### multmatrix

Modify coordinate matrix for substructures.

```
multmatrix(m) { ... }
```

  * `m` :: array[3..4] of array[3..4] of float

This specifies a 4x4 matrix.  The fourth column and row are optional
and both default to `0,0,0,1`.

The fourth row must have the value `0,0,0,1`.

The defined 4x4 matrix must be invertible.

A negative determinant of the given matrix cause mirroring.  This is
handled correctly as described [here](#coordinate-matrix).

`multmatrix([[a,b,c,d],[e,f,g,h],[i,j,k,l],[0,0,0,1]])` causes the
coordinate matrix to be multipled by:

```
| a  b  c  d |
| e  f  g  h |
| i  j  k  l |
| 0  0  0  1 |
```

### polygon

2D object: polygon.

```
polygon(points[,paths]);
```

Currently, 2D objects are not fully supported.

### polyhedron

3D object: polyhedron.

```
polyhedron(points[,faces]{,triangles,convexity});
```

  * `points` :: array[] of array[3] of float
  * `faces` :: array[] of array[] of integer
  * `triangles` :: array[] of array[3] of integer
  * `convexity` :: integer, ignored

`points` defines all the vertices of the polyhedron as `[x,y,z]`
coordinates.  No entry must be duplicate.

`faces` defines all the faces of the polyhedron as a list of 0-based
indices into the `points` array, which defines the path of each
polygon describing a face.  The vertices of each face must be
specified in clockwise direction when viewed from the outside of the
polyhedron.

The value of `faces` defaults to the value of `triangles`.

`faces` and `triangles` must not both be specified.

The polyhedron must be 2-manifold, i.e.:

  * There must be no holes, i.e., the solid must be specified completely
    with no face missing.
  * At each vertex, there must be a well-defined inside and outside, i.e.,
    the polyhedron must not touch itself in a vertex.
  * At each edge, there must be a well-defined inside and outside, i.e., the
    polyhedron must not touch itself in an edge.
  * At each face, there must be a well-defined inside and outside, i.e., the
    polyhedron must not touch itself in a face, nor must there be zero-thickness
    walls.
  * Each edge must have exactly two adjacent faces.

### rotate

Rotate substructures.

```
rotate(a[,v])) { ... }
```

#### If `v` is not specified

  * `a` :: array[3] of float

`rotate([x,y,z])` is equal to `rotate(z,[0,0,1]) rotate(y,[0,1,0]) rotate(x,[1,0,0])`.

#### If `v` is specified

  * `a` :: float
  * `v` :: (array[3] of float) != `[0,0,0]`.

Rotation of the substructures by `a` degrees around the axis specified
by `v`.

`rotate(a,[X,Y,Z])` causes the coordinate matrix to be multiplied by:

```
| x*x*d + c     x*y*d - z*s   x*z*d + y*s   0 |
| x*y*d + z*s   y*y*d + c     y*z*d - x*s   0 |
| x*z*d - y*s   y*z*d + x*s   z*z*d + c     0 |
| 0             0             0             1 |
```

where `[x,y,z]` is the unit vector of `[X,Y,Z]`, `c = cos(a)`, `s =
sin(a)`, `d = 1 -c`.

Rotation angles are in degrees.  If for a given angle representable in
int32, there is an exact solution to `sin` and/or `cos`, then this
exact value is used.  E.g. `cos(90) == 1` and `sin(30) = 0.5`,
exactly, so that `rotate(90,[1,0,0])` is an rotation by exactly 90
degrees around the X axis.

### scale

Scale substructures.

```
scale(v) { ... }
```

  * `v` :: (array[3] of (float != 0)) || (float != 0)

`v` defines the scaling of the substructures on X, Y, and Z axes.  For
equal scaling on all axes, a single value can be used.

Negative scale values cause mirroring on the given axis/axes.  This is
handled correctly as described [here](#coordinate-matrix).

`scale([x,y,z])` causes the coordinate matrix to be multiplied by:

```
| x  0  0  0 |
| 0  y  0  0 |
| 0  0  z  0 |
| 0  0  0  1 |
```

### sphere

3D object: ellipsoid.

```
sphere([r]{,d,$fa,$fs,$fn});
```

This is not yet supported.

### square

2D object: rectangle.

```
square([size,center]);
```

Currently, 2D objects are not fully supported.

### text

Ignored. (Not yet implemented.)

### translate

Translate substructures.

```
translate(v) { ... }
```

  * `v` :: array[3] of float

`v` defines the translation of the substructures on X, Y, and Z axes.

`translate([x,y,z])` causes the coordinate matrix to be multiplied by:

```
| 1  0  0  x |
| 0  1  0  y |
| 0  0  1  z |
| 0  0  0  1 |
```

### union

```
union() { ... }
```

This is exactly equivalent to [group](#group).
