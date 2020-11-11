/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:52:33 1997 by Koehler@Lisa
 * Last change: Sat Jun 28 16:52:42 1997
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
 * DoubleOfAnyp(ptr,bitpix,offs)				*
 * Returns pixel value at offset "offs" in buffer pointed to by	*
 * "ptr" as double.  "bitpix" gives type of data in buffer	*
 ****************************************************************/

double DoubleOfAnyp( union anypntr ptr, short bitpix, long offs)
{
  switch(bitpix)
  {	case 8:	  return (double) ptr.c[offs];
	case 16:  return (double) ptr.s[offs];
	case 32:  return (double) ptr.l[offs];
	case -32: return (double) ptr.f[offs];
	case -64: return (double) ptr.d[offs];
	default:  fprintf(stderr," unknown bitpix-value %d\n",bitpix);
  }
  return 0.0;
}
