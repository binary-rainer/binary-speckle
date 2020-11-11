/* $Id: modellxy.c,v 1.1 1996/10/31 09:46:43 koehler Exp koehler $
 * $Log: modellxy.c,v $
 * Revision 1.1  1996/10/31  09:46:43  koehler
 * Initial revision
 *
 * Created:     Sat Sep 21 19:01:04 1996 by Koehler@Lisa
 * Last change: Thu Aug  6 22:08:49 1998
 *
 * This file is Kukieware!
 * If You find it useful and decide to use it (in whole or in parts),
 * You are required to give me some kukies!
 */

#include "fit.h"
#include <float.h>
#include "fitsio.h"


short N_comp= 1, N_param= ANGLE1+3, RefIsMult=0;
double Modell[MAX_PARAM]= { 0.0, 0.0, 42.0, 14.0, 0.7 };
double PrimFlux= 1.0;	/** flux of primary component **/

/******************************************************************************/

void FT_Modell( double *VisBuf, double *PhaBuf, long sizex, long sizey, long rad)
{
	long halfx= sizex/2, halfy= sizey/2;
	long c, i, framesz, ix,iy;
 	double x,y, x0,y0, x1,y1, real,imag, fakx[MAX_COMP],faky[MAX_COMP];
	double rad_quad= (double)(rad * rad), tot_brgtn;

  framesz= sizex * sizey;
  for( i=0;  i<framesz;  ++i)	VisBuf[i]= PhaBuf[i]= 0.0;

  tot_brgtn= 1.0;	/** Hauptkomp. **/
  x= y= 0.0;
  for( c=0;  c<N_comp;  ++c)
  {	i= 3*c;		/** Offset in Modell[] **/

	x1= Modell[POSX1+i];
	y1= Modell[POSY1+i];

	/** die absolute Position der Sterne wirkt sich auf die Visibility **/
	/** gar nicht und auf die Phase nur als linearer Term aus	   **/
	/** => Wir berechnen alles fuer x0=y0=0 und shiften nur die Phase. **/

	/** jetzt rechnen wir aus, wo die H-Komp. absolut ist (bis jetzt noch 0,0) **/

	tot_brgtn += Modell[BRGTN1+i];
	x += x1 * Modell[BRGTN1+i];
	y += y1 * Modell[BRGTN1+i];

	fakx[c]= x1 * ZWEIPI / (double)sizex;
	faky[c]= y1 * ZWEIPI / (double)sizey;
  }
  x /= tot_brgtn;	/** Pos. des Schwerpunkts **/
  y /= tot_brgtn;	/** (relativ zur H.-Komp) **/

	/** Pos. der HK rel zum SP == - Pos des SP rel zur HK	 **/
	/** abs. Pos. der HK == abs. Pos des SP + Pos rel zum SP **/
	/** da die HK bisher bei 0,0 war, ist die abs Pos der HK **/
	/** der Vektor, um den wir das Modell shiften muessen	 **/

  if( !RefIsMult)
  {	x0= Modell[CE_POSX] - x;	/** x0,y0 ist unser */
	y0= Modell[CE_POSY] - y;	/** shift-vector  **/
  }
  else
  {	x0= -Modell[CE_POSX] - x;	/** x0,y0 ist unser */
	y0= -Modell[CE_POSY] - y;	/** shift-vector  **/
  }
  x0 *= ZWEIPI / (double)sizex;
  y0 *= ZWEIPI / (double)sizey;

	/***** Im Folgenden beziehen sich x und y auf den Fourierraum! *****/

  if( (ix= halfx-rad) < 0)  ix=0;
  for( ;  ix <= halfx+rad && ix < sizex;  ++ix)	/** mehr brauchen wir nicht **/
  {	x= (double)(ix - halfx);

    if( (iy= halfy-rad) < 0)  iy=0;
    for( ;  iy <= halfy+rad && iy < sizey;  ++iy)
    {	y= (double)(iy - halfy);

      if( x*x + y*y <= rad_quad )	/** Test fuer Maske sparen wir uns **/
      {	real= PrimFlux;		/** Hauptkomp.: cos(0)=1 **/
	imag= 0.0;		/**	''	sin(0)=0 **/

	for( c=0;  c<N_comp;  ++c)
	{ real += Modell[BRGTN1+3*c] * cos(fakx[c]*x + faky[c]*y);
	  imag -= Modell[BRGTN1+3*c] * sin(fakx[c]*x + faky[c]*y);
	}
	i= ix + sizex*iy;
	VisBuf[i]= sqrt(real * real + imag*imag) / tot_brgtn;
	PhaBuf[i]= atan2(imag,real) - (x0*x + y0*y);

	if(RefIsMult)
	{	VisBuf[i]= 1.0/VisBuf[i];
		PhaBuf[i]= -PhaBuf[i];
	}
      }
    }
  }
}
