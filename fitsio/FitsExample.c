/* $Id$
 * $Log$
 * Created:     Thu Nov 19 19:23:05 1998 by Koehler@hex
 * Last change: Fri Nov 20 13:03:37 1998
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

int main( int argc, char **argv)
{
	struct FitsFile *InFits;
	float *Buffer;
	double min, max;
	long framesz, i;

  fprintf(stderr,"\nFits Example, compiled on "__DATE__" =================================\n");

  if( !(InFits= FitsOpenR( argv[1])))	return 10;
  framesz= InFits->n1 * InFits->n2;

  fprintf(stderr,"Opened %s\nBITPIX = %d, %d x %d pixels\n",
	argv[1], InFits->bitpix, InFits->n1, InFits->n2 );

  if( !(Buffer= AllocBuffer( framesz*sizeof(float),"fits buffer")))	return 20;

  for( i=0;  i<framesz;  ++i)
  {
	Buffer[i]= FitsReadDouble(InFits);
  }
  FindMinMax(Buffer, BITPIX_FLOAT, framesz, &min, &max);

  printf("P5\n%d %d 255\n", InFits->n1, InFits->n2);
  for( i=0;  i<framesz;  ++i)
  {
	putchar((unsigned char)((Buffer[i] - min) * 255.0 / (max-min)));
  }
  FitsClose(InFits);
  free(Buffer);
  return 0;
}
