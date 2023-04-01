# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

TEST_TRIANGLE.scad := \
    scad-test/paper1.scad \
    scad-test/paper1-mx.scad \
    scad-test/paper1-r90.scad \
    scad-test/paper1-r30.scad \
    scad-test/test1b.scad \
    scad-test/test11.scad \
    scad-test/test11-mx.scad \
    scad-test/test11-r90.scad \
    scad-test/test11-r30.scad \
    scad-test/test12.scad \
    scad-test/test12-mx.scad \
    scad-test/test12-r90.scad \
    scad-test/test12-r30.scad \
    scad-test/test12b.scad \
    scad-test/test12b-mx.scad \
    scad-test/test12b-r90.scad \
    scad-test/test12b-r30.scad \
    scad-test/test12c-mx.scad \
    scad-test/test12c-r90.scad \
    scad-test/test12c-r30.scad \
    scad-test/test12d-mx.scad \
    scad-test/test12d-r90.scad \
    scad-test/test12d-r30.scad \
    scad-test/test13.scad \
    scad-test/test13-mx.scad \
    scad-test/test13-r90.scad \
    scad-test/test13-r30.scad \
    scad-test/test14.scad \
    scad-test/test14-mx.scad \
    scad-test/test14-r90.scad \
    scad-test/test14-r30.scad \
    scad-test/test15.scad \
    scad-test/test15-mx.scad \
    scad-test/test15-r90.scad \
    scad-test/test15-r30.scad \
    scad-test/test16.scad \
    scad-test/test16-mx.scad \
    scad-test/test16-r90.scad \
    scad-test/test16-r30.scad \
    scad-test/test17.scad \
    scad-test/test17-mx.scad \
    scad-test/test17-r90.scad \
    scad-test/test17-r30.scad \
    scad-test/test5b.scad \
    scad-test/test5c.scad \
    scad-test/test5d.scad \
    scad-test/test5e.scad \
    scad-test/test5f.scad \
    scad-test/test8.scad \
    scad-test/test10.scad \
    scad-test/test10b.scad \
    scad-test/test10b-r1.scad \
    scad-test/test10c.scad \
    scad-test/test10e.scad \
    scad-test/test10f.scad \
    scad-test/test10f-r1.scad \
    scad-test/test10g.scad \
    scad-test/test10g-r1.scad \
    scad-test/test10h.scad \
    scad-test/test20.scad \
    scad-test/test21.scad \
    scad-test/test22a.scad \
    scad-test/test22b.scad \
    scad-test/test22c.scad \
    scad-test/test22d.scad \
    scad-test/test22e.scad \
    scad-test/test22f.scad \
    scad-test/test22j.scad \
    scad-test/test22k.scad \
    scad-test/test22l.scad \
    scad-test/test22g.scad \
    scad-test/test22h.scad \
    scad-test/test22i.scad \
    scad-test/test23a.scad \
    scad-test/test23b.scad \
    scad-test/test23c.scad \
    scad-test/test23d.scad \
    scad-test/test23e.scad \
    scad-test/test23f.scad \
    scad-test/test23j.scad \
    scad-test/test23k.scad \
    scad-test/test23l.scad \
    scad-test/test23g.scad \
    scad-test/test23h.scad \
    scad-test/test23i.scad \
    scad-test/test24.scad \
    scad-test/test24a.scad \
    scad-test/test24b.scad \
    scad-test/test24c.scad \
    scad-test/test24d.scad \
    scad-test/test24e.scad \
    scad-test/test24f.scad \
    scad-test/test24g.scad \
    scad-test/test24h.scad \
    scad-test/test24i.scad \
    scad-test/test24j.scad \
    scad-test/test24k.scad \
    scad-test/test24l.scad \
    scad-test/test25a.scad \
    scad-test/test25b.scad \
    scad-test/test25c.scad \
    scad-test/test25d.scad \
    scad-test/test25e.scad \
    scad-test/test25f.scad \
    scad-test/test26a.scad \
    scad-test/test26b.scad \
    scad-test/test26c.scad \
    scad-test/test26d.scad \
    scad-test/test26e.scad \
    scad-test/test26f.scad \
    scad-test/test26g.scad \
    scad-test/test26h.scad \
    scad-test/test26i.scad \
    scad-test/test26j.scad \
    scad-test/test26k.scad \
    scad-test/test26l.scad \
    scad-test/test27a.scad \
    scad-test/test27b.scad \
    scad-test/test27c.scad \
    scad-test/test27d.scad \
    scad-test/test27e.scad \
    scad-test/test27f.scad \
    scad-test/test27g.scad \
    scad-test/test27h.scad \
    scad-test/test27i.scad \
    scad-test/test27j.scad \
    scad-test/test27k.scad \
    scad-test/test27l.scad \
    scad-test/test28a.scad \
    scad-test/test29a.scad \
    scad-test/test29b.scad \
    scad-test/test30a.scad \
    scad-test/test30a2.scad \
    scad-test/test30b.scad \
    scad-test/test30c.scad \
    scad-test/test30e.scad \
    scad-test/test17b.scad \
    scad-test/test30d.scad \
    scad-test/test30e.scad \
    scad-test/test30f.scad \
    scad-test/test31.scad \
    scad-test/uselessbox+body.scad \
    scad-test/test32f.scad \
    scad-test/test32b.scad \
    scad-test/test32e.scad \
    scad-test/test32.scad \
    scad-test/test32c.scad \
    scad-test/test32d.scad \
    scad-test/test35.scad \
    scad-test/test36c.scad \
    scad-test/test36e.scad \
    scad-test/test37.scad \
    scad-test/test37c.scad \
    scad-test/test39.scad \
    scad-test/test39b.scad \
    scad-test/test2.scad

