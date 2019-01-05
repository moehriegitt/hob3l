# Nozzl3 Sans Font Family

['Nozzl3' is pronounced like 'nozzle'.]

The Nozzl3 Sans font family is a new font specifically designed for
Hob3l.  The design criteria are:

  * no serifs
  * printable in plastic even at small sizes
  * reasonably good-looking
  * good Unicode support

The font is currently not available as an OpenType font (or the like),
but is a C module linked with Hob3l.

For an application like Hob3l, the font is probably way overdone, but
the author likes fonts and Unicode.

## Reasonably Good-Looking

There is always a trade-off between time spent and niceness.
'Reasonably good-looking' means that a good balance was tried.

This font has no serifs, which simplifies the design.  A special
generator for the font was implemented so that the font could be
programmed in C instead of being designed in a font design
application.  This is a bit like Metafont, but much simpler, because
only simple stroke-based designs are supported.

Nevertheless, the font does have different line widths, composed
glyphs, different stroke caps, etc. to make it good-looking.
Internally, Bezier curves are used to render the curves, but the font
generator does not support fully outlined designs needed for serifed
designs.  The output of the font renderer are not curves, but polygons
to be suitable for Hob3l's 2D engine.

Good-looking also means that the font supports some kerning.  With
good Unicode support, this gets exceedingly difficult, so an
algorithmic approach was chosen, which is surely suboptiomal (a manual
design is nicer for a given pair of glyphs), but it makes it feasible
to have a fully kerned font that still supports thousands of
characters, even algorithmically composed ones (say, which can kern
`T` plus `ae with ogonek`).

## Scripts

  * Latin

Currently, only the Latin script is supported.

## Languages

Language support is quite extensive for those languages using the
supported scripts.  Currently, MES-1 is fully supported (EU standard
subset of Unicode to support all European official languages), which
includes WGL-4, ISO-8859-1, ISO-8859-15, and US-ASCII.

The diacritic rendering is algorithmic, so that the font supports
languages that need characters not available as pre-composed Unicode
characters, like 'N' with diaeresis.

The font also supports some language variation, e.g. auto-ligating
'IJ' and 'ij' in Dutch so that tracking (inter-glyph space) works
properly, and Livonian 'd' is rendered with a comma below despite
using the Unicode for 'd with cedilla' (in the tradition of Latvian).
Marshallese is also supported by rendering cedillas as actual
cedillas.

## Multi-Line

No multi-line rendering is supported (no newlines are honoured), no
flushing, or any other text block algorithm is implemented, no
auto-line breaking algorihtm exists.

This is a restriction not of the font, but of Hob3l's rendering
library.

## Direction

Currently, only Left-to-Right rendering is really supported.
No BIDI algorithm is implemented.

This is a restriction not of the font, but of Hob3l's rendering
library.

## Slope Variants

  * Roman (normal)
  * Oblique

No readl italic style exists.

## Weight Variants

  * Book (normal)
  * Medium
  * Bold
  * Black
  * Light
