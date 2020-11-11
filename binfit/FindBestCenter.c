/* $Id$
 * $Log$
 * Created:     Sun Aug  3 19:30:34 1997 by Koehler@Lisa
 * Last change: Fri Aug 22 13:41:51 1997
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
#include "fit.h"

#undef SMOOTH

#ifdef SMOOTH
/********************************************************************************/
/* PrepSmoothTable(size_x,size_y)						*/
/* creates array contaning coordinates of all points, sorted by radius		*/
/********************************************************************************/

struct Point {	int r,x,y;  } *smtbl;

long tablesz;

static int cmp_point( const struct Point *p1, const struct Point *p2)
{
	if( p1->r > p2->r )	return 1;
	if( p1->r < p2->r )	return -1;
	return 0;
}

short PrepSmoothTables(long sizex, long sizey)
{
	long sizexh,sizeyh;
	long x,y,i;

  sizexh= sizex/2 + 1;	sizeyh= sizey/2 + 1;	tablesz= sizexh*sizeyh;
  if( !(smtbl= AllocBuffer(tablesz*sizeof(struct Point),"table of indices")))	return 1;

  /** erst mal alle radien ausrechnen **/
  i=0;
  for( x=0;  x<sizexh;  ++x)
    for( y=0;  y<sizeyh;  ++y)
    {	smtbl[i].x= x;
	smtbl[i].y= y;
	smtbl[i].r= x*x+y*y;	++i;
    }
  /** nach aufsteigendem radius sortieren **/
  qsort( smtbl, tablesz, sizeof(struct Point), (void *)cmp_point);
  return 0;
}

/********************************************************************************/
/* SmoothPhase(buffer,size_x,size_y)						*/
/* geht von innen nach aussen durch das Bild und buegelt Spruenge um 2pi aus	*/
/********************************************************************************/

#define BUFF(x,y)	(buffer[y*sizex + x])
#define BOXR		2
#define SQR(a) ((sqrarg=(a))==0 ? 0L : sqrarg*sqrarg)

void SmoothPhasePnt( double *buffer, long sizex, long sizey, long x, long y, long rq)
{
	double val;
	long cnt, xmid,ymid, xx,yy, sqrarg;

  if( x<0 || x>=sizex || y<0 || y>=sizey)  return;	/** out of area **/

  cnt= 0;  val= 0.0;	xmid= sizex/2;	ymid= sizey/2;

  if( (yy=y-BOXR) < 0)  yy=0;
  for( ;  yy<=y+BOXR && yy<sizey;  ++yy)	/** je BOXR Punkte oben und unten **/
  {
    if( (xx=x-BOXR) < 0)	xx=0;
    for( ;  xx<=x+BOXR && xx<sizex;  ++xx)	/** je BOXR Punkte links und rechts **/
    {
	if( SQR(xx-xmid) + SQR(yy-ymid) < rq)
	{	val += BUFF(xx,yy);	cnt++;	}
    }
  }
  val /= cnt;

  while( BUFF(x,y) < val-PI)	BUFF(x,y) += ZWEIPI;
  while( BUFF(x,y) > val+PI)	BUFF(x,y) -= ZWEIPI;
}

void SmoothPhase( double *buffer, long sizex, long sizey)
{
	long xmid,ymid, x,y, k;

  PrepSmoothTables(sizex,sizey);

/** wir gehen alle Punkte durch und sehen nach, ob's einen Sprung gibt **/
	/** ohne Punkte (0,0)=0, (1,0)=1, (0,1)=2, (-1,0)=3, (0,-1)=4 **/
  xmid= sizex/2;	ymid= sizey/2;

  for( k=5;  k<tablesz;  ++k)	/** alle pkte mit inneren Nachbarn vergleichen (average) **/
  {	x= smtbl[k].x;	y= smtbl[k].y;
	SmoothPhasePnt( buffer,sizex,sizey, xmid+x, ymid+y, smtbl[k].r );
	SmoothPhasePnt( buffer,sizex,sizey, xmid+x, ymid-y, smtbl[k].r );
	SmoothPhasePnt( buffer,sizex,sizey, xmid-x, ymid+y, smtbl[k].r );
	SmoothPhasePnt( buffer,sizex,sizey, xmid-x, ymid-y, smtbl[k].r );
  }
  free(smtbl);
}
#endif

/******************************************************************************/

void FindBestCenter(double *PhaBuf, double *DPhaBuf,
		    double *MPhaBuf, short *MaskBuf, long sizex, long sizey, long rmax)
{
	double SumAbweichU, SumAbweichV, SumUq, SumVq, val, abw, dpha;
	double xsp, ysp;
	long halfx= sizex/2, halfy= sizey/2;
	long u,v, offs, rad_quad= rmax * rmax;

#ifdef SMOOTH
  SmoothPhase(MPhaBuf,sizex,sizey);
#endif

  if( Modell[CE_DIST] != 0.0)
  {	val= RAD(Modell[CE_ANGLE]);
	xsp= Modell[CE_DIST] * cos(val);
	ysp= Modell[CE_DIST] * sin(val);
  }
  else{	xsp= ysp= 0.0;	}

  SumAbweichU= SumAbweichV= SumUq= SumVq= 0.0;
  if( (v= -rmax) < -halfy)  v= -halfy;
  for( ;  v<=rmax && v<halfy;  ++v)		/** Mitte ist 0	**/
  {
     if( (u= -rmax) < -halfx)  u= -halfx;
     offs= (u+halfx) + sizex*(v+halfy);
     for( ;  u<=rmax && u<halfx;  ++u, ++offs)	/** Mitte ist 0 **/
     {
	if( u*u + v*v <= rad_quad && (MaskBuf==NULL || MaskBuf[offs]==0))
	{
	  if(DPhaBuf)
	  {	if((dpha= DPhaBuf[offs]) < 0.001)  dpha= 0.001;		}
	  else	dpha= 0.01;
	  abw = (PhaBuf[offs] - MPhaBuf[offs]) / dpha;

	  val = (double)u/dpha;  SumAbweichU += val*abw;  SumUq += val*val;
	  val = (double)v/dpha;  SumAbweichV += val*abw;  SumVq += val*val;
	}
     }
  }
  xsp -= (double) sizex / ZWEIPI * SumAbweichU / SumUq;
  ysp -= (double) sizey / ZWEIPI * SumAbweichV / SumVq;

#ifdef DEBUG
  fprintf(stderr,"Center: x = %g, y = %g\n",xsp,ysp);
#endif

  Modell[CE_ANGLE]= DEG(atan2(ysp,xsp));
  if( Modell[CE_ANGLE] < 0.0)	Modell[CE_ANGLE] += 360.0;

  Modell[CE_DIST] = sqrt(xsp*xsp + ysp*ysp);
}
