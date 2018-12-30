all:
	cd .. && $(MAKE) font

uniname.h: mkuniname dummy
	./mkuniname > $@.new
	mv $@.new $@

unisets.inc: mkset dummy
	./mkset > $@.new
	mv $@.new $@

unicomp.inc: compdecomp dummy
	./compdecomp > $@.new
	mv $@.new $@

.PHONY: dummy
dummy:
