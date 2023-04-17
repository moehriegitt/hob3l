# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

# BUGS:
#
# #
# Still produces a broken polyhedron:
# 3dprint/dowelling90: hob3l.x -o dowelling90+jig-b.stl dowelling90+jig.scad
# (Slic3r complains about ~1000 errors and one Layer (42.0 in slic3r) is
# inverted.)

TEST_TRIANGLE.scad := \
    test/hob3l/corner17.scad \
    test/hob3l/paper1.scad \
    test/hob3l/paper1-mx.scad \
    test/hob3l/paper1-r90.scad \
    test/hob3l/paper1-r30.scad \
    test/hob3l/test1b.scad \
    test/hob3l/test11.scad \
    test/hob3l/test11-mx.scad \
    test/hob3l/test11-r90.scad \
    test/hob3l/test11-r30.scad \
    test/hob3l/test12.scad \
    test/hob3l/test12-mx.scad \
    test/hob3l/test12-r90.scad \
    test/hob3l/test12-r30.scad \
    test/hob3l/test12b.scad \
    test/hob3l/test12b-mx.scad \
    test/hob3l/test12b-r90.scad \
    test/hob3l/test12b-r30.scad \
    test/hob3l/test12c-mx.scad \
    test/hob3l/test12c-r90.scad \
    test/hob3l/test12c-r30.scad \
    test/hob3l/test12d-mx.scad \
    test/hob3l/test12d-r90.scad \
    test/hob3l/test12d-r30.scad \
    test/hob3l/test13.scad \
    test/hob3l/test13-mx.scad \
    test/hob3l/test13-r90.scad \
    test/hob3l/test13-r30.scad \
    test/hob3l/test14.scad \
    test/hob3l/test14-mx.scad \
    test/hob3l/test14-r90.scad \
    test/hob3l/test14-r30.scad \
    test/hob3l/test15.scad \
    test/hob3l/test15-mx.scad \
    test/hob3l/test15-r90.scad \
    test/hob3l/test15-r30.scad \
    test/hob3l/test16.scad \
    test/hob3l/test16-mx.scad \
    test/hob3l/test16-r90.scad \
    test/hob3l/test16-r30.scad \
    test/hob3l/test17.scad \
    test/hob3l/test17-mx.scad \
    test/hob3l/test17-r90.scad \
    test/hob3l/test17-r30.scad \
    test/hob3l/test5b.scad \
    test/hob3l/test5c.scad \
    test/hob3l/test5d.scad \
    test/hob3l/test5e.scad \
    test/hob3l/test5f.scad \
    test/hob3l/test8.scad \
    test/hob3l/test10.scad \
    test/hob3l/test10b.scad \
    test/hob3l/test10b-r1.scad \
    test/hob3l/test10c.scad \
    test/hob3l/test10e.scad \
    test/hob3l/test10f.scad \
    test/hob3l/test10f-r1.scad \
    test/hob3l/test10g.scad \
    test/hob3l/test10g-r1.scad \
    test/hob3l/test10h.scad \
    test/hob3l/test20.scad \
    test/hob3l/test21.scad \
    test/hob3l/test22a.scad \
    test/hob3l/test22b.scad \
    test/hob3l/test22c.scad \
    test/hob3l/test22d.scad \
    test/hob3l/test22e.scad \
    test/hob3l/test22f.scad \
    test/hob3l/test22j.scad \
    test/hob3l/test22k.scad \
    test/hob3l/test22l.scad \
    test/hob3l/test22g.scad \
    test/hob3l/test22h.scad \
    test/hob3l/test22i.scad \
    test/hob3l/test23a.scad \
    test/hob3l/test23b.scad \
    test/hob3l/test23c.scad \
    test/hob3l/test23d.scad \
    test/hob3l/test23e.scad \
    test/hob3l/test23f.scad \
    test/hob3l/test23j.scad \
    test/hob3l/test23k.scad \
    test/hob3l/test23l.scad \
    test/hob3l/test23g.scad \
    test/hob3l/test23h.scad \
    test/hob3l/test23i.scad \
    test/hob3l/test24.scad \
    test/hob3l/test24a.scad \
    test/hob3l/test24b.scad \
    test/hob3l/test24c.scad \
    test/hob3l/test24d.scad \
    test/hob3l/test24e.scad \
    test/hob3l/test24f.scad \
    test/hob3l/test24g.scad \
    test/hob3l/test24h.scad \
    test/hob3l/test24i.scad \
    test/hob3l/test24j.scad \
    test/hob3l/test24k.scad \
    test/hob3l/test24l.scad \
    test/hob3l/test25a.scad \
    test/hob3l/test25b.scad \
    test/hob3l/test25c.scad \
    test/hob3l/test25d.scad \
    test/hob3l/test25e.scad \
    test/hob3l/test25f.scad \
    test/hob3l/test26a.scad \
    test/hob3l/test26b.scad \
    test/hob3l/test26c.scad \
    test/hob3l/test26d.scad \
    test/hob3l/test26e.scad \
    test/hob3l/test26f.scad \
    test/hob3l/test26g.scad \
    test/hob3l/test26h.scad \
    test/hob3l/test26i.scad \
    test/hob3l/test26j.scad \
    test/hob3l/test26k.scad \
    test/hob3l/test26l.scad \
    test/hob3l/test27a.scad \
    test/hob3l/test27b.scad \
    test/hob3l/test27c.scad \
    test/hob3l/test27d.scad \
    test/hob3l/test27e.scad \
    test/hob3l/test27f.scad \
    test/hob3l/test27g.scad \
    test/hob3l/test27h.scad \
    test/hob3l/test27i.scad \
    test/hob3l/test27j.scad \
    test/hob3l/test27k.scad \
    test/hob3l/test27l.scad \
    test/hob3l/test28a.scad \
    test/hob3l/test29a.scad \
    test/hob3l/test29b.scad \
    test/hob3l/test30a.scad \
    test/hob3l/test30a2.scad \
    test/hob3l/test30b.scad \
    test/hob3l/test30c.scad \
    test/hob3l/test30e.scad \
    test/hob3l/test17b.scad \
    test/hob3l/test30d.scad \
    test/hob3l/test30e.scad \
    test/hob3l/test30f.scad \
    test/hob3l/test31.scad \
    test/hob3l/uselessbox+body.scad \
    test/hob3l/test32f.scad \
    test/hob3l/test32b.scad \
    test/hob3l/test32e.scad \
    test/hob3l/test32.scad \
    test/hob3l/test32c.scad \
    test/hob3l/test32d.scad \
    test/hob3l/test35.scad \
    test/hob3l/test36c.scad \
    test/hob3l/test36e.scad \
    test/hob3l/test37.scad \
    test/hob3l/test37c.scad \
    test/hob3l/test39.scad \
    test/hob3l/test39b.scad \
    test/hob3l/test37b.scad \
    test/hob3l/test38.scad \
    test/hob3l/test2.scad \
    test/hob3l/ergo.scad

