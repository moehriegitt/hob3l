# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

H_hob3lop := $(notdir $(wildcard include/hob3lop/*.h))

######################################################################

# Integer Algorithms:
MOD_C.libhob3lop.a := \
    hob3lop/hedron.c \
    hob3lop/gon.c \
    hob3lop/op-def.c \
    hob3lop/matq.c \
    hob3lop/op-slice.c \
    hob3lop/op-poly.c \
    hob3lop/op-trianglify.c \
    hob3lop/op-sweep.c \
    hob3lop/op-sweep-trace.c \
    hob3lop/op-sweep-reduce.c

ifeq ($(PSTRACE),1)
MOD_C.libhob3lop.a += \
    hob3lop/op-ps.c
endif

MOD_O.libhob3lop.a := $(addprefix out/bin/,$(MOD_C.libhob3lop.a:.c=.o))
MOD_D.libhob3lop.a := $(addprefix out/bin/,$(MOD_C.libhob3lop.a:.c=.d))

######################################################################

# Int Test Executable:
MOD_C.hob3lop-test.x := \
    hob3lop/hob3lop-test.c

MOD_O.hob3lop-test.x := $(addprefix out/bin/,$(MOD_C.hob3lop-test.x:.c=.o))
MOD_D.hob3lop-test.x := $(addprefix out/bin/,$(MOD_C.hob3lop-test.x:.c=.d))

LIB.hob3lop-test.x := \
    hob3lop \
    hob3lbase

LIB_A.hob3lop-test.x := \
    $(addsuffix $(_LIB), \
        $(addprefix out/bin/$(LIB_), $(LIB.hob3lop-test.x)))

LIB_L.hob3lop-test.x := $(addprefix -l, $(LIB.hob3lop-test.x))

######################################################################

_ := $(shell mkdir -p out/bin/hob3lop)
_ := $(shell mkdir -p out/test/hob3lop)
_ := $(shell mkdir -p out/src/hob3lop)

-include out/bin/hob3lop/*.d

all: \
    out/bin/$(LIB_)hob3lop$(_LIB) \
    out/bin/hob3lop-test.x

lib: $(LIB_A.hob3lop-test.x)

sweep: sweep-hob3lop

.PHONY: sweep-hob3lop
sweep-hob3lop:
	rm -f src/hob3lop/*~
	rm -f src/hob3lop/*.bak
	rm -f include/*~
	rm -f include/*.bak
	rm -f include/hob3lop/*~
	rm -f include/hob3lop/*.bak

out/bin/$(LIB_)hob3lop$(_LIB): $(MOD_O.libhob3lop.a)
	$(AR) cr $@.new$(_LIB) $+
	$(RANLIB) $@.new$(_LIB)
	mv $@.new$(_LIB) $@

out/bin/hob3lop-test.x: \
    $(MOD_O.hob3lop-test.x) \
    $(LIB_A.hob3lop-test.x)
	$(CC) -o $@ $(MOD_O.hob3lop-test.x) \
	    -Lout/bin $(LIB_L.hob3lop-test.x) \
	    $(LIBS) -lm $(CFLAGS)

out/bin/hob3lop/hob3lop-test.o: CPPFLAGS += -Iout/src/hob3lop

unit-test: unit-test-hobl3op

.PHONY: test-hob3lop
test-hob3lop: unit-test-hobl3op

.PHONY: test-op
test-op: unit-test-hobl3op

.PHONY: unit-test-hobl3op
unit-test-hobl3op: out/bin/hob3lop-test.x
	./out/bin/hob3lop-test.x

######################################################################
# installation, the usual ceremony.

installdirs-lib: installdirs-lib-hob3lop
installdirs-lib-hob3lop:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(libdir)

installdirs-include: installdirs-include-hob3lop
installdirs-include-hob3lop:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(includedir)/hob3lop

install-lib: install-lib-hob3lop
install-lib-hob3lop: installdirs-lib-hob3lop
	$(NORMAL_INSTALL)
	$(INSTALL_DATA) out/bin/$(LIB_)hob3lop$(_LIB) $(DESTDIR)$(libdir)/$(LIB_)hob3lop$(_LIB)

install-include: install-include-hob3lop
install-include-hob3lop: installdirs-include-hob3lop
	$(NORMAL_INSTALL)
	for H in $(H_hob3lop); do \
	    $(INSTALL_DATA) \
	        include/hob3lop/$$H \
	        $(DESTDIR)$(includedir)/hob3lop/$$H || exit 1; \
	done

uninstall: uninstall-hob3lop
uninstall-hob3lop:
	$(NORMAL_UNINSTALL)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3lop$(_LIB)
	for H in $(H_hob3lop); do \
	    $(UNINSTALL) $(DESTDIR)$(includedir)/hob3lop/$$H || exit 1; \
	done
	$(UNINSTALL_DIR) $(DESTDIR)$(includedir)/hob3lop
