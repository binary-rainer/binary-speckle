/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:48:21 1997 by Koehler@Lisa
 * Last change: Sat Jun 28 16:48:35 1997
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsio.h"


/********************************************************
 * FitsWordSize(FitsFile)				*
 * returns size (in bytes) of one pixel in FitsFile	*
 ********************************************************/

short FitsWordSize(struct FitsFile *Fits)
{
  switch( Fits->bitpix)
  {	case 8:	  return sizeof(unsigned char);
	case 16:  return sizeof(short);
	case 32:  return sizeof(long);
	case -32: return sizeof(float);
	case -64: return sizeof(double);
	default:  fprintf(stderr,"unknown BITPIX-Value %d\n",Fits->bitpix);
  }
  return 0;
}
