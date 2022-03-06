# -*- Mode: Makefile -*-
# Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file

package_name := hob3l
package_version := test

WITH_FONT :=

######################################################################

SHELL := /bin/sh

-include .mode.d

# default is 'just do it' mode, with some runtime checks
OPT := 2
SANITIZE := 0
WERROR := 0
NDEBUG := 0
FRAME := 0
PSTRACE := 0

# release mode: strict compilation, no sanitizing, no debug, no PS trace
ifeq ($(MODE), release)
OPT := 3
SANITIZE := 0
WERROR := 1
NDEBUG := 1
FRAME := 0
PSTRACE := 0
endif

# devel mode: strict compilation, full set of diagnostics and checks, debuggable
ifeq ($(MODE), devel)
OPT := 0
SANITIZE := 1
WERROR := 1
NDEBUG := 0
FRAME := 1
PSTRACE := 1
endif

_ := $(shell echo MODE:=$(MODE) > .mode.d)

######################################################################

CC.default := gcc
CC.nix64   := gcc -m64
CC.nix32   := gcc -m32
CC.win64   := x86_64-w64-mingw32-gcc
CC.win32   := i686-w64-mingw32-gcc

_EXE.win64  := .exe
_EXE.win32  := .exe

_LIB.default := .a
_LIB.nix64   := .a
_LIB.nix32   := .a
_LIB.win64   := .lib
_LIB.win32   := .lib

LIB_.default := lib
LIB_.nix64   := lib
LIB_.nix32   := lib

TARGET := default

CC   := $(CC.$(TARGET))
_EXE := $(_EXE.$(TARGET))
_LIB := $(_LIB.$(TARGET))
LIB_ := $(LIB_.$(TARGET))

AR := ar
RANLIB := ranlib
MKINSTALLDIR := mkdir -p
UNINSTALL := rm -f
UNINSTALL_DIR := rmdir

CFLAGS_OPT   := -O$(OPT) -g3
CFLAGS_ARCH  := -march=core2 -mfpmath=sse

CFLAGS_SAFE  += -pipe
CFLAGS_SAFE  += -fno-delete-null-pointer-checks
CFLAGS_SAFE  += -fwrapv
CFLAGS_SAFE  += -fno-strict-overflow

CPPFLAGS_SAFE += -D_FORTIFY_SOURCE=2
CFLAGS_SAFE  += -fstack-protector-strong
#CFLAGS_SAFE  += -fstack-clash-protection
CFLAGS_SAFE  += -fPIE -pie
CFLAGS_SAFE  += -Wl,-z,noexecstack
CFLAGS_SAFE  += -Wl,-z,relro
CFLAGS_SAFE  += -Wl,-z,now
CFLAGS_SAFE  += -Wl,-z,defs

ifeq ($(FRAME),0)
CFLAGS_DEBUG += -fomit-frame-pointer
endif
ifeq ($(SANITIZE),1)
CFLAGS_DEBUG += -fsanitize=undefined
# CFLAGS_DEBUG += -fsanitize=address # currently needs ASAN_OPTIONS=detect_leaks=0
endif
ifeq ($(NDEBUG),1)
CFLAGS_DEBUG += -DNDEBUG
endif
ifeq ($(PSTRACE),1)
CPPFLAGS_DEF += -DPSTRACE
endif
ifeq ($(WITH_FONT),1)
CPPFLAGS_DEF += -DWITH_FONT
endif

CSTD=c11
CPPFLAGS_STD := -std=$(CSTD)

CPPFLAGS_INC += -I$(srcdir)/include
CPPFLAGS_DEF += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

# warnings:
CFLAGS_WARN   += -W -Wall -Wextra

