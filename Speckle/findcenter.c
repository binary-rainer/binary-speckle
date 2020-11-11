/* $Id$
 * $Log$
 * Created:     Sun Nov  5 18:08:00 2000 by Koehler@cathy
 * Last change: Sun Nov  5 18:24:40 2000
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

char *version = "findcenter, compiled on " __DATE__
		 " ============================================\n";


int main( int argc, char **argv)
{
	char *filenm;
	struct FitsFile *ff;
	long  framesz, x,y;
	int   centx, centy, res;
	double sum,sumx,sumy, help;

  if( argc<=1) { printf(version);  return 1; }
  if( argv[1][0]=='-') {
	if( argv[1][1]=='v')  printf(version);
	if( argv[1][1]=='h' || argv[1][1]=='?')
		printf("USAGE: %s filename\n",argv[0]);
	return 0;
  }

  filenm= argv[1];
  centx = centy = -1;
  if( !(ff= FitsOpenR(filenm)))
  {	fprintf(stderr,"Can't open %s\a\n",filenm);	res= 2;  }
  framesz= ff->n1 * ff->n2;

  if( !(ff->Buffer.d= AllocBuffer(framesz*sizeof(double),filenm)))
  {	res = 3;	}
  else
  { if( FitsReadDoubleBuf( ff, ff->Buffer.d, framesz) < framesz)
    {	fprintf(stderr," Error while reading %s\n",filenm);  res= 4;	}
    else
    {	sum= sumx= sumy= 0.0;
	for( x=0;  x<ff->n1;  ++x)
	  for( y=0;  y<ff->n2;  ++y)
	  { help= ff->Buffer.d[x+y*ff->n1];
	    sum  += help;
	    sumx += (double)x * help;
	    sumy += (double)y * help;
	  }

	centx= (short)(sumx/sum + 0.5); 	/** 1.Pixel = (0,0) **/
	centy= (short)(sumy/sum + 0.5);
	res = 0;
    }
  }
  FitsClose(ff);	/** frees ff->Buffer, too **/

  printf("%d %d\n",centx,centy);
  return res;
}