test-out/test32.ps:  HOB3L_OPT := -gran=1
test-out/test32b.ps: HOB3L_OPT := -gran=0.5
test-out/test32c.ps: HOB3L_OPT := -gran=1
test-out/test32d.ps: HOB3L_OPT := -gran=0.5
test-out/test32e.ps: HOB3L_OPT := -gran=0.5
test-out/test32f.ps: HOB3L_OPT := -gran=0.5

test-out/test37c.js: HOB3L_OPT := -js-keep-ctxt

TEST_STL.scad := \
    scad-test/uselessbox+body.scad \
    scad-test/curry.scad \
    scad-test/test31b.scad \
    scad-test/test31c.scad \
    scad-test/linext1.scad \
    scad-test/linext1b.scad \
    scad-test/linext1c.scad \
    scad-test/linext1d.scad \
    scad-test/linext2.scad \
    scad-test/linext6.scad \
    scad-test/linext7.scad \
    scad-test/test2.scad \
    scad-test/paper1.scad \
    scad-test/test25a.scad \
    scad-test/test13.scad \
    scad-test/test14.scad \
    scad-test/test31d.scad \
    scad-test/test30a.scad \
    scad-test/test13c.scad \
    scad-test/test13c2.scad \
    scad-test/test13b.scad \
    scad-test/chain1.scad \
    scad-test/linext5.scad \
    scad-test/test34.scad \
    scad-test/test34b.scad \
    scad-test/test33b.scad \
    scad-test/test32.scad \
    scad-test/test32c.scad \
    scad-test/test32d.scad \
    scad-test/test35.scad \
    scad-test/test35b.scad \
    scad-test/test36a.scad \
    scad-test/test36b.scad \
    scad-test/test36c.scad \
    scad-test/test36d.scad \
    scad-test/test36e.scad \
    scad-test/test37.scad \
    scad-test/test37c.scad \
    scad-test/test39.scad \
    scad-test/test39b.scad \
    scad-test/test42a.scad \
    scad-test/test42b.scad

# The following tests currently fail:
#   - test43  triggers another orientation bug due to rounding in the bool algo
#   - test43b the same (test43 is the smaller file for debugging)
#   - test37b a non-3-manifold polyhedron currently assert-fails
#   - test38  2D hull() operation is not yet implemented

FAIL_TRIANGLE.scad := \
    scad-test/test43.scad \
    scad-test/test43b.scad \
    scad-test/test37b.scad \
    scad-test/test38.scad

FAIL_STL.scad := \
    scad-test/test43.scad \
    scad-test/test43b.scad \
    scad-test/test37b.scad \
    scad-test/test38.scad

FAIL_JS.scad :=

# The following STLs need hacks, because they are not 2-manifold, although
# slic3r does not complain.  Do I miss something?
#   scad-test/ergo.scad
#   scad-test/ergo1.scad
#   scad-test/nozzle.scad