CFLAGS_WARN   += -Wstrict-prototypes
CFLAGS_WARN   += -Wmissing-prototypes
CPPFLAGS_WARN += -Wundef
CFLAGS_WARN   += -Wold-style-definition
CFLAGS_WARN   += -Wnested-externs
CFLAGS_WARN   += -Wmissing-prototypes
CFLAGS_WARN   += -Wstrict-prototypes
CFLAGS_WARN   += -Waggregate-return
CFLAGS_WARN   += -Wpointer-arith
CFLAGS_WARN   += -Wcast-qual
CFLAGS_WARN   += -Wcast-align
CFLAGS_WARN   += -Wwrite-strings
CFLAGS_WARN   += -Wshadow
CFLAGS_WARN   += -Wdeprecated
CFLAGS_WARN   += -Wlogical-op
CFLAGS_WARN   += -Wtrampolines
CFLAGS_WARN   += -Wsuggest-attribute=format
CFLAGS_WARN   += -Wsuggest-attribute=noreturn
CFLAGS_WARN   += -Wconversion
CFLAGS_WARN   += -Wsign-conversion
CFLAGS_WARN   += -Wstrict-overflow=5
CPPFLAGS_WARN += -finput-charset=us-ascii
CFLAGS_WARN   += -Wshadow
CFLAGS_WARN   += -Wmultichar
CFLAGS_WARN   += -Wfloat-equal
CFLAGS_WARN   += -Wshift-overflow=2
CFLAGS_WARN   += -Wbad-function-cast
CFLAGS_WARN   += -Wjump-misses-init
CFLAGS_WARN   += -Wredundant-decls
CFLAGS_WARN   += -Wvla

ifeq ($(WERROR),1)
CFLAGS_WARN   += -Werror
endif

CFLAGS_WARN   += -fmax-errors=10

######################################################################

all:

include install.mk
include test.mk

######################################################################

CFLAGS   += $(CFLAGS_OPT)
CFLAGS   += $(CFLAGS_WARN)
CFLAGS   += $(CFLAGS_ARCH)
CFLAGS   += $(CFLAGS_SAFE)
CFLAGS   += $(CFLAGS_DEBUG)

CPPFLAGS += $(CPPFLAGS_STD)
CPPFLAGS += $(CPPFLAGS_DEF)
CPPFLAGS := $(CPPFLAGS_INC) $(CPPFLAGS)
CPPFLAGS += $(CPPFLAGS_WARN)
CPPFLAGS += $(CPPFLAGS_SAFE)

######################################################################

package_dir := $(package_name)

HOB3L := ./hob3l.exe
HOB3L_JS_COPY_AUX := pkgdatadir=./share sh ./script/hob3l-js-copy-aux.in

######################################################################

TEST_TRIANGLE.png := \
    $(addprefix test-out/,$(notdir $(TEST_TRIANGLE.scad:.scad=.png)))

TEST_STL.stl := \
    $(addprefix test-out/,$(notdir $(TEST_STL.scad:.scad=.stl)))

TEST_STL.jsgz := \
    $(addprefix test-out/,$(notdir $(TEST_STL.scad:.scad=.js.gz)))

FAIL_TRIANGLE := \
    $(addprefix test-out/fail-,$(notdir $(FAIL_TRIANGLE.scad:.scad=.ps)))

FAIL_STL := \
    $(addprefix test-out/fail-,$(notdir $(FAIL_STL.scad:.scad=.stl)))

FAIL_JS := \
    $(addprefix test-out/fail-,$(notdir $(FAIL_JS.scad:.scad=.js)))

######################################################################
# header files

