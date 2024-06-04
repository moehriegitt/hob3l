# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

SHELL := /bin/sh

ifeq ($(MODE),)
-include .mode.d
endif

# release mode: strict compilation, no sanitizing, no debug, no PS trace
ifeq ($(MODE), release)
OPT ?= 3
ARCH ?=
SANITIZE ?= 0
WERROR ?= 1
NDEBUG ?= 1
FRAME ?= 0
PSTRACE ?= 0
FUZZ ?= 0
endif

# devel mode: strict compilation, full set of diagnostics and checks, debuggable
ifeq ($(MODE), devel)
OPT ?= 0
ARCH ?= native
SANITIZE ?= 1
WERROR ?= 1
NDEBUG ?= 0
FRAME ?= 1
PSTRACE ?= 1
FUZZ ?= 1
endif

# default is 'just do it' mode, with some runtime checks
OPT ?= 2
ARCH ?=
SANITIZE ?= 0
WERROR ?= 0
NDEBUG ?= 0
FRAME ?= 0
PSTRACE ?= 0
FUZZ ?= 0
TARGET ?= gcc

_ := $(shell (\
    echo MODE:=$(MODE) ; \
    echo OPT:=$(OPT) ; \
    echo ARCH:=$(ARCH) ; \
    echo SANITIZE:=$(SANITIZE) ; \
    echo WERROR:=$(WERROR) ; \
    echo NDEBUG:=$(NDEBUG) ; \
    echo FRAME:=$(FRAME) ; \
    echo PSTRACE:=$(PSTRACE) ; \
    echo FUZZ:=$(FUZZ) ; \
    echo TARGET:=$(TARGET) ; \
    echo) > .mode.d)

######################################################################

_ := $(shell mkdir -p out/test)
_ := $(shell mkdir -p out/bin)
_ := $(shell mkdir -p out/src)

######################################################################

CC.gcc   := gcc
CC.clang := clang
CC.gcc64 := gcc -m64
CC.gcc32 := gcc -m32
CC.win64 := x86_64-w64-mingw32-gcc
CC.win32 := i686-w64-mingw32-gcc

KIND.gcc   := gcc
KIND.gcc32 := gcc
KIND.gcc64 := gcc
KIND.win32 := mingw
KIND.win64 := mingw
KIND.clang := clang

_EXE.win64 := .exe
_EXE.win32 := .exe

_LIB.gcc   := .a
_LIB.clang := .a
_LIB.gcc64 := .a
_LIB.gcc32 := .a
_LIB.win64 := .lib
_LIB.win32 := .lib

LIB_.gcc   := lib
LIB_.clang := lib
LIB_.gcc64 := lib
LIB_.gcc32 := lib

KIND := $(KIND.$(TARGET))
CC   := $(CC.$(TARGET))
_EXE := $(_EXE.$(TARGET))
_LIB := $(_LIB.$(TARGET))
LIB_ := $(LIB_.$(TARGET))

######################################################################

AR := ar
RANLIB := ranlib
MKINSTALLDIR := mkdir -p
UNINSTALL := rm -f
UNINSTALL_DIR := rmdir

CFLAGS_OPT   := -O$(OPT) -g3

ifneq ($(ARCH),)
CFLAGS_ARCH  += -march=$(ARCH)
endif

CFLAGS_SAFE      += -pipe
CFLAGS_SAFE.gcc  += -fno-delete-null-pointer-checks
CFLAGS_SAFE      += -fwrapv
CFLAGS_SAFE.gcc  += -fno-strict-overflow

CPPFLAGS_SAFE    += -D_FORTIFY_SOURCE=2
CFLAGS_SAFE      += -fstack-protector-strong
#CFLAGS_SAFE     += -fstack-clash-protection
CFLAGS_SAFE      += -fPIE
CFLAGS_SAFE.gcc  += -pie
CFLAGS_SAFE.gcc  += -Wl,-z,noexecstack
CFLAGS_SAFE.gcc  += -Wl,-z,relro
CFLAGS_SAFE.gcc  += -Wl,-z,now
CFLAGS_SAFE.gcc  += -Wl,-z,defs

ifeq ($(FRAME),0)
CFLAGS_DEBUG += -fomit-frame-pointer
endif
ifeq ($(SANITIZE),1)
CFLAGS_DEBUG += # -fsanitize=undefined -fno-sanitize-recover=all -fsanitize-undefined-trap-on-error
#CFLAGS_DEBUG += -fsanitize=address # currently needs ASAN_OPTIONS=detect_leaks=0
endif
ifeq ($(NDEBUG),1)
CFLAGS_DEBUG += -DNDEBUG
endif
ifeq ($(PSTRACE),1)
CPPFLAGS_DEF += -DPSTRACE -DCQ_TRACE
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
# CFLAGS_WARN   += -Waggregate-return  # we do that, and it is good.
CFLAGS_WARN   += -Wpointer-arith
CFLAGS_WARN   += -Wcast-qual
CFLAGS_WARN   += -Wcast-align
CFLAGS_WARN   += -Wwrite-strings
CFLAGS_WARN   += -Wshadow
CFLAGS_WARN   += -Wdeprecated
CFLAGS_WARN   += -Wconversion
CFLAGS_WARN   += -Wsign-conversion
CFLAGS_WARN   += -Wstrict-overflow=5
CFLAGS_WARN   += -Wshadow
CFLAGS_WARN   += -Wmultichar
CFLAGS_WARN   += -Wfloat-equal
CFLAGS_WARN   += -Wbad-function-cast
CFLAGS_WARN   += -Wredundant-decls
CFLAGS_WARN   += -Wvla

