#
# Makefile for binplot, maxbright et al.
#
# Last Change: Wed Aug 14 17:32:38 2002
#
#############################################################################
#
# The following lines were used with the Numerical Recipes version of amoeba.
# Believe me, you do not want to use that!
#
##AMOBJS = amoeba.o amotry.o	# uncomment if you have amotry.c
##AMOBJS = amoeba.o		# uncomment if you don't have amotry.c
#
#############################################################################

PRGS	= amf bff binwish mkbffs maxbrwish maxbright

FITOBJS	= amf.o bff.o binplot.o bintens.o \
	  CalcErrors.o FindBestCenter.o FindVisFak.o ReadWrtBff.o WriteFitten.o modell.o

suball:	${PRGS}

include ../Makefile

subinstall: $(addprefix $(BINDIR)/,$(PRGS)) $(BINDIR)/binplot

CFLAGS += $(ITCLTK) $(IPGPLOT)

##############################################################################
#
# Subroutines etc.
#
modell.o:	modell.c fit.h

##############################################################################
#
# Common Dependancies
#
$(FITOBJS) maxbrwish.o:	$(FITSDIR)/fitsio.h

$(FITOBJS) :	fit.h


##############################################################################
#
# Fit programs
#

ifdef AMOBJS
amoeba.o:	amoeba.c
	  $(CC) -c $(CFLAGS) -Wno-strict-prototypes amoeba.c

amf:	amf.o nrutil.o $(AMOBJS) modellxy.o\
    FindVisFak.o FindBestCenter.o CalcErrors.o ReadWrtBff.o WriteFitten.o $(FITSDIR)/libfitsio.$(SHLIBX)
	$(LN) -o amf amf.o nrutil.o $(AMOBJS) modellxy.o FindVisFak.o FindBestCenter.o CalcErrors.o ReadWrtBff.o WriteFitten.o $(FITSDIR)/libfitsio.$(SHLIBX) $(LDFLAGS)
	strip amf
else

gnamoeba.o:	gnamoeba.h

amf.o:		gnamoeba.h

amf:	amf.o gnamoeba.o modellxy.o FindVisFak.o FindBestCenter.o\
         CalcErrors.o ReadWrtBff.o WriteFitten.o $(FITSDIR)/libfitsio.$(SHLIBX)
	$(LN) -o amf amf.o gnamoeba.o modellxy.o FindVisFak.o FindBestCenter.o CalcErrors.o ReadWrtBff.o WriteFitten.o $(FITSDIR)/libfitsio.$(SHLIBX) $(LDFLAGS)
	strip amf

endif


bff:	bff.o modell.o\
    FindVisFak.o FindBestCenter.o CalcErrors.o ReadWrtBff.o WriteFitten.o $(FITSDIR)/libfitsio.$(SHLIBX)
	$(LN) -o bff bff.o modell.o FindVisFak.o FindBestCenter.o CalcErrors.o ReadWrtBff.o WriteFitten.o $(FITSDIR)/libfitsio.$(SHLIBX) $(LDFLAGS)
	strip bff


BINPLOTOBJ = binplot.o modell.o projektion.o binplotGUI.o FindVisFak.o FindBestCenter.o

binplot.o:	binplot.c binplot.h
	$(CC) $(CFLAGS) -DGHOSTVIEW='"$(GHOSTVIEW)"' -c -o binplot.o binplot.c

binplotGUI.o:	binplotGUI.c binplot.h
	$(CC) $(CFLAGS) -DGUI_SCRIPT='"$(BINDIR)/binplotGUI.tk"' -c -o binplotGUI.o binplotGUI.c

binwish:	$(BINPLOTOBJ) $(FITSDIR)/libfitsio.$(SHLIBX)
	$(FLN) -o binwish $(BINPLOTOBJ) $(LPGPLOT) $(LTCLTK) $(LDFLAGS)
	strip binwish

$(BINDIR)/binplot:	binplotGUI.tk
	sed -e "s,@@PATH_TO_BINWISH@@,$(BINDIR),"\
	    -e "s,@@PATH_TO_TCL_RK@@,$(BINDIR)/tcl-rk," binplotGUI.tk > $(BINDIR)/binplot
	chmod a+x $(BINDIR)/binplot

$(BINDIR)/mkbffs:	mkbffs
	sed -e "s,@@PATH_TO_WISH@@,$(WISH),"\
	    -e "s,@@PATH_TO_TCL_RK@@,$(BINDIR)/tcl-rk," mkbffs > $(BINDIR)/mkbffs
	chmod a+x $(BINDIR)/mkbffs

#############################################################################
# maxbright
#
maxbrwish:	maxbrwish.o projektion.o
	$(FLN) -o maxbrwish maxbrwish.o projektion.o $(LPGPLOT) $(LTCLTK) $(LDFLAGS)

maxbrightmap:	maxbrightmap.o projektion.o
	$(FLN) -o maxbrightmap maxbrightmap.o projektion.o $(LDFLAGS)


$(BINDIR)/maxbright:	maxbright $(BINDIR)/maxbrwish
	sed -e "s,@@PATH_TO_MAXBRWISH@@,$(BINDIR),"\
	    -e "s,@@PATH_TO_TCL_RK@@,$(BINDIR)/tcl-rk,"\
	    -e "s,@@GHOSTVIEW@@,$(GHOSTVIEW)," maxbright > $(BINDIR)/maxbright
	chmod a+x $(BINDIR)/maxbright
