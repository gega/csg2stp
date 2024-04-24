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

## Manual Operation

1. compile and install OCC-CSG (use my fork and the sweep branch)

    ```git clone https://github.com/gega/OCC-CSG.git ; cd OCC-CSG ; git checkout sweep ; mkdir build ; cd build ; cmake .. ; make ; sudo make install```

3. build these utilities with cloning this repo and make

    ```git clone git@github.com:gega/csg2stp.git ; cd csg2stp ; make```

4. open your design in OpenSCAD, render and export as CSG

    ```openscad -o model.csg model.scad```

5. pipe the csg to csg2xml and redirect the stdout to an xml file

    ```./csg2xml <model.csg >model.xml```

6. use xml2mak to generate a makefile

    ```./xml2mak model.xml model.stp >model.mak```

7. use make -f with the generated makefile

    ```make -f model.mak```

8. model.stp is generated, check if valid and complete, if not, go to step 7 and try again

## Fine Print

**NOTE:** The following OpenSCAD commands are not supported:
- hull
- import
- projection
- minkowski

**WARNING:** The conversion is not deterministic, for some models it needs to be converted several times in order to get the proper results!
