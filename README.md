# csg2stp
convert openscad design to step brep

Use my fork of OCC-CSG:
https://github.com/gega/OCC-CSG.git
branch: sweep

This fork has the necessary changes to support the complex conversions:

- sweep command for rotate_extrude
- random rotation for rotationally symmetric primitives
- brep analysis for csg operation to prevent crashes

You also need to have OpenSCAD installed and available in your path.
