# Hob3l's Dialect Of SCAD

## Table Of Contents

  * [Table Of Contents](#table-of-contents)
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

## Morphology

The input file is separated into a sequence of tokens of the following
kinds:

  * IDENTIFIER

    `a`, `xyz`, `$fs`

    Sequence of characters starting with `$` or `_` or an alphabetic
    character (`a`..`z`, `A`..`Z`), followed by a sequence or `_` or a
    decimal digit (`0`..`9`) or an alphabetic character.

  * INTEGER or FLOAT

    `77`, `2.8`, `+5e-9`

    Sequence of characters starting with `+`, `-`, `.`, or a decimal digit (`0`..`9`),
    consisting altogether of the following subsequences of characters:
      * optionally, `+` or `-`, then,
      * a potentially empty sequence of decimal digits (`0`..`9`), then,
      * optionally, `.`, followed by a potentially empty sequence of decimal
        digits, then
      * optionally, `e` or `E`, followed optionally by `-` or `+`, and then
        by a non-empty sequence of decimal digits.

    If the number contains `.` or `e` or `E`, it is classified as an
    INTEGER, otherwise as a FLOAT.

  * STRING

    `"abc"`, `"a\"b\\c"`

    Sequence of characters starting with a double quote, followed by a potentially
    empty sequence of
      * a non-double quote character,
      * a backslash character followed by another arbitrary character.

  * Line Comment

    `// text`

    Sequence of characters starting with a double `/` followed by a potentially
    empty sequence of non-newline characters.

  * Multiline Comment

    `/* text */`

    Sequence of characters starting with `/*` followed by a
    potentially empty sequence of characters, not containing the
    sequence `*/`, followed by `*/`.

  * White Space

    Sequence of white space characters (ASCII 32, Line break, Tab).

  * SYMBOL: Single printable ASCII character

    `+`, `/`, `#`

    Any character not among those used above to start tokens is a symbolic
    token on its own.

## Syntax

The syntax is defined ignoring any comment and white space tokens.

  * File = Stmt*
  * Stmt = Use | Item
  * Use = `use` `<` (not `<`)* `>`
  * Item = Item1 | Item2 | ItemBlock
  * Item1 = Func `;`
  * item2 = Func ItemBlock
  * ItemBlock = `{` Item* `}`
  * Func = IDENTIFIER `(` [ Arg { `,` Arg }* ] `)`
  * Arg = NamedArg | Value
  * NamedArg = IDENTIFIER `=` Value
  * Value = Integer | Float | IDENTIFIER | STRING | Array | Range
  * Integer = INTEGER, interpreted as int64 using strtoll()
  * Float = FLOAT, interpreted as ieeefloat64 using strtod()
  * Array = `[` [ Value { `,` Value }* ]  `]`
  * Range = `[` Value `:` Value [ `:` Value ] `]`

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
