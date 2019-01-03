all:
	cd .. && $(MAKE) font-doc

uni: \
	uniname.h \
	unisets.inc \
	unicomp.inc \
	../src/unidefign.inc \
	../src/unigrapheme.inc

uniname.h: mkuniname dummy
	./mkuniname > $@.new
	mv $@.new $@

unisets.inc: mkset dummy
	./mkset > $@.new
	mv $@.new $@

unicomp.inc: compdecomp dummy
	./compdecomp > $@.new
	mv $@.new $@

../src/unidefign.inc: prop2c dummy
	./prop2c DerivedCoreProperties.txt +Default_Ignorable_Code_Point > $@.new
	mv $@.new $@

../src/unigrapheme.inc: prop2c dummy
	./mkderived \
	    +Cc +Cf +Cs +Zl +Zp -Grapheme_Extend =Grapheme_Other \
	| ./prop2c - DerivedCoreProperties.txt \
	    +Grapheme_Extend=G_EXTEND \
	    +Grapheme_Other=G_OTHER \
	    > $@.new
	mv $@.new $@

.PHONY: dummy
dummy:
