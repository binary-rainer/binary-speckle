#
# Common Makefile for Binary/Speckle
#
# Created:     Sun Jul 13 18:28:22 1997 by Koehler@Lisa
# Last change: Thu Jan 15 15:50:24 2015
#############################################################################
#
# Base-directory of the package (the dir where this Makefile sits)
#
BINDIR  = /Users/koehler/Binary-Speckle

# Compiler, linker, and their flags
#
CC	= gcc
CFLAGS	= -O3 -Wall -Wno-uninitialized -Wcast-align -Wstrict-prototypes -I$(FITSDIR)
LN	= gcc
LDFLAGS	= -L$(FITSDIR) -lfitsio -lm
#
# Compiler to link with FORTRAN-stuff (e.g. PGPLOT)
#
FLN	= gfortran
#
# The name of GNU make (make sure this calls GNU make and nothing else)
#
GMAKE	= gmake
#
# Tcl/tk: paths to wish, the includes and libraries, and the libs to link with
#
#fink'ed up:
#WISH	   = /sw/bin/wish
#TCL_LIB_DIR= /sw/lib
#ITCLTK	   = -I/sw/include
#LTCLTK	   = -L$(TCL_LIB_DIR) -L/usr/X11R6/lib -ltk8.5 -ltcl8.5 -lX11

# quick hack - installed 8.4 from source
WISH	   = /Users/koehler/X-files/bin/wish
TCL_LIB_DIR= /Users/koehler/X-files/lib
ITCLTK	   = -I/Users/koehler/X-files/include
LTCLTK	   = -L$(TCL_LIB_DIR) -L/usr/X11R6/lib -ltk8.4 -ltcl8.4 -lX11

#
# PGPLOT: path to its directory, and what to do to compile and link
#
PGPLOTDIR = /Users/koehler/X-files/pgplot
IPGPLOT   = -I$(PGPLOTDIR) -DHAVE_TKPGPLOT
LPGPLOT   = -L$(PGPLOTDIR) -ltkpgplot -lcpgplot -lpgplot -ldl
#
# FFTW: what to do to compile and link to do real-to-complex transform
# 10-Jul-02: Define DOUBLE_FFTW to include "drfftw.h" instead of "rfftw.h"
#	    (if you have two versions of fftw for single- and double-precision)
#
IFFTW = -I/Users/koehler/X-files/include -DDOUBLE_FFTW
LFFTW = -L/Users/koehler/X-files/lib -ldrfftw -ldfftw
#
# Which program do you want to use to view Postscript-files?
#
GHOSTVIEW = gv
#
# Directory where libfitsio.sx lives
#
FITSDIR = $(BINDIR)/fitsio

##############################################################################
#
# Host/architecture-depending stuff
#
# not sure which systems have this, cygwin has not...
# Mac does not, either
#
##CFLAGS += -DHAVE_VALUES_H
#
# Linux
#
SHLIBX  = so
#
# Solaris
#
#FLN	  = f77
#SHLIBX  = so
#CFLAGS += -I/usr/openwin/include -I/usr/local/include $(HOST_ARCH:-sparc=-msupersparc)
#LPGPLOT = -R$(PGPLOTDIR) -L$(PGPLOTDIR) -lcpgplot -lpgplot
#LTCLTK := -R/usr/openwin/lib -L/usr/openwin/lib -R$(TCL_LIB_DIR) $(LTCLTK) -lXext -lsocket -lnsl -ldl -lposix4

#
# HP-UX
# (I used f2c to compile pgplot, which makes things a bit difficult afterwards...)
#
#FLN	  = gcc
#SHLIBX  = sl
#LDFLAGS = -L/lib/pa1.1 -L/usr/lib/X11R5 -L$(FITSDIR) -lfitsio -lm
#LPGPLOT = -L/usr/local/pgplot520 -lcpgplot -lpgplot -L/usr/local/lib/f2c -lF77 -lI77

##############################################################################
# You shouldn't need to change anything below this line
##############################################################################
#
# "Top-level" targets, i.e. something to make while in toplevel dir
#
all:
	for d in fitsio Movie Speckle binfit tcl-rk ; do \
		cd $$d ; $(MAKE) -k suball ; echo ----- ; cd .. ; done

install:
	if [ ! -d Masks ]; then mkdir Masks; fi
	if [ ! -d Flats ]; then mkdir Flats; fi
	for d in fitsio Movie Speckle binfit tcl-rk ; do \
		cd $$d ; $(MAKE) subinstall; cd .. ; done

allclean:
	for d in fitsio Movie Speckle binfit tcl-rk ; do \
		cd $$d ; $(MAKE) clean; cd .. ; done

##############################################################################
#
# pattern rules
#
# How to install a program in BINDIR:
$(BINDIR)/%:	%
	cp -p $? $(BINDIR)

# How to link a program (with libfitsio.sx):
%:	%.o $(FITSDIR)/libfitsio.$(SHLIBX)
	$(LN) -o $@ $< $(LDFLAGS)
	strip $@

##############################################################################
#
# Rule for making of libfitsio.sx:
# (Not needed as long as fitsio is entered first in "make all"
#$(FITSDIR)/libfitsio.$(SHLIBX): $(FITSDIR)
#	(cd $(FITSDIR); $(MAKE) libfitsio.$(SHLIBX))
#
##############################################################################
#
# Standard targets
#
clean:
	-rm *.o *.$(SHLIBX) *~ core 2>/dev/null

tar:
	cd ..; tar -c -v -z -f Binary-Speckle.tar.gz -T Binary-Speckle/TOC

##############################################################################
