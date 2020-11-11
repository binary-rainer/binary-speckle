/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:51:51 1997 by Koehler@Lisa
 * Last change: Wed Apr 25 11:06:43 2001
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

/****************************************************************
 * FitsReadDoubleBuf(FitsFile,buffer,cnt)			*
 * Reads the next "cnt" pixel values out of FitsFile and stores	*
 * the real pixel values as doubles in buffer.			*
 * returns number of pixels successfully read.			*
 ****************************************************************/

/* currently quite simple implementation, this may change in the future */

long FitsReadDoubleBuf( struct FitsFile *ff, double *buf, long cnt)
{	long i;

  ff->flags &= ~(FITS_FLG_EOF|FITS_FLG_ERROR);
  for( i=0;  i<cnt;  ++i)
  {
	*buf++ = (*ff->ReadDblPixel)(ff);

	if( feof(ff->fp))
	{	fprintf(stderr,"\n End of file while reading Fits-file!\a\n");
		ff->flags |= FITS_FLG_EOF;
		return i;
	}
	if( ferror(ff->fp))
	{	fprintf(stderr,"\n Error while reading Fits-file!\a\n");
		ff->flags |= FITS_FLG_ERROR;
		return i;
	}
  }
  return cnt;
}
