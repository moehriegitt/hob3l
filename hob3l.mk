# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

package_name := hob3l
package_version := test

WITH_FONT :=

######################################################################

HOB3L := ./out/bin/hob3l.x
HOB3L_JS_COPY_AUX := pkgdatadir=./share sh ./script/hob3l-js-copy-aux.in

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
    hob3l/internal.c \
    hob3l/syn.c \
    hob3l/syn-2scad.c \
    hob3l/syn-msg.c \
    hob3l/stl-parse.c \
    hob3l/vec3-dict.c \
    hob3l/scad.c \
    hob3l/scad-2scad.c \
    hob3l/csg3.c \
    hob3l/csg3-2scad.c \
    hob3l/csg2.c \
    hob3l/csg2-tree.c \
    hob3l/csg2-layer.c \
    hob3l/csg2-bool.c \
    hob3l/csg2-hull.c \
    hob3l/csg2-2scad.c \
    hob3l/csg2-2stl.c \
    hob3l/csg2-2js.c \
    hob3l/csg2-2ps.c \
    hob3l/ps.c \
    hob3l/gc.c

MOD_O.libhob3l.a := $(addprefix out/bin/,$(MOD_C.libhob3l.a:.c=.o))
MOD_D.libhob3l.a := $(addprefix out/bin/,$(MOD_C.libhob3l.a:.c=.d))

# Command Line Executable:
# hob3l.x:
MOD_C.hob3l.x := \
    hob3l/main.c

MOD_O.hob3l.x := $(addprefix out/bin/,$(MOD_C.hob3l.x:.c=.o))
MOD_D.hob3l.x := $(addprefix out/bin/,$(MOD_C.hob3l.x:.c=.d))

LIB.hob3l.x := \
    hob3l \
    hob3lmat \
    hob3lbase \
    hob3lop

LIB_A.hob3l.x := $(addsuffix $(_LIB), $(addprefix out/bin/$(LIB_), $(LIB.hob3l.x)))
LIB_L.hob3l.x := $(addprefix -l, $(LIB.hob3l.x))

######################################################################

_ := $(shell mkdir -p out/test/hob3l)
_ := $(shell mkdir -p out/bin/hob3l)
_ := $(shell mkdir -p out/src/hob3l)
_ := $(shell mkdir -p out/share)
_ := $(shell mkdir -p out/speed)

-include out/bin/hob3l/*.d

bin: \
    hob3l.x \
    out/share/hob3l-js-copy-aux

hob3l.x: out/bin/hob3l.x

lib: $(LIB_A.hob3l.x)

data: $(addprefix out/,$(OUT_DATA))

sweep: sweep-hob3l
sweep-hob3l:
	rm -f *~
	rm -f *.bak
	rm -f src/*~
	rm -f src/*.bak
	rm -f include/*~
	rm -f include/*.bak
	rm -f include/hob3l/*~
	rm -f include/hob3l/*.bak

out/bin/$(LIB_)hob3l$(_LIB): $(MOD_O.libhob3l.a)
	$(AR) cr $@.new$(_LIB) $+
	$(RANLIB) $@.new$(_LIB)
	mv $@.new$(_LIB) $@

out/bin/hob3l/main.o: CPPFLAGS += -Iout/src/hob3l

out/bin/hob3l.x: $(MOD_O.hob3l.x) $(LIB_A.hob3l.x)
	$(CC) -o $@ $(MOD_O.hob3l.x) -Lout/bin $(LIB_L.hob3l.x) $(LIBS) -lm $(CFLAGS)

out/share/%: script/%.in
	sed 's_@pkgdatadir@_$(pkgdatadir)_g' $< > $@.new
	mv $@.new $@

out/bin/hob3l/main.o: src/hob3l/main.c out/src/hob3l/opt.inc

out/%.inc: %.switch $(srcdir)/script/mkswitch
	$(srcdir)/script/mkswitch $< > $@.new
	mv $@.new $@

.PHONY: markdown
markdown: \
    out/html/README.html \
    out/html/doc/scadformat.html \
    out/html/doc/jsformat.html \

out/html/%.html: %.md
	mkdir -p ./$(dir $@)
	markdown $< > $@.new
	mv $@.new $@

######################################################################
# installation, the usual ceremony.

installdirs-bin: installdirs-bin-hob3l
installdirs-bin-hob3l:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(bindir)

installdirs-data: installdirs-data-hob3l
installdirs-data-hob3l:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(pkgdatadir)
	$(MKINSTALLDIR) $(DESTDIR)$(pkgdatadir)/gl-matrix

installdirs-lib: installdirs-lib-hob3l
installdirs-lib-hob3l:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(libdir)

installdirs-include: installdirs-include-hob3l
installdirs-include-hob3l:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(includedir)/hob3l

install-bin: install-bin-hob3l
install-bin-hob3l: installdirs-bin-hob3l
	$(NORMAL_INSTALL)
	$(INSTALL_BIN) \
	    out/bin/hob3l.x \
	    $(DESTDIR)$(bindir)/hob3l$(_EXE)
	$(INSTALL_SCRIPT) \
	    out/share/hob3l-js-copy-aux \
	    $(DESTDIR)$(bindir)/hob3l-js-copy-aux

install-data: install-data-hob3l
install-data-hob3l: installdirs-data-hob3l
	$(NORMAL_INSTALL)
	for F in $(SHARE_DATA); do \
	    $(INSTALL_DATA) \
	        share/$$F \
	        $(DESTDIR)$(pkgdatadir)/$$F || exit 1; \
	done

install-lib: install-lib-hob3l
install-lib-hob3l: installdirs-lib-hob3l
	$(NORMAL_INSTALL)
	$(INSTALL_DATA) out/bin/$(LIB_)hob3l$(_LIB) $(DESTDIR)$(libdir)/$(LIB_)hob3l$(_LIB)

install-include: install-include-hob3l
install-include-hob3l: installdirs-include-hob3l
	$(NORMAL_INSTALL)
	for H in $(H_HOB3L); do \
	    $(INSTALL_DATA) \
	        include/hob3l/$$H \
	        $(DESTDIR)$(includedir)/hob3l/$$H || exit 1; \
	done

uninstall: uninstall-hob3l
uninstall-hob3l:
	$(NORMAL_UNINSTALL)
	$(UNINSTALL) $(DESTDIR)$(bindir)/hob3l$(EXE)
	$(UNINSTALL) $(DESTDIR)$(bindir)/hob3l-js-copy-aux
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3l$(_LIB)
	for H in $(H_HOB3L); do \
	    $(UNINSTALL) $(DESTDIR)$(includedir)/hob3l/$$H || exit 1; \
	done
	for F in $(SHARE_DATA); do \
	    $(UNINSTALL) $(DESTDIR)$(pkgdatadir)/$$F || exit 1; \
	done
	$(UNINSTALL_DIR) $(DESTDIR)$(includedir)/hob3l
	$(UNINSTALL_DIR) $(DESTDIR)$(pkgdatadir)/gl-matrix
	$(UNINSTALL_DIR) $(DESTDIR)$(pkgdatadir)
