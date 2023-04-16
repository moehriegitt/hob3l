# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

H_hob3lmat := $(notdir $(wildcard include/hob3lmat/*.h))

######################################################################

# Basic Algorithms and Data Structures:
MOD_C.libhob3lmat.a := \
    hob3lmat/hob3lmat-test.c \
    hob3lmat/mat.c \
    hob3lmat/mat_gen_ext.c \
    hob3lmat/mat_is_rot.c \
    hob3lmat/algo.c

MOD_O.libhob3lmat.a := $(addprefix out/bin/,$(MOD_C.libhob3lmat.a:.c=.o))
MOD_D.libhob3lmat.a := $(addprefix out/bin/,$(MOD_C.libhob3lmat.a:.c=.d))

######################################################################

# Test Library:
MOD_C.libhob3lmat-test.a := \
    hob3lmat/math-test.c

MOD_O.libhob3lmat-test.a := $(addprefix out/bin/,$(MOD_C.libhob3lmat-test.a:.c=.o))
MOD_D.libhob3lmat-test.a := $(addprefix out/bin/,$(MOD_C.libhob3lmat-test.a:.c=.d))

######################################################################

# Test Executable:
MOD_C.hob3lmat-test.x := \
    hob3lmat/math-test-main.c

MOD_O.hob3lmat-test.x := $(addprefix out/bin/,$(MOD_C.hob3lmat-test.x:.c=.o))
MOD_D.hob3lmat-test.x := $(addprefix out/bin/,$(MOD_C.hob3lmat-test.x:.c=.d))

LIB.hob3lmat-test.x := \
    hob3lmat-test \
    hob3lmat

LIB_A.hob3lmat-test.x := \
    $(addsuffix $(_LIB), \
        $(addprefix out/bin/$(LIB_), $(LIB.hob3lmat-test.x)))

LIB_L.hob3lmat-test.x := $(addprefix -l, $(LIB.hob3lmat-test.x))

######################################################################

_ := $(shell mkdir -p out/bin/hob3lmat)
_ := $(shell mkdir -p out/test/hob3lmat)

-include out/bin/hob3lmat/*.d

all: \
    out/bin/libhob3lmat.a \
    out/bin/libhob3lmat-test.a \
    out/bin/hob3lmat-test.x \
    $(LIB_A.hob3lmat-test.x)

lib: $(LIB_A.hob3lmat-test.x)

sweep: sweep-hob3lmat

.PHONY: sweep-hob3lmat
sweep-hob3lmat:
	rm -f $(srcdir)/src/hob3lmat/*~
	rm -f $(srcdir)/src/hob3lmat/*.bak
	rm -f $(srcdir)/include/*~
	rm -f $(srcdir)/include/*.bak
	rm -f $(srcdir)/include/hob3lmat/*~
	rm -f $(srcdir)/include/hob3lmat/*.bak

out/bin/libhob3lmat.a: $(MOD_O.libhob3lmat.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

out/bin/libhob3lmat-test.a: $(MOD_O.libhob3lmat-test.a)
	$(AR) cr $@.new.a $+
	$(RANLIB) $@.new.a
	mv $@.new.a $@

out/bin/hob3lmat-test.x: \
    $(MOD_O.hob3lmat-test.x) \
    $(LIB_A.hob3lmat-test.x)
	$(CC) -o $@ $(MOD_O.hob3lmat-test.x) \
	    -Lout/bin $(LIB_L.hob3lmat-test.x) \
	    $(LIBS) -lm $(CFLAGS)

src/hob3lmat/mat_gen_ext.c: $(srcdir)/script/mkmat
	$(srcdir)/script/mkmat

src/hob3lmat/mat_is_rot.c: $(srcdir)/script/mkrotmat
	$(srcdir)/script/mkrotmat > $@.new
	mv $@.new $@

out/bin/hob3lmat/math-test.o: CFLAGS+=-Wno-float-equal

.PHONY: mat
mat: src/hob3lmat/mat_gen_ext.c

.PHONY: unit-test
unit-test: unit-test-mat

.PHONY: test-hob3lmat
test-hob3lmat: unit-test-mat

.PHONY: test-mat
test-mat: unit-test-mat

.PHONY: unit-test-mat
unit-test-mat: out/bin/hob3lmat-test.x
	./out/bin/hob3lmat-test.x

######################################################################
# installation, the usual ceremony.

installdirs-lib: installdirs-lib-hob3lmat
installdirs-lib-hob3lmat:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(libdir)

installdirs-include: installdirs-include-hob3lmat
installdirs-include-hob3lmat:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(includedir)/hob3lmat

install-lib: install-lib-hob3lmat
install-lib-hob3lmat: installdirs-lib-hob3lmat
	$(NORMAL_INSTALL)
	$(INSTALL_DATA) out/bin/libhob3lmat.a $(DESTDIR)$(libdir)/$(LIB_)hob3lmat$(_LIB)

install-include: install-include-hob3lmat
install-include-hob3lmat: installdirs-include-hob3lmat
	$(NORMAL_INSTALL)
	for H in $(H_hob3lmat); do \
	    $(INSTALL_DATA) \
	        include/hob3lmat/$$H \
	        $(DESTDIR)$(includedir)/hob3lmat/$$H || exit 1; \
	done

uninstall: uninstall-hob3lmat
uninstall-hob3lmat:
	$(NORMAL_UNINSTALL)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3lmat$(_LIB)
	for H in $(H_hob3lmat); do \
	    $(UNINSTALL) $(DESTDIR)$(includedir)/hob3lmat/$$H || exit 1; \
	done
	$(UNINSTALL_DIR) $(DESTDIR)$(includedir)/hob3lmat