H_CPMAT := $(notdir $(wildcard include/hob3lbase/*.h))
H_HOB3L := $(notdir $(wildcard include/hob3l/*.h))

######################################################################
# data files

SHARE_DATA := \
    gl-matrix/common.js \
    gl-matrix/mat4.js \
    gl-matrix/vec3.js \
    js-hob3l.local.html

######################################################################

# Basic Algorithms and Data Structures:
# libhob3lbase.a:
MOD_C.libhob3lbase.a := \
    mat.c \
    mat_gen_ext.c \
    mat_is_rot.c \
    arith.c \
    algo.c \
    vec.c \
    dict.c \
    list.c \
    ring.c \
    stream.c \
    pool.c \
    vchar.c \
    panic.c \
    internal.c \
    qsort.c \
    utf8.c

MOD_O.libhob3lbase.a := $(addprefix out/,$(MOD_C.libhob3lbase.a:.c=.o))
MOD_D.libhob3lbase.a := $(addprefix out/,$(MOD_C.libhob3lbase.a:.c=.d))

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
    csg2-tree.c \
    csg2-layer.c \
    csg2-triangle.c \
    csg2-bitmap.c \
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

# Tests:
# libcptest.a:
MOD_C.libcptest.a := \
    test.c \
    math-test.c \
    dict-test.c \
    list-test.c \
    ring-test.c

MOD_O.libcptest.a := $(addprefix out/,$(MOD_C.libcptest.a:.c=.o))
MOD_D.libcptest.a := $(addprefix out/,$(MOD_C.libcptest.a:.c=.d))

# Command Line Executable:
# hob3l.exe:
MOD_C.hob3l.exe := \
    main.c

MOD_O.hob3l.exe := $(addprefix out/,$(MOD_C.hob3l.exe:.c=.o))
MOD_D.hob3l.exe := $(addprefix out/,$(MOD_C.hob3l.exe:.c=.d))

# Test Executable:
# cptest.exe:
MOD_C.cptest.exe := \
    test-main.c

MOD_O.cptest.exe := $(addprefix out/,$(MOD_C.cptest.exe:.c=.o))
MOD_D.cptest.exe := $(addprefix out/,$(MOD_C.cptest.exe:.c=.d))

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

all: \
    cptest.exe \
    libcptest.a

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

libcptest.a: $(MOD_O.libcptest.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

hob3l.exe: $(MOD_O.hob3l.exe) $(LIB_A.hob3l.exe)
	$(CC) -o $@ $(MOD_O.hob3l.exe) -L. $(LIB_L.hob3l.exe) $(LIBS) -lm $(CFLAGS)

cptest.exe: $(MOD_O.cptest.exe) libhob3lbase.a libcptest.a
	$(CC) -o $@ $(MOD_O.cptest.exe) -L. -lcptest -lhob3lbase $(LIBS) -lm $(CFLAGS)

fontgen.exe: $(MOD_O.fontgen.exe) libhob3lbase.a libhob3l.a
	$(CC) -o $@ $(MOD_O.fontgen.exe) -L. -lhob3l -lhob3lbase $(LIBS) -lm $(CFLAGS)

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

.PHONY: unit-test
unit-test: cptest.exe
	./cptest.exe


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

test-out/%.png: test-out/%.ps
	gm convert -border 10x10 -bordercolor '#ffffff' -density 144x144 $< $@.new.png
	mv $@.new.png $@

test-out/%.ps: scad-test/%.scad hob3l.exe
	$(HOB3L) $< -z=2.0 $(HOB3L_OPT) -o $@.new.ps
	mv $@.new.ps $@

test-out/fail-%.ps: scad-test/%.scad hob3l.exe
	! $(MAKE) test-out/$*.ps
	echo >| $@

test-out/%.stl: scad-test/%.scad hob3l.exe
	$(HOB3L) $< -o $@.new.stl
	mv $@.new.stl $@

test-out/fail-%.stl: scad-test/%.scad hob3l.exe
	! $(MAKE) test-out/$*.stl
	echo >| $@

test-out/%.stl: $(SCAD_DIR)/%.scad hob3l.exe
	openscad $< -o $@.new.csg
	$(HOB3L) $@.new.csg -o $@.new.stl
	mv $@.new.stl $@
	rm -f $@.new.csg

test-out/%.js: scad-test/%.scad hob3l.exe
	$(HOB3L) $< $(HOB3L_OPT) -o $@.new.js
	cat $(wildcard $<.js) $@.new.js > $@.new2.js
	mv $@.new2.js $@
	rm $@.new.js

test-out/fail-%.js: scad-test/%.scad hob3l.exe
	! $(HOB3L) $< -o $@.new.js
	echo >| $@

test-out/%.js: $(SCAD_DIR)/%.scad hob3l.exe
	openscad $< -o $@.new.csg
	$(HOB3L) $@.new.csg -o $@.new.js
	mv $@.new.js $@
	rm -f $@.new.csg

test-out/%.js.gz: test-out/%.js
	$(HOB3L_JS_COPY_AUX) $(@:.gz=)

scad-test/%.scad: scad-test/%.fig $(srcdir)/script/fig2scad
	$(srcdir)/script/fig2scad $< > $@.new
	mv $@.new $@

scad-test/%-mx.scad: scad-test/%.fig $(srcdir)/script/fig2scad
	$(srcdir)/script/fig2scad --mirror=x $< > $@.new
	mv $@.new $@

scad-test/%-r90.scad: scad-test/%.fig $(srcdir)/script/fig2scad
	$(srcdir)/script/fig2scad --rotate=90 $< > $@.new
	mv $@.new $@

scad-test/%-r30.scad: scad-test/%.fig $(srcdir)/script/fig2scad
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
test-scad: $(addprefix test-out/,$(notdir $(SCAD_SCAD:.scad=.stl)))

.PHONY: speed-test
speed-test:
	rm -f .mode.d.old && mv .mode.d .mode.d.old
	$(MAKE) clean MODE=release
	$(MAKE) -j
	$(MAKE) -j test
	sync && sleep 1
	bash -c 'time $(HOB3L) scad-test/curry.scad -o out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) scad-test/curry.scad -o out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) scad-test/uselessbox+body.scad -o out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) scad-test/uselessbox+body.scad -o out.stl'
	sync && sleep 1
	bash -c 'time $(HOB3L) scad-test/uselessbox+body.scad'
	sync && sleep 1
	bash -c 'time $(HOB3L) scad-test/uselessbox+body.scad'
	sync && sleep 1
	bash -c 'time $(HOB3L) scad-test/test31b.scad'
	sync && sleep 1
	bash -c 'time $(HOB3L) scad-test/test31b.scad'
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
	    $(DESTDIR)$(bindir)/$(package_name)$(_EXE)
	$(INSTALL_SCRIPT) \
	    out/hob3l-js-copy-aux \
	    $(DESTDIR)$(bindir)/$(package_name)-js-copy-aux

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
	$(INSTALL_DATA) libhob3l.a $(DESTDIR)$(libdir)/$(LIB_)$(package_name)$(_LIB)

install-include: installdirs-include
	$(NORMAL_INSTALL)
	for H in $(H_CPMAT); do \
	    $(INSTALL_DATA) \
	        include/hob3lbase/$$H \
	        $(DESTDIR)$(includedir)/hob3lbase/$$H || exit 1; \
	done
	for H in $(H_HOB3L); do \
	    $(INSTALL_DATA) \
	        include/hob3l/$$H \
	        $(DESTDIR)$(includedir)/hob3l/$$H || exit 1; \
	done

uninstall:
	$(NORMAL_UNINSTALL)
	$(UNINSTALL) $(DESTDIR)$(bindir)/$(package_name)$(EXE)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3lfont$(_LIB)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3lbase$(_LIB)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)$(package_name)$(_LIB)
	for H in $(H_CPMAT); do \
	    $(UNINSTALL) $(DESTDIR)$(includedir)/hob3lbase/$$H || exit 1; \
	done
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
	    HOB3L=$(DESTDIR)$(bindir)/$(package_name)$(_EXE) \
	    HOB3L_JS_COPY_AUX=$(DESTDIR)$(bindir)/$(package_name)-js-copy-aux \
	    no-unit-test
