# Hob3l's Dialect Of SCAD

## Table Of Contents

  * [Table Of Contents](#table-of-contents)
  * [Introduction](#introduction)
  * [OpenSCAD CSG Format](#openscad-csg-format)
  * [Informal Overview](#informal-overview)
      * [Broken SCAD Syntax](#broken-scad-syntax)
  * [Morphosyntax](#morphosyntax)
      * [Notation](#notation)
      * [Morphology](#morphology)
      * [Syntax](#syntax)
  * [Special Value Identifiers](#special-value-identifiers)
  * [Dimensions](#dimensions)
  * [Coordinate Matrix](#coordinate-matrix)
  * [Functor Calls](#functor-calls)
  * [Ignored Children](#ignored-children)
      * [Ignored Projection](#ignored-projection)
  * [Functors](#functors)
      * [circle](#circle)
      * [color](#color)
      * [cube](#cube)
      * [cylinder](#cylinder)
      * [difference](#difference)
      * [group](#group)
      * [import](#import)
      * [intersection](#intersection)
      * [linear_extrude](#linear_extrude)
      * [mirror](#mirror)
      * [multmatrix](#multmatrix)
      * [polygon](#polygon)
      * [polyhedron](#polyhedron)
      * [projection](#projection)
      * [rotate](#rotate)
          * [If `v` is not specified](#if-v-is-not-specified)
          * [If `v` is specified](#if-v-is-specified)
      * [rotate_extrude](#rotate_extrude)
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
subset, which is described in this document.

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

Up to now, there are no extensions introduced by Hob3l in the syntax,
so any file Hob3l reads should also be a valid input for OpenSCAD.
However, the semantics occasionally has less restrictions than
OpenSCAD, like mirroring objects using `scale` instead of `mirror`.

This describes OpenSCAD 2015.3 syntax.

## OpenSCAD CSG Format

OpenSCAD can write a flattened `.csg` file in SCAD syntax that removes
a lot of things that Hob3l cannot read, so the `.csg` exporter of
OpenSCAD can often be used to make a file readable by Hob3l.  This
`.csg` exporter is generally very fast and thus a viable preprocessor
step for running Hob3l.

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
    undef
    true
    false
    PI

    group() { ... }
    union() { ... }          // interpreted the same as 'group'
    intersection() { ... }
    difference() { ... }

    sphere(r, d, $fa, $fs, $fn)
    cube(size, center)
    cylinder(h, r, r1, r2, d, d1, d2, center, $fn)
    polyhedron(points, faces, triangles)
    import(file, layer, convexity)

    polygon(points, paths, convexity)
    circle(r, d, $fa, $fs, $fn)
    square(size, center)

    linear_extrude(height, center, slices, twist, scale, $fa, $fs, $fn)

    multmatrix(m)
    translate(v)
    mirror(v)
    scale(v)
    rotate(a,v)

    color(c, alpha)
```

Some additional items are parsed, but currently ignored:

```
    text(...)
```

### Broken SCAD Syntax

One irritating detail about SCAD syntax that I found was the
interpretation of children elements of `difference()`: the first
non-ignored child is interpreted as positive, all others are negative.

However, neither the OpenSCAD syntax nor the syntax description make
it immediately clear which thing is an ['ignored
child'](#ignored-children), because even an empty `group(){}` and
empty 'linear_extrude(){}' are skipped, and even recursively so.

The parser of `hob3l` spends quite some effort on determining which
one is the first non-ignored child of `difference`.  Whether it does
that in the same way as OpenSCAD, I can only hope for.  I think this
part of the SCAD syntax is broken.

## Morphosyntax

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
interpreted as US-ASCII character set, exceptionally allowing
uninterpreted 8-bit characters when stated explicitly.  In particular,
input files with paths and strings encoded in UTF-8 are trivially
supported, but not rejected for invalid UTF-8 encoding, nor converted
to a different output character set, e.g. in error messages.

The sequence of bytes is separated into a sequence of TOKENs according
to the morphology description in this section.  Based on this tokens,
the syntax will be defined.  The syntax rules drive in which order to
apply the morphology rules to the input file, e.g., in one rules, the
syntax may call for a `<` to follow, where in the next, it requires a
PATH, which also starts with `<` but will select a longer sequence of
characters.

In this description, ANY is a single US-ASCII printable character.
ANY8 is any byte value, including 8-bit characters outside of
US-ASCII.  No character set is assumed for 8-bit characters, i.e.,
they are processed as is, e.g., inside comments and strings.

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
  * PATHCHAR = ANY8 ! `>` .
  * PATH = `<` {PATHCHAR} `>` .
  * TOKEN = IDENT | INTEGER | FLOAT | STRING | LINECOM | MULTICOM | WHITE .
  * SYMBOL = ANY : if no TOKEN matches .

Example IDENT: `a`, `xyz`, `$fs`

Example NUM: `77`, `2.8`, `+5e-9`

Example STRING: `"abc"`, `"a\"b\\c"`

Example SYMBOL: `+`, `/`, `#`

### Syntax

The syntax is defined as follows, ignoring any WHITE, LINECOM and
MULTICOM tokens.

In the following, any `...` tokens must also match IDENT or SYMBOL,
i.e., they only match if they were analysed by the morphology as IDENT
or SYMBOL.  In particular, they cannot be prefixes of TOKEN, because
the morphology rules are applied first.  E.g. `use` cannot be the
prefix of `useful`, and `/` cannot be the prefix of `/*`.

  * File = {Stmt} .
  * Stmt = Use | Item .
  * Use = `use` PATH .
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
[0,0,0,1].  (Because of this redundancy, Hob3l uses only 4x3 matrices
internally to speed up operations.)

If the determinant of an object's coordinate matrix is negative, it
means that the object is mirrored.  Internally, to implement this, all
polyhedron face paths are reversed to correctly reflect the mirroring,
i.e., so that after mirroring, the vertex order is again clockwise
when viewed from the outside.  This means that hob3l correctly handles
mirroring in all `mirror`, `scale`, and `multmatrix` transformations
(`translate` and `rotate` matrices have a determinant of 1, so no
mirroring can happen).

It was decided that it is an error if the coordinate matrix's
determinant becomes 0.  This is to find bugs in the input model early
and more easily.  In the future, a command line option may be added to
just ignore those objects (they collapse into emptiness if the
determinant is 0).

## Functor Calls

Functors are defined individually in the following sections.

Functor arguments are all named.  Each functor defines the parameter
names by listing them, and defines which parameters are optional by
putting them in [] or {} brackets, and gives a default value for the
optional ones.  {} brackets indicate optional parameters that are only
recognised by name, not by position.

For each parameter, also the possible types are listed after a `::`.
Alternatives are possible, separated by `||`.

## Ignored Children

As mentioned, I think the OpenSCAD syntax is broken wrt. the
definition of the 'first non-ignored child', which makes `difference`
particularly hard to define formally, and confusing to use.  This
extends to `intersection`, too, because that function also ignores some
children.

In essence, the decision of which child is ignored needs semantics.
In the case of `projection`, ignoring ('projection failed') is decided
even [dynamically at runtime](#ignored-projection).

The following defines recursively a function `IGNORE` to decide
whether a child is ignored.

In the definition, children of a functor are written `{ C1; C2;
... Cn; }` and a group of children as `Cs`.  `;` matches the same as
`{}`, e.g., `group();` matches `group() Cs` with `Cs = {}`.

Parameters of functors are matched by `...`.

Note that OpenSCAD ignores 3D objects with a warning inside 2D
context, that's why the definition is so complex inside
`linear_extrude` et.al.  Hob3l handles those as an error so this is
irrelevant.

Note that `for` and `intersection_for` never iterate zero times (even
for ranges like `[1:0]`), so they just pass through whether their children
are all ignored.

The definition ignores `use`, `function`, `module` for now, because it
is assumed that these only occur at toplevel, never as a child.

`F` is a functor (like `group` or `cube`).

  * IGNORE(`{}`) = true

  * IGNORE(`X = Y`) = true

  * IGNORE(`{ C1; C2; ... Cn; }`) =
    IGNORE(`C1`) && IGNORE(`{ C2; ... Cn; }`)

  * IGNORE(`F(...) Cs`) = IGNORE(`Cs`)
    where F in
        `group`, `union`, `intersection`, `difference`,
        `translate`, `scale`, `rotate`, `mirror`,
        `color`, `resize`, `multmatrix`,
        `render`, `hull`, `minkowski`, `offset`,
        `for`, `intersection_for`

  * IGNORE(`F(...) Cs`) = IGNORE2D(`Cs`)
    where F in
        `linear_extrude`, `rotate_extrude`

  * IGNORE(`F(...);`) = false
    where F in
        `polygon`, `circle`, `square`, `text`,
        `polyhedron`, `sphere`, `cube`, `cylinder`,
        `import`, `surface`

  * *OpenSCAD*:

    IGNORE(`projection(...) Cs`) =
    IGNORE(`Cs`) || (`the resulting polygon is empty`)

  * *Hob3l*:

    IGNORE(`projection(...) Cs`) = IGNORE(`Cs`)

  * IGNORE2D(`F(...);`) = true
    where F in
        `polyhedron`, `sphere`, `cube`, `cylinder`,
        `import`, `surface`,
        `render`, `hull`, `minkowski`, `resize`

  * IGNORE2D(X) = IGNORE(X) otherwise

Note the inconsistency: 2D objects in 3D context are not ignored (in
OpenSCAD, they are extruded to 1mm in F5 view and treated as empty in
F6 view), but 3D objects in 2D context are ignored.

### Ignored Projection

This is a difference in behavior between Hob3l and OpenSCAD.
OpenSCAD, ignores `projection` when the result of the projection
becomes empty.  Because I think it should be decidable from the input
file without evaluating it whether it computes 'A-B-C' or 'B-C', Hob3l
behaves differently.  Hob3l does not ignore `projection` when the
projection polygon becomes empty, but handles it like a 2D object.

This section is an example of OpenSCAD behaviour.

In the following, `linear_extrude` is the first non-ignored child of
`difference` (both OpenSCAD and Hob3l):

```
difference() {
    linear_extrude(height=10) {
        projection(cut = true) translate([0,0,-0.1]) cube(8);
    }
    sphere(5);
}
```

In the following, `sphere` is the first non-ignored child of
`difference` in OpenSCAD.  With Hob3l, it is still `linear_extrude`.


```
difference() {
    linear_extrude(height=10) {
        projection(cut = true) translate([0,0,+0.1]) cube(8);
    }
    sphere(5);
}
```

Note that the difference is in the value `-0.1` vs. `+0.1`, which may
come from complex computation, but totally changing the outcome of the
result, deciding whether `sphere(5)` is subtracted or taken as the
main positive object.

## Functors

### circle

2D object: ellipse.

```
circle([r]{,d,$fa,$fs,$fn});
```

  * `r` :: float > 0, default=1
  * `d` :: float
  * `$fn` :: integer, default=0
  * `$fa` :: float, ignored
  * `$fs` :: float, ignored

Some parameters are mutually exclusive:

  * `d` and `r` must not both be specified.

`d`, `r` define diameter or radius of the sphere.  The radius `r`
will be determined if `d` is specified and `r` is not:

  * If `d` is specified, `r` will be set to `d`/2.

This specifies a circle centered at [0,0] with the radius `r`.

The circle is approximated by a polygon with `$fn` vertices.  One
vertex is at y=0 in the positive x axis.

### color

Graphics context modifier: set the colour of substructures.

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
  * `$fa` :: float, ignored
  * `$fs` :: float, ignored

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

CSG operation: combine substructures by subtracting from the first
non-ignored child.  This is the CSG 'SUB' operation, also referred to
as the Boolean 'AND NOT' operation.

```
difference() { C1; C2; ... Cn; }
```

The first child `Ci` for which `IGNORED(Ci)` is false is the base
object from which all subsequent children are subtracted.

_Caution_: SCAD has a complex definition of 'ignored child':
see [the definition](#ignored-children).

### group

CSG operation: combine substructures by uniting them.  This is the CSG
'ADD' operation, also referred to as the Boolean 'OR' operation.

```
group() { ... }
```

### import

3D object: load a polyhedron from a file.

```
import(file[,layer,convexity])
```

  * `file` :: string
  * `layer` :: string, ignored
  * `convexity` :: integer, ignored

This loads a polyhedron from the file whose path is specified by
`file`.  If `file` is a relative path name, then it is interpreted
relative to the file where the `import` was located.

Currently, files in text STL format are supported.  The vertices are
assumed to be ordered in the opposite order of what the SCAD format
uses, i.e., when viewed from the outside, each face's vertices run
counter-clockwise.  This corresponds to the right hand rule, where the
thumb is the face normal and the other fingers indicate the vertex
order.

The normal is used to check that the vertices are ordered the expected
way: if the sign of the computed normal is opposite, the vertices are
reversed to restore the right hand rule order.

The input polyhedron must be 2-manifold just like for the `polyhedron`
functor.

_OpenSCAD compatibility_:

  * OpenSCAD reads more file formats than Hob3l.

  * OpenSCAD and also Slic3r are much more forgiving than Hob3l if the
    input STL is not a proper 2-manifold.  Hob3l rejects many files,
    which is frustrating.  The underlying problem is that many STL
    files are just not correct 2-manifolds, although this should be
    the case according to the specification.  Empty triangles (with
    three collinear edges) are very frequent.  And Hob3l really cannot
    handle them at the moment, because of how its slicing algorithm
    works.

### intersection

CSG operation: combine substructures by intersecting them.  This is
the CSG 'CUT' operation, also referred to as the Boolean 'AND'
operation.

```
intersection() { C1; C2; ... Cn; }
```

This intersects all children `Ci` for which `IGNORE(Ci)` is false.
The ones where `IGNORE(Ci)` is true are ignored in the operation and
do not cause the result to be empty.

Note again: an intuitively empty child will not necessarily produce an
empty result.  Only if the child is not ignored will the result be
empty. E.g. `cube(0)` is not ignored, so it will make the result
empty, but `group(){}` is ignored and consequently has no influence on
the result.

_Caution_: SCAD has a complex definition of 'ignored child':
see [the definition](#ignored-children).

### linear_extrude

3D object: make a polyhedron from a polygon by extruding it in the z
axis.

```
linear_extrude({height, center, slices, twist, scale,
    convexity, $fn, $fa, $fs})
{ ... }
```

  * `height` :: float != 0
  * `center` :: boolean, default=false
  * `slices` :: integer >= 1, default=1
  * `twist` :: float, default=0
  * `scale` :: (array[2] of (float >= 0)) || float, default=1
  * `convexity` :: integer, ignored
  * `$fn` :: integer, default=0, ignored
  * `$fa` :: float, ignored
  * `$fs` :: float, ignored

This renders an extrusion into the Z axis of the 2D child shapes in
the XY plane.

The extrusion ranges on the Z axis from 0..`height` if `center` is
false, and from `-height/2`..`+height/2` if `center` is true.

The 2D children are first merged into a single polygon with possibly
multiple paths, and each path is extruded into a separate polyhedron.

Each polyhedron consists of `slices+1` layers of vertices stacked
along the Z axis, where each layer is derived from the original 2D
path.

Each polyhedron's bottom face (at the lowest z coordinate) is equal to
its original 2D path.  The polyhedron's top face is equal to the path
after being fully rotated by `twist` degrees clockwise when viewed
from the top, and then scaled by `scale` in X and Y directions.

The intermediate `slice-1` layers use regularly interpolated twist and
scale amounts for defining their vertices.

This renders the sides as rectangles unless `twist` is non-0.  In this
case, the rectangles would not be planar, so they need to be split
into triangles to get two planar faces.  For each broken rectangular
face, the two triangles are arranged in such a way that the connecting
edge is inward, i.e., the broken side face becomes concave.

If `scale` is `[0,0]`, the top layer of the polyhedron collapses into
a single point on the Z axis.

`scale` must not have only one component that is 0.  Either both must
be non-0 or both must be 0.  The shape that would result otherwise is
currently not supported in rendering by Hob3l.

Note: Internally, Hob3l uses a XOR operation to implement this with
intersecting 2D paths, which are not representable in SCAD format (XOR
is not usually helpful in 3D space).  This means that the
`--dump-csg3` output cannot be read back as input file.  The XOR node
is represented by a `hob3l_xor` functor.

_OpenSCAD compatibility_:

  * `slices`:
      * OpenSCAD derives default for `slices` from `$fa`, `$fs`
      * Hob3l assumes 1

  * `scale`: negative components:
      * OpenSCAD assumes 0
      * Hob3l rejects the input ('scale is negative')

  * `scale`: one component is 0, the other is non-0:
      * OpenSCAD produces a non-manifold, but often visually correct output
      * Hob3l rejects the input ('not implemented')

  * `$fn, $fa, $fs` are ignored Hob3l for now, because scopes and
    variables are not implemented yet.  E.g., `$fn` essentially resets the
    value for all children of linear_extrude.  I.e., in OpenSCAD, the
    following are equivalent, but Hob3l only accepts the last one at the
    moment:

    ```
    linear_extrude(height=20, $fn=20) { circle(10); }
    linear_extrude(height=20) { $fn = 20; circle(10); }
    linear_extrude(height=20) { circle(10, $fn=20); }
    ```

    OpenSCAD's `.csg` output resolves this and sets `$fn` at the children,
    so that the `.csg` output works fine with Hob3l.

### mirror

Transformation: mirror substructures.

```
mirror(v) { ... }
```

  * `v` :: (array[2..3] of float) != [0,0,0]

`v` is the direction vector of the plane at which to mirror the
substructures.

If an array with only 2 entries is given, the third one is assumed to
be 0.

`mirror([X,Y,Z])` causes the coordinate matrix to be multiplied by:

```
| 1-2*x*x  -2*x*y   -2*x*z   0 |
| -2*x*y   1-2*y*y  -2*y*z   0 |
| -2*x*z   -2*y*z   1-2*z*z  0 |
| 0        0        0        1 |
```

where `[x,y,z]` is the unit vector of `[X,Y,Z]`.

_OpenSCAD compatibility_:
  * `mirror([0,0,0]) X`:
      * OpenSCAD treats this like `X` (without warning),
      * Hob3l rejects this with an error (`[0,0,0]` cannot be normalised).

### multmatrix

Transformation: modify coordinate matrix for substructures.

```
multmatrix(m) { ... }
```

  * `m` :: array[2..4] of array[2..4] of float

Each entry of `m` is one row of a 4x4 matrix.

In total, `m` specifies a full 4x4 matrix.  Any entries that are
missing from a 4x4 matrix are filled from a unit matrix, i.e., the
third row and column default to `0,0,1,0` and the fourth row and
column default to `0,0,0,1`.

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

_OpenSCAD compatibility_:
  * `multmatrix(m) X`: where `m` is not a two dimensional array:
      * OpenSCAD treats this like `X` (without warning),
      * Hob3l rejects this with an error.
  * `multmatrix(m) X`: where `m` is not invertible:
      * OpenSCAD applies the matrix and collapses `X` into 2D,
      * Hob3l rejects this with an error (determinant is 0)

### polygon

2D object: one or multiple polygon paths.

```
polygon(points[,paths]);
```

  * `points` :: array[3..] of array[2] of float
  * `paths` :: array[] of array[3..] of integer || undef,
     default=`[[0,1,...points.size-1]]`
  * `convexity` :: integer, ignored

`points` defines all vertices of the polygon as `[x,y]` 2D
coordinates.  No entry in `points` must be duplicate.

`paths` defines all paths of the polygon that each defines the outline
of a path by a list of 0-based indices into the `points` array.  If
`paths` is not given or is `undef`, it defaults to a singleton list of
a list of all indices in `points`, i.e., `points` defines the outline
of the polygon directly.

The entries in `paths` are normalised so that they run clockwise from
viewed from 'above', i.e., from the positive Z axis.  This corresponds
to the left hand rule, where the thumb is the Z axis in positive
direction and the other fingers indicate the vertex order.

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
3D coordinates.  No entry must be duplicate.

`faces` defines all the faces of the polyhedron as a list of 0-based
indices into the `points` array, which defines the path of each
polygon describing a face.  The vertices of each face must be
specified in clockwise direction when viewed from the outside of the
polyhedron.  This corresponds to the left hand rule, where the thumb
is the face normal and the other fingers indicate the vertex order.

The value of `faces` defaults to the value of `triangles`.

`faces` and `triangles` must not both be specified.

The polyhedron must be 2-manifold, i.e.:

  * There must be no holes, i.e., the solid must be specified completely
    with no face missing.
  * Each edge must have exactly two adjacent faces.
  * At each face, there must be a well-defined inside and outside, i.e., the
    polyhedron must not touch itself in a face, nor must there be zero-thickness
    walls.
  * At each edge, there must be a well-defined inside and outside, i.e., the
    polyhedron must not touch itself in an edge.
  * At each vertex, there must be a well-defined inside and outside, i.e.,
    the polyhedron must not touch itself in a vertex.

The last criterion causes no error message, and the slicing algorithm
of Hob3l often copes with this, but is does not handle this in all
cases.  This is why Hob3l's `rotary_extrude` algorithm avoids
generating this.

### projection

2D object: cut out a 2D slice from a 3D model.

```
projection([cut]{,convexity}) { ... }
```

  * `cut` :: boolean, default=false
  * `convexity` :: integer, ignored

Project a 3D object into a 2D object in the XY plane by cutting out
the slice at z=0.

Hob3l can only handle this for `cut=true`, i.e., to cut a slice.
Hob3l cannot perform the 3D operation of casting a shadow of the whole
object into the XY plane, because no 3D algorithms are implemented.

The result of the projection is a 2D object.

_OpenSCAD compatibility_:

  * If the result of the projection is empty:
      * OpenSCAD ignores the projection in `intersection` and `difference`,
      * Hob3l does not ignore this for this reason, in order to ensure that
        which child is ignored can be determined
        without evaluating anything.  Hob3l ignores this exactly if all the
        children of `projection` are ignored, i.e., the criterion is the
        same as with `rotate` etc.

### rotate

Transformation: rotate substructures.

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
sin(a)`, `d = 1-c`.

Rotation angles are in degrees.  If for a given angle representable in
int32, there is an exact solution to `sin` and/or `cos`, then this
exact value is used.  E.g. `cos(90) = 1` and `sin(30) = 0.5`,
exactly, so that `rotate(90,[1,0,0])` is a rotation by exactly 90
degrees around the X axis.

### rotate_extrude

3D object: make a polyhedron by spinning a polygon around the z axis.

```
rotate_extrude({angle,convexity,$fn,$fa,$fs}) { ... }
```

  * `angle` :: float 0..360, default = 360
  * `convexity` :: integer, ignored
  * `$fn` :: integer >= 2, default = 0
  * `$fa` :: float, ignored
  * `$fs` :: float, ignored

This takes a 2D scene and rotates it around the Z axis to make a
torus-like polyhedron.  It uses `$fn` torus segments for the rotation.
If `angle` is 360, this generates a full torus, starting with the
first step in the negative X axis.  This is unlike other circular
operations, but it is legacy OpenSCAD behaviour.  With a value other
than 360, rotation starts in the positive X axis as usual.

The X coordinates of the points of the 2D children must all be
either >=0 or <=0.  Mixed positive and negative X coordinates are currently
not supported.  X coordinates equal to 0 can be combined with either
alternative.

`angle` is in degrees and produces exact results in the internal `sin`
and `cos` computations if possible.  If `angle` is smaller than 360,
this generates an arc out of the full torus with the given angle,
starting at the positive x axis for the first step and running
counter-clockwise when viewed from the top.

  * If angle is >= 360, then `$fn` is adjusted to be at least 3.
  * If angle is >= 180, then `$fn` is adjusted to be at least 2.
  * If angle is < 180, then `$fn` may be as small as 1.
  * If `$fn` is 0 (the default), then the torus will be rendered with
    a number of steps set by the `max-fn` parameter (e.g.
    on the command line option).

Note that this ignores `$fa` and `$fs`, which usually means that `$fn`
needs to be supplied for a result compatible with OpenSCAD.

_OpenSCAD compatibility_:

  * Hob3l is able to generate 2-manifold polyhedra in all
    circumstances, even if points touch the Z axis (i.e., the 2D scene
    has points with x==0).  Hob3l achieves this by not rendering a
    full torus, because this could generate a singleton vertex where
    the polyhedron is not 2-manifold. Instead, Hob3l splits the full
    torus into 2 pieces and merges them.  The subsequent 2D algorithms
    of Hob3l cope with this equivalent form.
  * OpenSCAD uses the usual heuristics for finding the number of steps,
    while Hob3l relies on `$fn` to be manually set or using a command
    line defined default value.
  * The `angle` != 360 case is implemented in Hob3l without comparing
    the result experimentally with OpenSCAD 2016.x, because only
    OpenSCAD 2015.3 was used, which does not support this feature.  The
    hook from the OpenSCAD documentation does look the same, though.

### scale

Transformation: scale substructures.

```
scale(v) { ... }
```

  * `v` :: (array[2..3] of (float != 0)) || (float != 0)

`v` defines the scaling of the substructures on X, Y, and Z axes.  For
equal scaling on all axes, a single value can be used.

If an array with only 2 values is given, the third value is assumed to
be 1.

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

  * `r` :: float != 0
  * `d` :: float
  * `$fn` :: integer, default=0
  * `$fa` :: float, ignored
  * `$fs` :: float, ignored

Some parameters are mutually exclusive:

  * `d` and `r` must not both be specified.

`d`, `r` define diameter or radius of the sphere.  The radius `r`
will be determined if `d` is specified and `r` is not:

  * If `d` is specified, `r` will be set to `d`/2.

This specifies a sphere centered at [0,0,0] with the radius `r`.

If `$fn` is non-0 but less than or equal to the `max-fn` parameter
(e.g., set on the command line), then this renders a polyhedron like
OpenSCAD 2015.3: $fn, or at least 3, will be the number of vertices in
a polygon.

if `fn` is greater than `max-fn`, then instead, Hob3l computes for
each slice the ellipse cut from the ellipsoid defined by the sphere
with its coordinate matrix.  The resulting ellipse will be converted
into a polygon with `$fn` vertices.  One of the vertices of the
polygon shape is at y=0 in the positive x axis.

This difference in rendering compared to OpenSCAD has numeric
advantages for approximating the sphere in Z direction, particularly
when the sphere is rotated.  It has the disadvantage of rendering
spheres differently from OpenSCAD, particularly at small values of
`$fn` where the sphere will still be perfectly round (in steps of
layer thickness) in Hob3l, while in OpenSCAD, it will be a polygon
when viewed from the side.

### square

2D object: rectangle.

```
square([size,center]);
```

  * `size` :: (array[2] of (float != 0)) || float, default=1
  * `center` :: boolean, default=false

This defines a rectangle, i.e., an optionally scaled and translated
unit square.

`square()` is equivalent to
`polygon(points=[[0,0],[1,0],[0,1],[1,1]],paths=[[0,2,3,1]])`.

`square(center=true)` is equivalent to `translate([-.5,-.5,0]) square()`.

`square(size=[a,b])` is equivalent to `scale([a,b,]) square()`.

`square(size=[a,b],center=true)` is equivalent to `scale([a,b,])
translate([-.5,-.5,0]) square()`.

### text

Ignored. (Not yet implemented.)

### translate

Transformation: translate substructures.

```
translate(v) { ... }
```

  * `v` :: array[2..3] of float.

If `v` has only 2 entries, the z component is assumed to be 0.

`v` defines the translation of the substructures on X, Y, and Z axes.

`translate([x,y,z])` causes the coordinate matrix to be multiplied by:

```
| 1  0  0  x |
| 0  1  0  y |
| 0  0  1  z |
| 0  0  0  1 |
```

### union

CSG operation

```
union() { ... }
```

This is exactly equivalent to [group](#group).
