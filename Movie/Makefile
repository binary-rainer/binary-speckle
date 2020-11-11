#
# Makefile for tkfmovie
#
# Last Change: Tue Jul 16 18:43:00 2002

PRGS	= tkfmovie movish

include ../Makefile

CFLAGS += $(ITCLTK) $(IPGPLOT)

#
# Standard targets
#
suball:	${PRGS}

subinstall: $(addprefix $(BINDIR)/,$(PRGS))

#
# Real programs
#
movish:	movish.o color.o pgtcl.o $(FITSDIR)/libfitsio.$(SHLIBX)
	$(FLN) -o movish movish.o color.o pgtcl.o $(LTCLTK) $(LPGPLOT) $(LDFLAGS)


tkfmovie:	movish


$(BINDIR)/tkfmovie:	tkfmovie
	sed -e "s,@@PATH_TO_MOVISH@@,$(BINDIR),"\
	    -e "s,@@PATH_TO_TCL_RK@@,$(BINDIR)/tcl-rk,"\
	    -e "s,@@PATH_TO_DOCS@@,$(BINDIR)/Doc," tkfmovie > $(BINDIR)/tkfmovie
	chmod a+x $(BINDIR)/tkfmovie
