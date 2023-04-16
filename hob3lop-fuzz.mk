# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

FUZZ_X.hob3lop := out/fuzz/bin/hob3lop-fuzz.x

######################################################################

# Int Fuzz Executable:
MOD_C.hob3lop-fuzz.x := \
    $(MOD_C.hob3lop-test.x) \
    $(MOD_C.libhob3lop.a) \
    $(MOD_C.libhob3lbase.a)

MOD_O.hob3lop-fuzz.x := $(addprefix out/fuzz/bin/,$(MOD_C.hob3lop-fuzz.x:.c=.o))
MOD_D.hob3lop-fuzz.x := $(addprefix out/fuzz/bin/,$(MOD_C.hob3lop-fuzz.x:.c=.d))

LIB.hob3lop-fuzz.x :=

LIB_A.hob3lop-fuzz.x := \
    $(addsuffix $(_LIB), \
        $(addprefix out/bin/$(LIB_), $(LIB.hob3lop-fuzz.x)))

LIB_L.hob3lop-fuzz.x := $(addprefix -l, $(LIB.hob3lop-fuzz.x))

######################################################################

_ := $(shell mkdir -p out/fuzz/bin/hob3lbase)
_ := $(shell mkdir -p out/fuzz/bin/hob3lop)

-include out/fuzz/bin/hob3lbase/*.d
-include out/fuzz/bin/hob3lop/*.d

all: \
    $(FUZZ_X.hob3lop) \
    $(LIB_A.hob3lop-fuzz.x)

lib: $(LIB_A.hob3lop-fuzz.x)

out/fuzz/bin/hob3lop/hob3lop-test.o: \
    $(DEP.hob3lop-test.o)

out/fuzz/bin/hob3lop/hob3lop-test.o: CPPFLAGS += -Iout/src/hob3lop

$(FUZZ_X.hob3lop): \
    $(MOD_O.hob3lop-fuzz.x) \
    $(LIB_A.hob3lop-fuzz.x)
	$(FUZZ_CC) -o $@ $(MOD_O.hob3lop-fuzz.x) \
	    -Lout/bin $(LIB_L.hob3lop-fuzz.x) \
	    $(LIBS) -lm $(CFLAGS)

######################################################################

AFL_ENV  := unset ASAN_OPTIONS ;
AFL_FUZZ := $(AFL_ENV) afl-fuzz

.PHONY: fuzz.x
fuzz.x: $(FUZZ_X.hob3lop)

ifeq ($(CONTINUE),1)
AFL_INPUT := -
endif

ifneq ($(AFL_INPUT),)
AFL1_INPUT := $(AFL_INPUT)
endif
AFL1_INPUT ?= out/fuzz/in

.PHONY: fuzz
fuzz: fuzz.x
	mkdir -p out/fuzz/in
	mkdir -p out/fuzz/001
	echo -n -e '\0' > out/fuzz/in/test0
	$(AFL_FUZZ) -i $(AFL1_INPUT) -o out/fuzz/001 -f out/fuzz/001/test.pol -t 1000 $(AFL_OPT) -- \
	    $(FUZZ_X.hob3lop) --max=10 out/fuzz/001/test.pol

ifneq ($(AFL_INPUT),)
AFL2_INPUT := $(AFL_INPUT)
endif
AFL2_INPUT ?= out/fuzz/in

.PHONY: fuzz2
fuzz2: fuzz.x
	mkdir -p out/fuzz/in
	mkdir -p out/fuzz/002
	echo -n -e '\0' > out/fuzz/in/test0
	$(AFL_FUZZ) -i $(AFL2_INPUT) -o out/fuzz/002 -f out/fuzz/002/test.pol -t 1000 $(AFL_OPT) -- \
	    $(FUZZ_X.hob3lop) --max=100 out/fuzz/002/test.pol

ifneq ($(AFL_INPUT),)
AFL3_INPUT := $(AFL_INPUT)
endif
AFL3_INPUT ?= out/fuzz/in

.PHONY: fuzz3
fuzz3: fuzz.x
	mkdir -p out/fuzz/in
	mkdir -p out/fuzz/003
	echo -n -e '\0' > out/fuzz/in/test0
	$(AFL_FUZZ) -i $(AFL3_INPUT) -o out/fuzz/003 -f out/fuzz/003/test.pol -t 1000 $(AFL_OPT) -- \
	    $(FUZZ_X.hob3lop) --max=20 out/fuzz/003/test.pol

ifneq ($(AFL_INPUT),)
AFL4_INPUT := $(AFL_INPUT)
endif
AFL4_INPUT ?= out/fuzz/in

.PHONY: fuzz4
fuzz4: fuzz.x
	mkdir -p out/fuzz/in
	mkdir -p out/fuzz/004
	echo -n -e '\0' > out/fuzz/in/test0
	$(AFL_FUZZ) -i $(AFL4_INPUT) -o out/fuzz/004 -f out/fuzz/004/test.pol -t 1000 $(AFL_OPT) -- \
	    $(FUZZ_X.hob3lop) --max=50 out/fuzz/004/test.pol