HOB3L_OPT += --path

out/test/hob3l/test32.ps:  HOB3L_OPT += -gran=1
out/test/hob3l/test32b.ps: HOB3L_OPT += -gran=0.5
out/test/hob3l/test32c.ps: HOB3L_OPT += -gran=1
out/test/hob3l/test32d.ps: HOB3L_OPT += -gran=0.5
out/test/hob3l/test32e.ps: HOB3L_OPT += -gran=0.5
out/test/hob3l/test32f.ps: HOB3L_OPT += -gran=0.5

out/test/hob3l/test37c.js: HOB3L_OPT += -js-keep-ctxt

TEST_STL.scad := \
    test/hob3l/large1.scad \
    test/hob3l/corner17.scad \
    test/hob3l/uselessbox+body.scad \
    test/hob3l/curry.scad \
    test/hob3l/test31b.scad \
    test/hob3l/test31c.scad \
    test/hob3l/linext1.scad \
    test/hob3l/linext1b.scad \
    test/hob3l/linext1c.scad \
    test/hob3l/linext1d.scad \
    test/hob3l/linext2.scad \
    test/hob3l/linext6.scad \
    test/hob3l/linext7.scad \
    test/hob3l/test2.scad \
    test/hob3l/paper1.scad \
    test/hob3l/test25a.scad \
    test/hob3l/test13.scad \
    test/hob3l/test14.scad \
    test/hob3l/test31d.scad \
    test/hob3l/test30a.scad \
    test/hob3l/test13c.scad \
    test/hob3l/test13c2.scad \
    test/hob3l/test13b.scad \
    test/hob3l/chain1.scad \
    test/hob3l/linext5.scad \
    test/hob3l/test34.scad \
    test/hob3l/test34b.scad \
    test/hob3l/test33b.scad \
    test/hob3l/test32.scad \
    test/hob3l/test32c.scad \
    test/hob3l/test32d.scad \
    test/hob3l/test35.scad \
    test/hob3l/test35b.scad \
    test/hob3l/test36a.scad \
    test/hob3l/test36b.scad \
    test/hob3l/test36c.scad \
    test/hob3l/test36d.scad \
    test/hob3l/test36e.scad \
    test/hob3l/test37.scad \
    test/hob3l/test37c.scad \
    test/hob3l/test39.scad \
    test/hob3l/test39b.scad \
    test/hob3l/test42a.scad \
    test/hob3l/test42b.scad \
    test/hob3l/test37b.scad \
    test/hob3l/test38.scad \
    test/hob3l/ergo.scad

