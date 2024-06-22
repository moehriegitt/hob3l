# -*- Mode: Makefile -*-
# Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file

H_hob3lfont := $(notdir $(wildcard include/hob3lfont/*.h))

######################################################################

# Font library:
# libhob3lfont.a:
MOD_C.libhob3lfont.a := \
    hob3lfont/font-nozzl3_sans.c \
    hob3lfont/font-nozzl3_sans_medium.c \
    hob3lfont/font-nozzl3_sans_medium_oblique.c \
    hob3lfont/font-nozzl3_sans_oblique.c

MOD_O.libhob3lfont.a := $(addprefix out/bin/,$(MOD_C.libhob3lfont.a:.c=.o))
MOD_D.libhob3lfont.a := $(addprefix out/bin/,$(MOD_C.libhob3lfont.a:.c=.d))

# Font generator:
# hob3lfontgen.x:
MOD_C.hob3lfontgen.x := \
    hob3lfont/fontgen.c

MOD_O.hob3lfontgen.x := $(addprefix out/bin/,$(MOD_C.hob3lfontgen.x:.c=.o))
MOD_D.hob3lfontgen.x := $(addprefix out/bin/,$(MOD_C.hob3lfontgen.x:.c=.d))

LIB.hob3lfontgen.x := \
    hob3l \
    hob3lmat \
    hob3lbase \
    hob3lop

LIB_A.hob3lfontgen.x := $(addsuffix $(_LIB), $(addprefix out/bin/$(LIB_), $(LIB.hob3lfontgen.x)))
LIB_L.hob3lfontgen.x := $(addprefix -l, $(LIB.hob3lfontgen.x))

######################################################################

_ := $(shell mkdir -p out/bin/hob3lfont)
_ := $(shell mkdir -p out/test/hob3lfont)
_ := $(shell mkdir -p out/src/hob3lfont)

-include out/bin/hob3lfont/*.d

all: \
    out/bin/hob3lfontgen.x

sweep: sweep-hob3lfont

.PHONY: sweep-hob3lfont
sweep-hob3lfont:
	rm -f src/hob3lfont/*~
	rm -f src/hob3lfont/*.bak
	rm -f include/*~
	rm -f include/*.bak
	rm -f include/hob3lfont/*~
	rm -f include/hob3lfont/*.bak

out/bin/hob3lfontgen.x: \
    $(MOD_O.hob3lfontgen.x) \
    $(LIB_A.hob3lfontgen.x)
	$(CC) -o $@ $(MOD_O.hob3lfontgen.x) \
	    -Lout/bin $(LIB_L.hob3lfontgen.x) \
	    $(LIBS) -lm $(CFLAGS)

ifneq ($(wildcard out/src/hob3lfont%*.c),)

all: \
    out/bin/$(LIB_)hob3lfont$(_LIB)

lib: $(LIB_A.hob3lfontgen.x)

out/bin/$(LIB_)hob3lfont$(_LIB): $(MOD_O.libhob3lfont.a)
	$(AR) cr $@.new$(_LIB) $+
	$(RANLIB) $@.new$(_LIB)
	mv $@.new$(_LIB) $@

endif

######################################################################
# installation, the usual ceremony.

installdirs-lib: installdirs-lib-hob3lfont
installdirs-lib-hob3lfont:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(libdir)

installdirs-include: installdirs-include-hob3lfont
installdirs-include-hob3lfont:
	$(NORMAL_INSTALL)
	$(MKINSTALLDIR) $(DESTDIR)$(includedir)/hob3lfont

install-lib: install-lib-hob3lfont
install-lib-hob3lfont: installdirs-lib-hob3lfont
	$(NORMAL_INSTALL)
	$(INSTALL_DATA) out/bin/$(LIB_)hob3lfont$(_LIB) $(DESTDIR)$(libdir)/$(LIB_)hob3lfont$(_LIB)

install-include: install-include-hob3lfont
install-include-hob3lfont: installdirs-include-hob3lfont
	$(NORMAL_INSTALL)
	for H in $(H_hob3lfont); do \
	    $(INSTALL_DATA) \
	        include/hob3lfont/$$H \
	        $(DESTDIR)$(includedir)/hob3lfont/$$H || exit 1; \
	done

uninstall: uninstall-hob3lfont
uninstall-hob3lfont:
	$(NORMAL_UNINSTALL)
	$(UNINSTALL) $(DESTDIR)$(libdir)/$(LIB_)hob3lfont$(_LIB)
	for H in $(H_hob3lfont); do \
	    $(UNINSTALL) $(DESTDIR)$(includedir)/hob3lfont/$$H || exit 1; \
	done
	$(UNINSTALL_DIR) $(DESTDIR)$(includedir)/hob3lfont
