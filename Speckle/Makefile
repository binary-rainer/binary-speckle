#
# Makefile for Speckle-Programs
#
# Last Change: Sun Mar 17 00:50:32 2002
#

PRGS = mcspeckle mkfitscube.tcl mkfitscube prespeckle fitsmean maskedit findcenter\
	mkflat fits2pnm saa quickvis despeckle

suball:	${PRGS}

include ../Makefile

subinstall: $(addprefix $(BINDIR)/,$(PRGS))

#
# Common dependancies
#
badpixmask.o despeckle.o fits2pnm.o fitsmean.o findcenter.o\
 maskedit.o mkfitscube.o mkflat.o mkgrpfile.o prespeckle.o quickvis.o saa.o :	$(FITSDIR)/fitsio.h

#
# mcspeckle
#
$(BINDIR)/mcspeckle:	mcspeckle
	sed -e "s,@@PATH_TO_WISH@@,$(WISH),"\
	    -e "s,@@PATH_TO_DOCS@@,$(BINDIR)/Doc,"\
	    -e "s,@@SPECKLE_DIR@@,$(BINDIR),"\
	    -e "s,@@GMAKE@@,$(GMAKE),"\
	    -e "s,@@GHOSTVIEW@@,$(GHOSTVIEW)," mcspeckle > $(BINDIR)/mcspeckle
	chmod a+x $(BINDIR)/mcspeckle

#
# prespeckle
#
prespFFT.o:	prespeckle.h prespFFT.c
	$(CC) -c $(CFLAGS) $(IFFTW) prespFFT.c

expand_frame.o prespeckle.o:	prespeckle.h

prespeckle:	prespeckle.o badpixmask.o prespFFT.o expand_frame.o $(FITSDIR)/libfitsio.$(SHLIBX)
	if [ -f prespeckle ]; then chmod u+w prespeckle; fi
	$(LN) -o prespeckle prespeckle.o badpixmask.o prespFFT.o expand_frame.o $(LFFTW) $(LDFLAGS)
	strip prespeckle

#
# holospeck
#
holospeck:	holospeck.o expand_frame.o
	if [ -f holospeck ]; then chmod u+w holospeck; fi
	$(LN) -o holospeck holospeck.o expand_frame.o $(LFFTW) $(LDFLAGS)
	strip holospeck

#
# mkfitscube
#
$(BINDIR)/mkfitscube.tcl:	mkfitscube.tcl
	sed -e "s,@@PATH_TO_WISH@@,$(WISH),"\
	    -e "s,@@SPECKLE_DIR@@,$(BINDIR)," mkfitscube.tcl > $(BINDIR)/mkfitscube.tcl
	chmod a+x $(BINDIR)/mkfitscube.tcl
