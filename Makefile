# -*- Mode: Makefile -*-

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

TARGET := default

CC := $(CC.$(TARGET))

_ := $(shell mkdir -p out)
_ := $(shell mkdir -p test-out)

AR := ar
RANLIB := ranlib

CSTD=c11

CFLAGS_OPT  := -O$(OPT) -g3
CFLAGS_STD  := -std=$(CSTD)
CFLAGS_ARCH := -march=core2 -mfpmath=sse
CFLAGS_SAFE := -fno-delete-null-pointer-checks -fwrapv

ifeq ($(FRAME),0)
CFLAGS_DEBUG += -fomit-frame-pointer
endif
ifeq ($(SANITIZE),1)
CFLAGS_DEBUG += -fsanitize=undefined
# CFLAGS_DEBUG += -fsanitize=address # FIXME: after using pools correctly, debug leaks
endif
ifeq ($(NDEBUG),1)
CFLAGS_DEBUG += -DNDEBUG
endif
ifeq ($(PSTRACE),1)
CPPFLAGS += -DPSTRACE
endif

CFLAGS += $(CFLAGS_STD)
CFLAGS += $(CFLAGS_OPT)
CFLAGS += $(CFLAGS_ARCH)
CFLAGS += $(CFLAGS_SAFE)
CFLAGS += $(CFLAGS_DEBUG)

CPPFLAGS += -I./include
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE

# warnings:
CFLAGS   += -W -Wall -Wextra

CFLAGS   += -Wstrict-prototypes
CFLAGS   += -Wmissing-prototypes
CPPFLAGS += -Wundef
CFLAGS   += -Wold-style-definition
CFLAGS   += -Wnested-externs
CFLAGS   += -Wmissing-prototypes
CFLAGS   += -Wstrict-prototypes
CFLAGS   += -Waggregate-return
CFLAGS   += -Wpointer-arith
CFLAGS   += -Wcast-qual
CFLAGS   += -Wwrite-strings
CFLAGS   += -Wshadow
CFLAGS   += -Wdeprecated
CFLAGS   += -Wlogical-op
CFLAGS   += -Wtrampolines
CFLAGS   += -Wsuggest-attribute=format
CFLAGS   += -Wsuggest-attribute=noreturn
CFLAGS   += -Wconversion
CFLAGS   += -Wstrict-overflow=5
CPPFLAGS += -finput-charset=us-ascii
CFLAGS   += -Wshadow
CFLAGS   += -Wmultichar

ifeq ($(WERROR),1)
CFLAGS   += -Werror -Wno-error=unused-parameter
endif

######################################################################

include test.mk

######################################################################

# Basic Algorithms and Data Structures:
# libcpmat.a:
MOD_C.libcpmat.a := \
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
    qsort.c

MOD_O.libcpmat.a := $(addprefix out/,$(MOD_C.libcpmat.a:.c=.o))
MOD_D.libcpmat.a := $(addprefix out/,$(MOD_C.libcpmat.a:.c=.d))

# Main Functionality:
# libcsg2plane.a:
MOD_C.libcsg2plane.a := \
    syn.c \
    syn-2scad.c \
    scad.c \
    scad-2scad.c \
    csg3.c \
    csg3-2scad.c \
    csg2-tree.c \
    csg2-layer.c \
    csg2-triangle.c \
    csg2-bool.c \
    csg2-2scad.c \
    csg2-2stl.c \
    csg2-2ps.c \
    ps.c \
    gc.c

MOD_O.libcsg2plane.a := $(addprefix out/,$(MOD_C.libcsg2plane.a:.c=.o))
MOD_D.libcsg2plane.a := $(addprefix out/,$(MOD_C.libcsg2plane.a:.c=.d))

# Tests:
# libcptest.a:
MOD_C.libcptest.a := \
    test-lib.c \
    dict-test.c \
    list-test.c \
    ring-test.c

MOD_O.libcptest.a := $(addprefix out/,$(MOD_C.libcptest.a:.c=.o))
MOD_D.libcptest.a := $(addprefix out/,$(MOD_C.libcptest.a:.c=.d))

