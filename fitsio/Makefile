#
# Makefile for libfitsio.so
#
# Created:     Sat Jun 28 16:41:16 1997 by Koehler@Lisa
# Last change: Mon Nov 20 00:04:42 2000
#
include ../Makefile

ifeq "$(CC)" "gcc"
  CFLAGS += -fpic
else
  CFLAGS += -Kpic
endif


OBJS   = rfopen.o AllocBuffer.o FitsWordSize.o FitsFindKeyw.o\
	 CopyFits.o FitsOpenR.o FitsClose.o LoadFitsFile.o\
	 FitsReadShort.o FitsReadDouble.o FitsReadDoubleBuf.o DoubleOfAnyp.o\
	 FitsMakeSimpleHdr.o FitsSetHdrLine.o FitsFindEmptyHdrLine.o\
	 FitsAddHistKeyw.o FindMinMax.o FitsWriteAny.o


suball:	libfitsio.$(SHLIBX)

subinstall:
# Nothing to do

libfitsio.$(SHLIBX): $(OBJS)
	ar r libfitsio.$(SHLIBX) $(OBJS)