# The following tests currently fail:
#   - test43  (FONT) triggers another orientation bug due to rounding in the bool algo
#   - test43b (FONT) the same (test43 is the smaller file for debugging)
#   - test37b a non-2-manifold polyhedron currently assert-fails

FAIL_TRIANGLE.scad := \
    test/hob3l/test43.scad \
    test/hob3l/test43b.scad \

FAIL_STL.scad := \
    test/hob3l/test43.scad \
    test/hob3l/test43b.scad \

FAIL_JS.scad :=

# The following STLs need hacks, because they are not 2-manifold, although
# slic3r does not complain.  Do I miss something?
#   test/hob3l/ergo1.scad
#   test/hob3l/nozzle.scad

######################################################################

TEST_TRIANGLE.png := \
    $(addprefix out/test/hob3l/,$(notdir $(TEST_TRIANGLE.scad:.scad=.png)))

TEST_STL.stl := \
    $(addprefix out/test/hob3l/,$(notdir $(TEST_STL.scad:.scad=.stl)))

TEST_STL.jsgz := \
    $(addprefix out/test/hob3l/,$(notdir $(TEST_STL.scad:.scad=.js.gz)))

FAIL_TRIANGLE := \
    $(addprefix out/test/hob3l/fail-,$(notdir $(FAIL_TRIANGLE.scad:.scad=.ps)))

FAIL_STL := \
    $(addprefix out/test/hob3l/fail-,$(notdir $(FAIL_STL.scad:.scad=.stl)))

FAIL_JS := \
    $(addprefix out/test/hob3l/fail-,$(notdir $(FAIL_JS.scad:.scad=.js)))

######################################################################

no-unit-test: no-unit-test-hob3l

.PHONY: test-hob3l
test-hob3l: no-unit-test-hob3l

.PHONY: test-main
test-main: no-unit-test-hob3l

no-unit-test-hob3l: \
    test-hob3l-triangle \
    test-hob3l-triangle-prepare \
    test-hob3l-stl \
    test-hob3l-js

fail: fail-hob3l
fail-hob3l: \
    fail-hob3l-triangle \
    fail-hob3l-stl \
    fail-hob3l-js

.PHONY: test-hob3l-triangle
test-hob3l-triangle: $(TEST_TRIANGLE.png)

.PHONY: test-hob3l-stl
test-hob3l-stl: $(TEST_STL.stl)

.PHONY: test-hob3l-js
test-hob3l-js: $(TEST_STL.jsgz)


.PHONY: fail-hob3l-triangle
fail-hob3l-triangle: $(FAIL_TRIANGLE)

.PHONY: fail-hob3l-stl
fail-hob3l-stl: $(FAIL_STL)

.PHONY: fail-hob3l-js
fail-hob3l-js: $(FAIL_JS)


.PHONY: test-hob3l-jsgz
test-hob3l-jsgz: test-hob3l-js

