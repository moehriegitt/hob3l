# -*- Mode: Makefile -*-
# Copyright (C) 2018-2023 by Henrik Theiling, License: GPLv3, see LICENSE file

# The usual structure of variables and targets for a standard package.

######################################################################
# programs

INSTALL := install
INSTALL_PROG   := $(INSTALL)
INSTALL_DATA   := $(INSTALL) -m 644
INSTALL_BIN    := $(INSTALL_PROG)
INSTALL_SCRIPT := $(INSTALL_PROG)

######################################################################
# directories

srcdir           := .

prefix           := /usr/local
exec_prefix      := $(prefix)

bindir           := $(exec_prefix)/bin
sbindir          := $(exec_prefix)/sbin
libexecdir       := $(exec_prefix)/libexec
libdir           := $(exec_prefix)/lib

datarootdir      := $(prefix)/share
sysconfdir       := $(prefix)/etc
includedir       := $(prefix)/include
sharedstatedir   := $(prefix)/com
localstatedir    := $(prefix)/var

datadir          := $(datarootdir)
docdir           := $(datarootdir)/doc/$(package_dir)
infodir          := $(datarootdir)/info
localedir        := $(datarootdir)/locale
mandir           := $(datarootdir)/man
lispdir          := $(datarootdir)/emacs/site-lisp

pkgdatadir       := $(datadir)/$(package_dir)

htmldir          := $(docdir)
dvidir           := $(docdir)
pdfdir           := $(docdir)
psdir            := $(docdir)

runstatedir      := $(localstatedir)/run

man1dir          := $(mandir)/man1
man2dir          := $(mandir)/man2

man1ext          := .1
man2ext          := .2

######################################################################
# targets:

.PHONY: install-bin
install-bin:

.PHONY: install-data
install-data:

.PHONY: install-lib
install-lib:

.PHONY: install-include
install-include:

.PHONY: install-doc
install-doc: doc

.PHONY: install
install: install-bin install-data install-lib install-include install-doc

.PHONY: uninstall
uninstall:

.PHONY: install-lib
install-lib: lib installdirs-lib

.PHONY: install-bin
install-bin: bin installdirs-bin

.PHONY: install-data
install-data: data installdirs-data

.PHONY: install-include
install-include: installdirs-include

.PHONY: install-doc
install-doc: install-man install-info install-html install-dvi install-ps install-pdf

.PHONY: installdirs
installdirs: \
    installdirs-bin installdirs-data installdirs-lib installdirs-include installdirs-doc

.PHONY: installdirs-bin
installdirs-bin:

.PHONY: installdirs-data
installdirs-data:

.PHONY: installdirs-lib
installdirs-lib:

.PHONY: installdirs-include
installdirs-include:

.PHONY: installdirs-doc
installdirs-doc: \
    installdirs-man installdirs-info \
    installdirs-html installdirs-dvi installdirs-ps installdirs-pdf

.PHONY: installdirs-man
installdirs-man:

.PHONY: installdirs-info
installdirs-info:

.PHONY: installdirs-html
installdirs-html:

.PHONY: installdirs-dvi
installdirs-dvi:

.PHONY: installdirs-ps
installdirs-ps:

.PHONY: installdirs-pdf
installdirs-pdf:

.PHONY: install-man
install-man: man installdirs-man

.PHONY: install-info
install-info: info installdirs-info

.PHONY: install-html
install-html: html installdirs-html

.PHONY: install-dvi
install-dvi: dvi installdirs-dvi

.PHONY: install-ps
install-ps: ps installdirs-ps

.PHONY: install-pdf
install-pdf: pdf installdirs-pdf

.PHONY: install-strip-bin
install-strip-bin:
	$(MAKE) INSTALL_BIN='$(INSTALL_BIN) -s' install-bin

.PHONY: install-strip
install-strip:
	$(MAKE) INSTALL_BIN='$(INSTALL_BIN) -s' install

.PHONY: mostlyclean
mostlyclean:

.PHONY: clean
clean: mostlyclean

.PHONY: distclean
distclean: clean

.PHONY: maintainer-clean
maintainer-clean: distclean

.PHONY: all
all: bin data lib doc

.PHONY: doc
doc: info man html dvi ps pdf

.PHONY: bin
bin:

.PHONY: data
data:

.PHONY: lib
lib:

.PHONY: info
info:

.PHONY: man
man:

.PHONY: html
html:

.PHONY: ps
ps:

.PHONY: dvi
dvi:

.PHONY: pdf
pdf:

.PHONY: dist
dist:

.PHONY: check
check:

.PHONY: TAGS
TAGS:
