# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

package_name := hob3l
package_version := test

WITH_FONT :=

######################################################################

HOB3L := ./hob3l.exe
HOB3L_JS_COPY_AUX := pkgdatadir=./share sh ./script/hob3l-js-copy-aux.in

######################################################################

TEST_TRIANGLE.png := \
    $(addprefix out/hob3l/test/,$(notdir $(TEST_TRIANGLE.scad:.scad=.png)))

TEST_STL.stl := \
    $(addprefix out/hob3l/test/,$(notdir $(TEST_STL.scad:.scad=.stl)))

TEST_STL.jsgz := \
    $(addprefix out/hob3l/test/,$(notdir $(TEST_STL.scad:.scad=.js.gz)))

FAIL_TRIANGLE := \
    $(addprefix out/hob3l/test/fail-,$(notdir $(FAIL_TRIANGLE.scad:.scad=.ps)))

FAIL_STL := \
    $(addprefix out/hob3l/test/fail-,$(notdir $(FAIL_STL.scad:.scad=.stl)))

FAIL_JS := \
    $(addprefix out/hob3l/test/fail-,$(notdir $(FAIL_JS.scad:.scad=.js)))

######################################################################
# header files

H_HOB3L := $(notdir $(wildcard include/hob3l/*.h))

######################################################################
# data files

SHARE_DATA := \
    gl-matrix/common.js \
    gl-matrix/mat4.js \
    gl-matrix/vec3.js \
    js-hob3l.local.html

######################################################################

# Main Functionality:
# libhob3l.a:
MOD_C.libhob3l.a := \
    syn.c \
    syn-2scad.c \
    syn-msg.c \
    stl-parse.c \
    vec3-dict.c \
    scad.c \
    scad-2scad.c \
    csg3.c \
    csg3-2scad.c \
    csg2.c \
    csg2-tree.c \
    csg2-layer.c \
    csg2-triangle.c \
    csg2-bool.c \
    csg2-hull.c \
    csg2-2scad.c \
    csg2-2stl.c \
    csg2-2js.c \
    csg2-2ps.c \
    ps.c \
    gc.c

ifeq ($(WITH_FONT),1)
MOD_C.libhob3l.a += \
    font.c
endif

MOD_O.libhob3l.a := $(addprefix out/,$(MOD_C.libhob3l.a:.c=.o))
MOD_D.libhob3l.a := $(addprefix out/,$(MOD_C.libhob3l.a:.c=.d))

######################################################################

# Font library:
# libhob3lfont.a:
MOD_C.libhob3lfont.a := \
    font-nozzl3_sans.c \
    font-nozzl3_sans_black.c \
    font-nozzl3_sans_black_oblique.c \
    font-nozzl3_sans_bold.c \
    font-nozzl3_sans_bold_oblique.c \
    font-nozzl3_sans_light.c \
    font-nozzl3_sans_light_oblique.c \
    font-nozzl3_sans_medium.c \
    font-nozzl3_sans_medium_oblique.c \
    font-nozzl3_sans_oblique.c

MOD_O.libhob3lfont.a := $(addprefix out/,$(MOD_C.libhob3lfont.a:.c=.o))
MOD_D.libhob3lfont.a := $(addprefix out/,$(MOD_C.libhob3lfont.a:.c=.d))

# Command Line Executable:
# hob3l.exe:
MOD_C.hob3l.exe := \
    main.c

MOD_O.hob3l.exe := $(addprefix out/,$(MOD_C.hob3l.exe:.c=.o))
MOD_D.hob3l.exe := $(addprefix out/,$(MOD_C.hob3l.exe:.c=.d))

# Font generator:
# fontgen.exe:
MOD_C.fontgen.exe := \
    fontgen.c

MOD_O.fontgen.exe := $(addprefix out/,$(MOD_C.fontgen.exe:.c=.o))
MOD_D.fontgen.exe := $(addprefix out/,$(MOD_C.fontgen.exe:.c=.d))

######################################################################

_ := $(shell mkdir -p out)
_ := $(shell mkdir -p test-out)

-include out/*.d

.SECONDARY:

.SUFFIXES:

all:

bin: \
    hob3l.exe \
    out/hob3l-js-copy-aux

LIB.hob3l.exe := \
    hob3l \
    hob3lbase

ifeq ($(WITH_FONT),1)
LIB.hob3l.exe += \
    hob3lfont
endif

LIB_A.hob3l.exe := $(addsuffix $(_LIB), $(addprefix $(LIB_), $(LIB.hob3l.exe)))
LIB_L.hob3l.exe := $(addprefix -l, $(LIB.hob3l.exe))

lib: $(LIB_A.hob3l.exe)

data: \
    $(addprefix out/,$(OUT_DATA))

maintainer-clean: zap

.PHONY: sweep
sweep:
	rm -f *~
	rm -f *.bak
	rm -f src/*~
	rm -f src/*.bak
	rm -f font/*~
	rm -f font/*.bak
	rm -f include/*~
	rm -f include/*.bak
	rm -f include/hob3lbase/*~
	rm -f include/hob3lbase/*.bak
	rm -f include/hob3l/*~
	rm -f include/hob3l/*.bak

.PHONY: font-clean
font-clean: clean
	rm -f src/font-nozzl3*.c

.PHONY: zap
zap: sweep clean font-clean
	rm -f .mode.d

.PHONY: distclean
distclean: sweep clean

.PHONY: clean-test
clean-test:
	rm -rf test-out

clean: clean-test
	rm -rf out
	rm -rf out-font
	rm -rf html
	rm -f *.o
	rm -f *.d
	rm -f *.i
	rm -f *.a
	rm -f *.x
	rm -f *.exe
	rm -f core
	rm -f src/core
	rm -f font/core

libhob3l.a: $(MOD_O.libhob3l.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

libhob3lbase.a: $(MOD_O.libhob3lbase.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

libhob3lfont.a: $(MOD_O.libhob3lfont.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

fontgen.exe: $(MOD_O.fontgen.exe) libhob3lbase.a libhob3l.a
	$(CC) -o $@ $(MOD_O.fontgen.exe) -Lout/bin -lhob3l -lhob3lbase $(LIBS) -lm $(CFLAGS)

out/%: script/%.in
	sed 's_@pkgdatadir@_$(pkgdatadir)_g' $< > $@.new
	mv $@.new $@

src/mat_gen_ext.c: $(srcdir)/script/mkmat
	$(srcdir)/script/mkmat

src/mat_is_rot.c: $(srcdir)/script/mkrotmat
	$(srcdir)/script/mkrotmat > $@.new
	mv $@.new $@

out/math-test.o: CFLAGS+=-Wno-float-equal

out/%.o: src/%.c src/mat_gen_ext.c
	$(CC) -MMD -MP -MT $@ -MF out/$*.d -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

out/%.o: font/%.c
	$(CC) -MMD -MP -MT $@ -MF out/$*.d -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

%.i: %.c
	$(CC) -MMD -MP -MT $@ -MF out/$*.d -E $< $(CPPFLAGS) $(CFLAGS) > $@.new
	mv $@.new $@

out/main.o: src/main.c src/opt.inc

%.inc: %.switch $(srcdir)/script/mkswitch
	$(srcdir)/script/mkswitch $< > $@.new
	mv $@.new $@

.PHONY: test
test: unit-test no-unit-test

.PHONY: no-unit-test
no-unit-test: test-triangle test-triangle-prepare test-stl test-js

.PHONY: fail
fail: fail-triangle fail-stl fail-js

.PHONY: test-triangle
test-triangle: $(TEST_TRIANGLE.png)

.PHONY: test-stl
test-stl: $(TEST_STL.stl)

.PHONY: test-js
test-js: $(TEST_STL.jsgz)


.PHONY: fail-triangle
fail-triangle: $(FAIL_TRIANGLE)

.PHONY: fail-stl
fail-stl: $(FAIL_STL)

.PHONY: fail-js
fail-js: $(FAIL_JS)


.PHONY: test-jsgz
test-jsgz: test-js

.PHONY: test-triangle-prepare
test-triangle-prepare: $(TEST_TRIANGLE.scad)

out/hob3l/test/%.png: out/hob3l/test/%.ps
	gm convert -border 10x10 -bordercolor '#ffffff' -density 144x144 $< $@.new.png
	mv $@.new.png $@

out/hob3l/test/%.ps: test/hob3l/%.scad hob3l.exe
	$(HOB3L) $< -z=2.0 $(HOB3L_OPT) -o $@.new.ps
	mv $@.new.ps $@

out/hob3l/test/fail-%.ps: test/hob3l/%.scad hob3l.exe
	! $(MAKE) out/hob3l/test/$*.ps
	echo >| $@

out/hob3l/test/%.stl: test/hob3l/%.scad hob3l.exe
	$(HOB3L) $< -o $@.new.stl
	mv $@.new.stl $@

out/hob3l/test/fail-%.stl: test/hob3l/%.scad hob3l.exe
	! $(MAKE) out/hob3l/test/$*.stl
	echo >| $@

out/hob3l/test/%.stl: $(SCAD_DIR)/%.scad hob3l.exe
	openscad $< -o $@.new.csg
	$(HOB3L) $@.new.csg -o $@.new.stl
	mv $@.new.stl $@
	rm -f $@.new.csg

out/hob3l/test/%.js: test/hob3l/%.scad hob3l.exe
	$(HOB3L) $< $(HOB3L_OPT) -o $@.new.js
	cat $(wildcard $<.js) $@.new.js > $@.new2.js
	mv $@.new2.js $@
	rm $@.new.js

out/hob3l/test/fail-%.js: test/hob3l/%.scad hob3l.exe
	! $(HOB3L) $< -o $@.new.js
	echo >| $@

out/hob3l/test/%.js: $(SCAD_DIR)/%.scad hob3l.exe
	openscad $< -o $@.new.csg
	$(HOB3L) $@.new.csg -o $@.new.js
	mv $@.new.js $@
	rm -f $@.new.csg

out/hob3l/test/%.js.gz: out/hob3l/test/%.js
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

.PHONY: update-header
update-header: script/xproto
	$(srcdir)/script/xproto \
	    $(srcdir)/src/*.c \
	    $(srcdir)/src/*.h \
	    $(srcdir)/include/*/*.h
	$(srcdir)/script/mkmacro \
	    $(srcdir)/src/*.c \
	    $(srcdir)/src/*.h \
	    $(srcdir)/include/*/*.h

.PHONY: update-toc
update-toc: README.md script/mktoc
	$(srcdir)/script/mktoc -skip1 -in-place README.md
	$(srcdir)/script/mktoc -skip1 -in-place doc/scadformat.md

.PHONY: markdown
markdown: \
    html/README.html \
    html/doc/scadformat.html \
    html/doc/jsformat.html \

html/%.html: %.md
	mkdir -p ./$(dir $@)
	markdown $< > $@.new
	mv $@.new $@

SCAD_SCAD := $(wildcard $(SCAD_DIR)/*.scad)

.PHONY: test-scad
test-scad: $(addprefix out/hob3l/test/,$(notdir $(SCAD_SCAD:.scad=.stl)))

.PHONY: speed-test
speed-test:
	rm -f .mode.d.old && mv .mode.d .mode.d.old
	$(MAKE) clean MODE=release
	$(MAKE) -j
	$(MAKE) -j test
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/curry.scad -o out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/curry.scad -o out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/uselessbox+body.scad -o out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) test/hob3l/uselessbox+body.scad -o out.stl'
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

######################################################################

PS2PDF := ps2pdf

.PHONY: font
font: out-font/.stamp

.PHONY: font-doc
font-doc: font
	cd out-font && pdflatex nozzl3_sans-coverage.tex < /dev/null
	cd out-font && pdflatex nozzl3_sans-coverage.tex < /dev/null
	cd out-font && $(PS2PDF) nozzl3_sans-family.ps nozzl3_sans-family.pdf

out-font/.stamp: fontgen.exe
	mkdir -p out-font
	./fontgen.exe

######################################################################
# installation, the usual ceremony.

installdirs-bin:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(bindir)

installdirs-data:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(pkgdatadir)
	$(MKINSTALLDIR) $(DESTDIR)$(pkgdatadir)/gl-matrix

installdirs-lib:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(libdir)

installdirs-include:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(includedir)/hob3lbase
	$(MKINSTALLDIR) $(DESTDIR)$(includedir)/hob3l

install-bin: installdirs-bin
	$(NORMAL_INSTALL)
	$(INSTALL_BIN) \
	    hob3l.exe \
	    $(DESTDIR)$(bindir)/hob3l$(_EXE)
	$(INSTALL_SCRIPT) \
	    out/hob3l-js-copy-aux \
	    $(DESTDIR)$(bindir)/hob3l-js-copy-aux

install-data: installdirs-data
	$(NORMAL_INSTALL)
	for F in $(SHARE_DATA); do \
	    $(INSTALL_DATA) \
	        share/$$F \
	        $(DESTDIR)$(pkgdatadir)/$$F || exit 1; \
	done

install-font:

ifeq ($(WITH_FONT),1)
install-font: installdirs-lib
	$(NORMAL_INSTALL)
	$(INSTALL_DATA) libhob3lfont.a $(DESTDIR)$(libdir)/$(LIB_)hob3lfont$(_LIB)
endif

install-lib: installdirs-lib
	$(NORMAL_INSTALL)
	$(INSTALL_DATA) libhob3lbase.a $(DESTDIR)$(libdir)/$(LIB_)hob3lbase$(_LIB)
	$(INSTALL_DATA) libhob3l.a $(DESTDIR)$(libdir)/$(LIB_)hob3l$(_LIB)

install-include: installdirs-include
	$(NORMAL_INSTALL)
	for H in $(H_HOB3L); do \
	    $(INSTALL_DATA) \
	        include/hob3l/$$H \
	        $(DESTDIR)$(includedir)/hob3l/$$H || exit 1; \
	done

uninstall:
	$(NORMAL_UNINSTALL)
	$(UNINSTALL) $(DESTDIR)$(bindir)/hob3l$(EXE)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3lfont$(_LIB)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3lbase$(_LIB)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3l$(_LIB)
	for H in $(H_HOB3L); do \
	    $(UNINSTALL) $(DESTDIR)$(includedir)/hob3l/$$H || exit 1; \
	done
	for F in $(SHARE_DATA); do \
	    $(UNINSTALL) $(DESTDIR)$(pkgdatadir)/$$F || exit 1; \
	done
	$(UNINSTALL_DIR) $(DESTDIR)$(includedir)/hob3lbase
	$(UNINSTALL_DIR) $(DESTDIR)$(includedir)/hob3l
	$(UNINSTALL_DIR) $(DESTDIR)$(pkgdatadir)/gl-matrix
	$(UNINSTALL_DIR) $(DESTDIR)$(pkgdatadir)

# check installation by running 'test' with installed binary
check: clean-test
	$(MAKE) \
	    HOB3L=$(DESTDIR)$(bindir)/hob3l$(_EXE) \
	    HOB3L_JS_COPY_AUX=$(DESTDIR)$(bindir)/hob3l-js-copy-aux \
	    no-unit-test