.PHONY: test-hob3l-triangle-prepare
test-hob3l-triangle-prepare: $(TEST_TRIANGLE.scad)

out/test/hob3l/%.png: out/test/hob3l/%.ps
	gm convert -border 10x10 -bordercolor '#ffffff' -density 144x144 $< $@.new.png
	mv $@.new.png $@

out/test/hob3l/%.ps: test/hob3l/%.scad hob3l.x
	$(HOB3L) $< -z=2.0 $(HOB3L_OPT) -o $@.new.ps
	mv $@.new.ps $@

out/test/hob3l/fail-%.ps: test/hob3l/%.scad hob3l.x
	! $(MAKE) out/test/hob3l/$*.ps
	echo >| $@

out/test/hob3l/%.stl: test/hob3l/%.scad hob3l.x
	$(HOB3L) $< -o $@.new.stl
	mv $@.new.stl $@

out/test/hob3l/fail-%.stl: test/hob3l/%.scad hob3l.x
	! $(MAKE) out/test/hob3l/$*.stl
	echo >| $@

out/test/hob3l/%.stl: $(SCAD_DIR)/%.scad hob3l.x
	openscad $< -o $@.new.csg
	$(HOB3L) $@.new.csg -o $@.new.stl
	mv $@.new.stl $@
	rm -f $@.new.csg

out/test/hob3l/%.js: test/hob3l/%.scad hob3l.x
	$(HOB3L) $< $(HOB3L_OPT) -o $@.new.js
	cat $(wildcard $<.js) $@.new.js > $@.new2.js
	mv $@.new2.js $@
	rm $@.new.js

out/test/hob3l/fail-%.js: test/hob3l/%.scad hob3l.x
	! $(HOB3L) $< -o $@.new.js
	echo >| $@

out/test/hob3l/%.js: $(SCAD_DIR)/%.scad hob3l.x
	openscad $< -o $@.new.csg
	$(HOB3L) $@.new.csg -o $@.new.js
	mv $@.new.js $@
	rm -f $@.new.csg

out/test/hob3l/%.js.gz: out/test/hob3l/%.js
	$(HOB3L_JS_COPY_AUX) $(@:.gz=)

test/hob3l/%.scad: test/hob3l/%.fig $(srcdir)/script/fig2scad
	$(srcdir)/script/fig2scad $< > $@.new
	mv $@.new $@

test/hob3l/%-mx.scad: test/hob3l/%.fig $(srcdir)/script/fig2scad
	$(srcdir)/script/fig2scad --mirror=x $< > $@.new
	mv $@.new $@

test/hob3l/%-r90.scad: test/hob3l/%.fig $(srcdir)/script/fig2scad
	$(srcdir)/script/fig2scad --rotate=90 $< > $@.new
	mv $@.new $@

test/hob3l/%-r30.scad: test/hob3l/%.fig $(srcdir)/script/fig2scad
	$(srcdir)/script/fig2scad --rotate=30 $< > $@.new
	mv $@.new $@

SCAD_SCAD := $(wildcard $(SCAD_DIR)/*.scad)

.PHONY: test-hob3l-scad
test-hob3l-scad: $(addprefix out/test/hob3l/,$(notdir $(SCAD_SCAD:.scad=.stl)))

speed-test: speed-test-hob3l
speed-test-hob3l:
	rm -f .mode.d.old && mv .mode.d .mode.d.old
	$(MAKE) clean MODE=release
	$(MAKE) -j
	$(MAKE) -j test
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/curry.scad -o out/speed/out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/curry.scad -o out/speed/out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/uselessbox+body.scad -o out/speed/out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/uselessbox+body.scad -o out/speed/out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/uselessbox+body.scad'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/uselessbox+body.scad'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/test31b.scad'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/test31b.scad'
	rm -f .mode.d && mv .mode.d.old .mode.d
	$(MAKE) clean

# check installation by running 'test' with installed binary
check: clean-test
	$(MAKE) \
	    HOB3L=$(DESTDIR)$(bindir)/hob3l$(_EXE) \
	    HOB3L_JS_COPY_AUX=$(DESTDIR)$(bindir)/hob3l-js-copy-aux \
	    no-unit-test
