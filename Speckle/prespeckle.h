/* $Id$
 * $Log$
 * Created:     Sat Jul 26 17:09:17 1997 by Koehler@Lisa
 * Last change: Fri Jan 25 14:20:12 2002
 *
 * Header-File for prespeckle
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "fitsio.h"


extern struct FitsFile *Fits;
extern char histline[80];

extern int Verbose, DefBitpix;

extern void CreateBadpixmask(char *opts);
extern void CorrectBadpix(char *opts);
extern int  FourierTrafo( char *opts);
extern void expand_frame(double *bild, long dimm, long dimn, long M, long N);
