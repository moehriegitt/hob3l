# Hob3l's Dialect Of SCAD

## Table Of Contents

  * [Table Of Contents](#table-of-contents)
  * [Literals](#literals)
  * [Special Value Literals](#special-value-literals)
  * [Functor Calls](#functor-calls)
  * [Functors](#functors)
      * [color](#color)
      * [group](#group)
      * [union](#union)

## Literals

  * Identifiers
  * Integers
  * Floats
  * Strings
  * Arrays
  * Ranges

## Special Value Literals

  * `true` :: boolean, numeric value = 1,
  * `false` :: boolean, numeric value = 0,
  * `undef`: undefined value
  * `PI` :: float, numeric value = 3.14159265358979...

## Functor Calls

Functors are defined individually in the following sections.

Functor arguments are all named.  Each functor defines the parameter
names by listing them, and defines which parameters are optional by
putting them in [] brackets, and gives a default value for the optional
ones.

For each parameter, also the possible types are listed after a `::`.
Alternatives are possible, separated by `||`.

## Functors

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

3D object functor: a cuboid.

```
cube([size,center])
```

  * `size` :: (array[3] of float) || float, default=1.0
  * `center` :: boolean, default=false

`size` defines the dimensions of the cube in X, Y, and Z dimensions.

If `center` is non-false, this will center the cube at [0,0,0],
otherwise, the cube will extend into the positive X, Y, and Z axes.

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

### union

```
union() { ... }
```

This is exactly equivalent to [group](#group).
