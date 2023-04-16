# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

H_hob3lbase := $(notdir $(wildcard include/hob3lbase/*.h))

######################################################################

# Basic Algorithms and Data Structures:
MOD_C.libhob3lbase.a := \
    hob3lbase/arith.c \
    hob3lbase/alloc.c \
    hob3lbase/base-mat.c \
    hob3lbase/bool-bitmap.c \
    hob3lbase/vec.c \
    hob3lbase/heap.c \
    hob3lbase/dict.c \
    hob3lbase/list.c \
    hob3lbase/stream.c \
    hob3lbase/pool.c \
    hob3lbase/vchar.c \
    hob3lbase/panic.c \
    hob3lbase/qsort.c \
    hob3lbase/utf8.c

MOD_O.libhob3lbase.a := $(addprefix out/bin/,$(MOD_C.libhob3lbase.a:.c=.o))
MOD_D.libhob3lbase.a := $(addprefix out/bin/,$(MOD_C.libhob3lbase.a:.c=.d))

######################################################################

# Test Library:
MOD_C.libhob3lbase-test.a := \
    hob3lbase/hob3lbase-test.c \
    hob3lbase/dict-test.c \
    hob3lbase/list-test.c

MOD_O.libhob3lbase-test.a := $(addprefix out/bin/,$(MOD_C.libhob3lbase-test.a:.c=.o))
MOD_D.libhob3lbase-test.a := $(addprefix out/bin/,$(MOD_C.libhob3lbase-test.a:.c=.d))

######################################################################

# Test Executable:
MOD_C.hob3lbase-test.x := \
    hob3lbase/base-test-main.c

MOD_O.hob3lbase-test.x := $(addprefix out/bin/,$(MOD_C.hob3lbase-test.x:.c=.o))
MOD_D.hob3lbase-test.x := $(addprefix out/bin/,$(MOD_C.hob3lbase-test.x:.c=.d))

LIB.hob3lbase-test.x := \
    hob3lbase-test \
    hob3lbase

LIB_A.hob3lbase-test.x := \
    $(addsuffix $(_LIB), \
        $(addprefix out/bin/$(LIB_), $(LIB.hob3lbase-test.x)))

LIB_L.hob3lbase-test.x := $(addprefix -l, $(LIB.hob3lbase-test.x))

######################################################################

_ := $(shell mkdir -p out/bin/hob3lbase)
_ := $(shell mkdir -p out/test/hob3lbase)

-include out/bin/hob3lbase/*.d

all: \
    out/bin/libhob3lbase.a \
    out/bin/libhob3lbase-test.a \
    out/bin/hob3lbase-test.x \
    $(LIB_A.hob3lbase-test.x)

lib: $(LIB_A.hob3lbase-test.x)

sweep: sweep-hob3lbase

.PHONY: sweep-hob3lbase
sweep-hob3lbase:
	rm -f $(srcdir)/src/hob3lbase/*~
	rm -f $(srcdir)/src/hob3lbase/*.bak
	rm -f $(srcdir)/include/*~
	rm -f $(srcdir)/include/*.bak
	rm -f $(srcdir)/include/hob3lbase/*~
	rm -f $(srcdir)/include/hob3lbase/*.bak

out/bin/libhob3lbase.a: $(MOD_O.libhob3lbase.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

out/bin/libhob3lbase-test.a: $(MOD_O.libhob3lbase-test.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

out/bin/hob3lbase-test.x: \
    $(MOD_O.hob3lbase-test.x) \
    $(LIB_A.hob3lbase-test.x)
	$(CC) -o $@ $(MOD_O.hob3lbase-test.x) \
	    -Lout/bin $(LIB_L.hob3lbase-test.x) \
	    $(LIBS) -lm $(CFLAGS)

out/script/%: script/%.in
	sed 's_@pkgdatadir@_$(pkgdatadir)_g' $< > $@.new
	mv $@.new $@

.PHONY: unit-test
unit-test: unit-test-base

.PHONY: test-hob3lbase
test-hob3lbase: unit-test-base

.PHONY: test-base
test-base: unit-test-base

.PHONY: unit-test-base
unit-test-base: out/bin/hob3lbase-test.x
	./out/bin/hob3lbase-test.x

######################################################################
# installation, the usual ceremony.

installdirs-lib: installdirs-lib-hob3lbase
installdirs-lib-hob3lbase:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(libdir)

installdirs-include: installdirs-include-hob3lbase
installdirs-include-hob3lbase:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(includedir)/hob3lbase

install-lib: install-lib-hob3lbase
install-lib-hob3lbase: installdirs-lib-hob3lbase
	$(NORMAL_INSTALL)
	$(INSTALL_DATA) out/bin/libhob3lbase.a $(DESTDIR)$(libdir)/$(LIB_)hob3lbase$(_LIB)

install-include: install-include-hob3lbase
install-include-hob3lbase: installdirs-include-hob3lbase
	$(NORMAL_INSTALL)
	for H in $(H_hob3lbase); do \
	    $(INSTALL_DATA) \
	        include/hob3lbase/$$H \
	        $(DESTDIR)$(includedir)/hob3lbase/$$H || exit 1; \
	done

uninstall: uninstall-hob3lbase
uninstall-hob3lbase:
	$(NORMAL_UNINSTALL)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3lbase$(_LIB)
	for H in $(H_hob3lbase); do \
	    $(UNINSTALL) $(DESTDIR)$(includedir)/hob3lbase/$$H || exit 1; \
	done
	$(UNINSTALL_DIR) $(DESTDIR)$(includedir)/hob3lbase
