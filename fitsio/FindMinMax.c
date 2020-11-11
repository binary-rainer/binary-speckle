/* $Id$
 * $Log$
 * Created:     Sat Jun 28 17:04:30 1997 by Koehler@Lisa
 * Last change: Sat May 30 00:04:10 1998
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#ifdef __sparc
# include <ieeefp.h>	/** for finite() **/
#endif
#include "fitsio.h"

/****************************************************************
 * FindMinMax(buffer,bitpix,size,min,max)			*
 * Searches for minimum and maximum value in buffer and	leaves	*
 * the result in the variables pointed to by min and max	*
 ****************************************************************/

void FindMinMax( union anypntr buff, short bitpix,
			long size, double *min, double *max)
{	double val;
	long i, infcnt=0;

  *min= DBL_MAX;	*max= -DBL_MAX;
  for( i=0;  i<size;  ++i)
  {	val= DoubleOfAnyp( buff,bitpix,i);
	if( finite(val))
	{	if( *min > val)	 *min= val;
		if( *max < val)	 *max= val;
	}
	else	infcnt++;
  }
  if(infcnt)
	fprintf(stderr," %ld pixels with infinite value encountered!\a\n",infcnt);
}
