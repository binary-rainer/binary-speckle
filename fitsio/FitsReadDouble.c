/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:50:43 1997 by Koehler@Lisa
 * Last change: Tue Apr 17 12:39:51 2001
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <sys/types.h>
#ifdef __sun
/**# include <sys/byteorder.h>**/
#define ntohl(x)	(x)
#define ntohs(x)	(x)
#else
# include <netinet/in.h>
#endif
#include "fitsio.h"

/****************************************************************
 * FitsReadDouble(FitsFile)					*
 * Reads the next pixel value out of FitsFile and returns the	*
 * real	pixel value as double					*
 * (bscale and bzero are applied by ReadDblPixel)		*
 ****************************************************************/

double FitsReadDouble(struct FitsFile *ff)
{
	double val;

  val = (*ff->ReadDblPixel)(ff);

  if( feof(ff->fp))
  {	fprintf(stderr,"\n End of file while reading Fits-file!\a\n");
	ff->flags |= FITS_FLG_EOF;
  }
  if( ferror(ff->fp))
  {	fprintf(stderr,"\n Error while reading Fits-file!\a\n");
	ff->flags |= FITS_FLG_ERROR;
  }
  return val;
}
