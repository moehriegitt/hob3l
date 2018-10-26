# Hob3l's Dialect Of SCAD

## Table Of Contents

  * [Table Of Contents](#table-of-contents)
  * [Introduction](#introduction)
  * [Syntax](#syntax)
      * [Notation](#notation)
      * [Morphology](#morphology)
      * [Syntax](#syntax)
  * [Special Value Identifiers](#special-value-identifiers)
  * [Dimensions](#dimensions)
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
      * [scale](#scale)
      * [sphere](#sphere)
      * [square](#square)
      * [text](#text)
      * [translate](#translate)
      * [union](#union)

## Introduction

This document tries to be more formal about SCAD syntax than the
OpenSCAD documentation and to handle all corner cases and answer all
questions about what syntax is accepted by Hob3l.

When writing Hob3l, it was necessary to run OpenSCAD in experiments to
find out how it really parses the input file, to be compatible.  This
document tries to avoid this kind of underspecificity.

That said, this document is probably incomplete anyway, but that is a
bug.

## Syntax

### Notation

This file uses a modified Wirth Syntax Notation (WSN) to define the
syntax.  The following differences exist with standard WSN:

  * Instead of double quotes, we use backticks so that the text
    looks better in .md format.

  * A negation `!` is introduced to signify 'but not this'.

  * A range operator `...` for characters is introduced to abbreviate
    the syntax definitions.

  * A comment after `:` for explanations, restrictions, etc.

  * C syntax string escaping is used.

### Morphology

The input file is processed as a sequence of TOKENs.  In this
description, ANY is any US-ASCII printable character.

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
  * STRCHAR = ANY ! ( `"` | `\` ) .
  * STRSEQ = STRCHAR | `\` ANY .
  * STRING = `"` {STRSEQ} `"` .
  * LCCHAR = ANY ! `\n`
  * LINECOM = `//` {LCCHAR} .
  * MCCHAR = {ANY} : if the sequence does not contain `*/` .
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
  * PathChar = ANY ! `<` .
  * Use = `use` `<` {PathChar} `>` .
  * Item = Item1 | Item2 | ItemBlock .
  * Item1 = Func `;` .
  * item2 = Func ItemBlock .
  * ItemBlock = `{` { Item } `}` .
  * Func = IDENT `(` [ Arg { `,` Arg } ] `)` .
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

Dimensions are generally interpreted as [mm].

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

  * `size` :: (array[3] of float) || float, default=1.0
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

Ignored.

### mirror

Mirror substructures.

```
mirror(v) { ... }
```

### multmatrix

Modify coordinate matrix for substructures.

```
multmatrix(m) { ... }
```

### polygon

2D object: polygon.

```
polygon(points[,paths]);
```

### polyhedron

3D object: polyhedron.

```
polyhedron(points[,faces]{,triangles});
```

### rotate

Rotate substructures.

```
rotate(a[,v])) { ... }
```

### scale

Scale substructures.

```
scale(v) { ... }
```

### sphere

3D object: ellipsoid.

```
sphere([r]{,d,$fa,$fs,$fn});
```

### square

2D object: rectangle.

```
square([size,center]);
```

### text

Ignored.

### translate

Translate substructures.

```
translate(v) { ... }
```

### union

```
union() { ... }
```

This is exactly equivalent to [group](#group).
