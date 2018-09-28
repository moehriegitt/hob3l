# eins
Fast Slicing of SCAD Files for 3D Printing

## Replace 3D CSG by Fast 2D Polygon Clipping

Preparing a 3D model in CSG format (e.g., when using OpenSCAD) for
printing may take a long time and is computationally instable.

So this tool wants to replace this workflow:
![3D CSG](img/csg1-old.png)

by this workflow:

![2D CSG](img/csg1-new.png)

In the hope that the latter is faster.  First experiments indeed
indicate a speed-up of 200 for a non-trivial example, and it gains
much better computational stability.