# Command Line Executable:
# csg2plane.x:
MOD_C.csg2plane.x := \
    main.c

MOD_O.csg2plane.x := $(addprefix out/,$(MOD_C.csg2plane.x:.c=.o))
MOD_D.csg2plane.x := $(addprefix out/,$(MOD_C.csg2plane.x:.c=.d))

# Test Executable:
# cptest.x:
MOD_C.cptest.x := \
    test.c

MOD_O.cptest.x := $(addprefix out/,$(MOD_C.cptest.x:.c=.o))
MOD_D.cptest.x := $(addprefix out/,$(MOD_C.cptest.x:.c=.d))

######################################################################

all:

-include out/*.d

.SECONDARY:

.SUFFIXES:

.PHONY: all
all: \
    csg2plane.x \
    cptest.x

.PHONY: zap
zap: sweep clean

.PHONY: sweep
sweep:
	rm -f *~
	rm -f *.bak
	rm -f src/*~
	rm -f src/*.bak
	rm -f include/*~
	rm -f include/*.bak
	rm -f include/cpmat/*~
	rm -f include/cpmat/*.bak
	rm -f include/csg2plane/*~
	rm -f include/csg2plane/*.bak

.PHONY: clean-test
clean-test:
	rm -rf test-out

.PHONY: zap
zap: clean sweep
	rm -f mode.d

.PHONY: clean
clean: clean-test
	rm -rf out
	rm -f *.o
	rm -f *.d
	rm -f *.i
	rm -f *.a
	rm -f *.x

libcsg2plane.a: $(MOD_O.libcsg2plane.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

libcpmat.a: $(MOD_O.libcpmat.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

libcptest.a: $(MOD_O.libcptest.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

csg2plane.x: $(MOD_O.csg2plane.x) libcsg2plane.a libcpmat.a
	$(CC) $(CFLAGS) -o $@ $(MOD_O.csg2plane.x) -L. -lcsg2plane -lcpmat $(LIBS) -lm

cptest.x: $(MOD_O.cptest.x) libcpmat.a libcptest.a
	$(CC) $(CFLAGS) -o $@ $(MOD_O.cptest.x) -L. -lcptest -lcpmat $(LIBS) -lm

src/mat_gen_ext.c: mkmat.pl
	./mkmat.pl

src/mat_is_rot.c: mkrotmat.pl
	./mkrotmat.pl > $@.new
	mv $@.new $@

out/%.o: src/%.c src/mat_gen_ext.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MT $@ -MF out/$*.d -c -o $@ $<

%.i: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MT $@ -MF out/$*.d -E $< > $@.new
	mv $@.new $@

out/main.o: src/main.c src/opt.inc

%.inc: %.switch mkswitch
	./mkswitch $< > $@.new
	mv $@.new $@

.PHONY: test
test: test-triangle test-triangle-prepare unit-test

.PHONY: unit-test
unit-test: cptest.x
	./cptest.x

.PHONY: test-triangle
test-triangle: $(TEST_TRIANGLE.png)

.PHONY: test-triangle-prepare
test-triangle-prepare: $(TEST_TRIANGLE.scad)

test-out/%.png: test-out/%.ps
	gm convert -border 10x10 -bordercolor '#ffffff' -density 144x144 $< $@.new.png
	mv $@.new.png $@

test-out/%.ps: scad-test/%.scad csg2plane.x
	./csg2plane.x $< -z=2.0 --dump-ps > $@.new
	mv $@.new $@

scad-test/%.scad: scad-test/%.fig
	./fig2scad.pl $< > $@.new
	mv $@.new $@

scad-test/%-mx.scad: scad-test/%.fig
	./fig2scad.pl --mirror=x $< > $@.new
	mv $@.new $@

scad-test/%-r90.scad: scad-test/%.fig
	./fig2scad.pl --rotate=90 $< > $@.new
	mv $@.new $@

scad-test/%-r30.scad: scad-test/%.fig
	./fig2scad.pl --rotate=30 $< > $@.new
	mv $@.new $@
