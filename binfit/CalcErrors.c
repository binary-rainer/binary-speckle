/* $Id$
 * $Log$
 * Created:     Thu Aug  6 12:00:52 1998 by Koehler@cathy
 * Last change: Tue Aug 22 21:52:58 2000
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include "fit.h"
#ifdef __sun
# include <ieeefp.h>	/** for finite() **/
#endif

extern struct FitsFile *VisFits, *PhaFits;
extern double *VisBuf, *DVisBuf, *PhaBuf, *DPhaBuf, *MVisBuf,*MPhaBuf, *EVisBuf,*EPhaBuf;
extern short  *MaskBuf;
extern double VisFak;
extern long rad_min, radius, naxis1, naxis2;

void CalcErrors( double *errvis, double *errpha)
{
	double cnt, val;
	long halfx= naxis1/2, halfy= naxis2/2,
	     rmin_q= rad_min*rad_min, rmax_q= radius*radius;
	long x,y, xc,yc, q, i;

  *errvis= *errpha= cnt= 0.0;

  if( (x= halfx-radius) < 0)	x=0;
  for( ;  x <= halfx+radius && x < naxis1;  ++x)
  {	xc= x-halfx;

    if( (y= halfy-radius) < 0)	y=0;
    for( ;  y <= halfy+radius && y < naxis2;  ++y)
    {	yc= y-halfy;
	q = xc*xc + yc*yc;

      if( (q >= rmin_q) && (q <= rmax_q))
      {	i= x + naxis1 * y;
	if( MaskBuf==NULL || MaskBuf[i]==0)
	{
	  if( VisFits)
	  {	val= (MVisBuf[i] / VisFak - VisBuf[i]) / DVisBuf[i];
		*errvis += val * val;
		if( EVisBuf)	EVisBuf[i]= val;
	  }
	  if( PhaFits)
 	  {	if(finite(DPhaBuf[i]))
		{ val= MPhaBuf[i] - PhaBuf[i];
		  while( val >  PI)  val -= ZWEIPI;
		  while( val < -PI)  val += ZWEIPI;
 		  val /= DPhaBuf[i];
		  *errpha += val * val;
		}
		else { val = 0; }
		if( EPhaBuf)	EPhaBuf[i]= val;
	  }
	  cnt += 1.0;
	}
      }
    }
  }
  *errvis /= cnt;	*errpha /= cnt;

#ifdef DEBUG
	fprintf(stderr,"Errs: %f, %f, %f pnts\n",*errvis,*errpha,cnt);
#endif
}
