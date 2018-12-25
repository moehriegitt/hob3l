all:
	cd .. && $(MAKE) font

uniname.h: mkuniname dummy
	./mkuniname > $@.new
	mv $@.new $@

unisets.inc: mkset dummy
	./mkset > $@.new
	mv $@.new $@

.PHONY: dummy
dummy:
