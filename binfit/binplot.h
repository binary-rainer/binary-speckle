/* $Id$
 * $Log$
 * Created:     Sat May 24 19:28:29 1997 by Koehler@Lisa
 * Last change: Sun May 12 01:55:23 2002
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include <float.h>
#include <math.h>

#include <cpgplot.h>

#include "fitsio.h"
#include "fit.h"


/** Functions in binplot.c **/

extern double atoangle( char *str);
extern char *FilenamePlus(char *templ);
extern short ReadFrame(char *FileName, struct FitsFile **ffp);
extern short ReadFitFile(char *fname);
extern void WriteBff(char *bff_out);
extern short Plot(void);
extern void Hardcopy(char *fname);
extern void WriteDatafiles(void);


/** Functions in binplotGUI.c **/

extern short InitGUI(void);
extern char *SetDoubleVar(char *fmt, double var, char *tcl_name);


/** sonstige Functions **/
void Projektion( struct FitsFile *ValFits, struct FitsFile *DValFits,
		 double *v, double *dv, short *npp, double *psoll,
		 double pangle, short rmin, short rmax, short stripw );


/** Variables **/

extern struct FitsFile *VisFits, *PhaFits, *MaskFits;
extern struct FitsFile *DVisFits,*DPhaFits;

extern char *VisName, *PhaName, *Title, *MaskName;
extern char *DVisName,*DPhaName;

extern struct FitsFile VisModel, PhaModel;

extern short naxis1, naxis2;

extern int   rmin, rmax, rfit, stripw;
extern float pangle, visfak;
extern double vismin,vismax, phamin,phamax;

extern Tcl_Interp *tclip;
