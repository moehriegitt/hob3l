# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

HOB3LOP_TEST_GON := \
    corner17 \
    err_test28a \
    err_test20 \
    trifail1 \
    paper1 \
    bend1 \
    order1 \
    order2 \
    order3 \
    collapse1 \
    collapse2 \
    close1 \
    cross1 \
    vert1 \
    all1 \
    over1 \
    over1a \
    over2 \
    over3 \
    over4 \
    over5 \
    over6 \
    over7 \
    over8 \
    over9 \
    over10 \
    over11 \
    touch1 \
    touch2 \
    round1 \
    round2 \
    round3 \
    star2 \
    star2b \
    star1 \
    star3 \
    star4 \
    collapse3 \
    collapse3b \
    collapse4 \
    vert2

DEP.hob3lop-test.o := \
    src/hob3lop/hob3lop-test.c \
    out/src/hob3lop/hob3lop-test-1.inc \
    out/src/hob3lop/hob3lop-test-2.inc \
    out/src/hob3lop/useless_box.inc \
    out/src/hob3lop/test1.inc \
    out/src/hob3lop/test2.inc \
    out/src/hob3lop/corner1.inc \
    out/src/hob3lop/corner1b.inc \
    out/src/hob3lop/corner2.inc \
    $(addsuffix .inc, $(addprefix out/src/hob3lop/, $(HOB3LOP_TEST_GON)))

out/bin/hob3lop/hob3lop-test.o: \
    $(DEP.hob3lop-test.o)

######################################################################

STL2C := $(srcdir)/script/stl2c

out/src/hob3lop/hob3lop-test-1.inc: hob3lop-test.mk
	for i in $(HOB3LOP_TEST_GON); do echo '#include "'"$$i"'.inc"'; done > $@.new
	mv $@.new $@

out/src/hob3lop/hob3lop-test-2.inc: hob3lop-test.mk
	for i in $(HOB3LOP_TEST_GON); do echo "TEST_TRIANGLIFY_GON($$i);"; done > $@.new
	mv $@.new $@

out/src/hob3lop/corner2.inc: test/hob3lop/corner2.stl

out/test/hob3lop/corner2.stl: test/hob3lop/corner2.scad
	openscad $< -o $@

out/src/%.inc: test/%.stl $(STL2C)
	$(STL2C) $< > $@.new
	mv $@.new $@

out/src/%.inc: test/%.fig $(STL2C)
	$(STL2C) $< > $@.new
	mv $@.new $@

out/test/%.csg: test/%.scad
	openscad $< -o $@

out/src/%.inc: test/%.csg3 $(STL2C)
	$(STL2C) $< > $@.new
	mv $@.new $@

# This is a recursive dependency: we cannot have hob3l before we test its internals.
#out/test/%.csg3: test/hob3lop/%.csg
#	hob3l --dump-csg3 $< > $@.new
#	mv $@.new $@