CFLAGS_WARN.gcc += -Wlogical-op
CFLAGS_WARN.gcc += -Wjump-misses-init
CFLAGS_WARN.gcc += -Wtrampolines
CFLAGS_WARN.gcc += -Wsuggest-attribute=format
CFLAGS_WARN.gcc += -Wsuggest-attribute=noreturn
CFLAGS_WARN.gcc += -Wshift-overflow=2

CPPFLAGS_WARN.gcc += -finput-charset=us-ascii

CFLAGS_WARN.clang += -Wno-missing-field-initializers
CFLAGS_WARN.clang += -Wno-missing-braces

ifeq ($(WERROR),1)
CFLAGS_WARN   += -Werror
endif

CFLAGS_WARN.gcc += -fmax-errors=10

######################################################################

FUZZ_CC  := afl-gcc
FUZZ_DEF := -DFUZZ -UNDEBUG -UPSTRACE -UCQ_TRACE
FUZZ_OPT := -O3

######################################################################

all:

include install.mk

CFLAGS += $(CFLAGS_OPT)
CFLAGS += $(CFLAGS_WARN)
CFLAGS += $(CFLAGS_ARCH)
CFLAGS += $(CFLAGS_SAFE)
CFLAGS += $(CFLAGS_DEBUG)

CPPFLAGS += $(CPPFLAGS_STD)
CPPFLAGS += $(CPPFLAGS_DEF)
CPPFLAGS := $(CPPFLAGS_INC) $(CPPFLAGS)
CPPFLAGS += $(CPPFLAGS_WARN)
CPPFLAGS += $(CPPFLAGS_SAFE)

CFLAGS += $(CFLAGS_OPT.$(KIND))
CFLAGS += $(CFLAGS_WARN.$(KIND))
CFLAGS += $(CFLAGS_ARCH.$(KIND))
CFLAGS += $(CFLAGS_SAFE.$(KIND))
CFLAGS += $(CFLAGS_DEBUG.$(KIND))

CPPFLAGS += $(CPPFLAGS_STD.$(KIND))
CPPFLAGS += $(CPPFLAGS_DEF.$(KIND))
CPPFLAGS := $(CPPFLAGS_INC.$(KIND)) $(CPPFLAGS)
CPPFLAGS += $(CPPFLAGS_WARN.$(KIND))
CPPFLAGS += $(CPPFLAGS_SAFE.$(KIND))

######################################################################

.SUFFIXES:

.SECONDARY:

.PHONY: lib
lib:

.PHONY: sweep
sweep:
	rm -f *~
	rm -f *.bak

.PHONY: zap
zap: sweep clean font-clean
	rm -f .mode.d

.PHONY: distclean
distclean: sweep clean

.PHONY: speed-test
speed-test:

.PHONY: test
test: unit-test no-unit-test

.PHONY: fail
fail:

.PHONY: unit-test
unit-test:

.PHONY: no-unit-test
no-unit-test:

.PHONY: clean
clean: clean-test clean-fuzz clean-bin clean-src clean-speed clean-share
	rm -rf out
	rm -f *.o
	rm -f *.d
	rm -f *.i
	rm -f *.a
	rm -f *.x
	rm -f *.exe
	rm -f core
	rm -f src/core
	rm -f src/*/core

.PHONY: clean-test
clean-test:
	rm -rf out/test

.PHONY: clean-fuzz
clean-fuzz:
	rm -rf out/fuzz

.PHONY: clean-bin
clean-bin:
	rm -rf out/bin

.PHONY: clean-src
clean-src:
	rm -rf out/src

.PHONY: clean-speed
clean-speed:
	rm -rf out/speed

.PHONY: clean-share
clean-share:
	rm -rf out/share

######################################################################

PROTO_CH := \
    $(wildcard \
	    $(srcdir)/src/hob3l/*.c \
	    $(srcdir)/src/hob3l/*.h \
	    $(srcdir)/src/hob3lbase/*.c \
	    $(srcdir)/src/hob3lbase/*.h \
	    $(srcdir)/include/hob3l/*.h \
	    $(srcdir)/include/hob3lbase/*.h)

.PHONY: update-header
update-header: script/xproto
	$(srcdir)/script/xproto  $(PROTO_CH)
	$(srcdir)/script/mkmacro $(PROTO_CH)

.PHONY: update-toc
update-toc: README.md script/mktoc
	$(srcdir)/script/mktoc -skip1 -in-place README.md
	$(srcdir)/script/mktoc -skip1 -in-place doc/scadformat.md

######################################################################

out/bin/%.o: src/%.c
	$(CC) -MMD -MP -MT $@ -c -o $@ $< $(CPPFLAGS) $(CFLAGS)

out/bin/%.i: src/%.c
	$(CC) -MMD -MP -MT $@ -E $< $(CPPFLAGS) $(CFLAGS) > $@.new
	mv $@.new $@

out/fuzz/bin/%.o: src/%.c
	$(FUZZ_CC) -MMD -MP -MT $@ -c -o $@ $< $(CPPFLAGS) $(CFLAGS) $(FUZZ_DEF) $(FUZZ_OPT)

######################################################################

include hob3lmat.mk
include hob3lbase.mk
include hob3lop.mk
include hob3l.mk

include hob3lop-test.mk
include hob3l-test.mk

ifeq ($(FUZZ),1)
include hob3lop-fuzz.mk
endif

# Later...
#ifeq ($(FONT),1)
#include hob3lfont.mk
#endif
