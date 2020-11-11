/* $Id$
 * $Log$
 * Created:     Wed Jun 18 19:04:38 1997 by Koehler@Lisa
 * Last change: Wed Aug  6 10:22:50 1997
 *
 * This file is Hellware!
 *
 * Projektion(): projiziert 2-dim. Array auf 1-dim. Achse.
 *		 Auf Wunsch auch mit Fehlern bei Input und Output
 *
 * Aufruf:
 * Projektion(ValFits,DValFits,v,dv,npp,psoll,pangle,rmin,rmax,stripw );
 *
 * struct FitsFile *ValFits:  2D Frame mit den Daten in ValFits->Buffer.d
 * struct FitsFile *DValFits: 2D Frame mit den Fehlern der Daten in
 *				DValFits->Buffer.d (oder NULL).
 * double *v:	1D Array, in das die projizierten 1D Daten sollen
 * double *dv:	1D Array, in das die Fehler der proj. Daten sollen
 *		die Fehler werden entweder via Fehlerfortpflanzung aus
 *		den Fehlern der Eingabedaten oder als Standardabweichung
 *		des Mittelwertes senkrecht zur Proj.-Achse berechnet.
 * short *npp:	1D Array, indem hinterher steht, wieviele Punkte jeweils
 *		auf einen Punkt projiziert wurden.
 * double *psoll: Wenn Phasen projiziert werden, sollte das ein Pointer
 *		auf ein ebenso grosses Array wie v sein.  Das wird dafuer
 *		verwendet, sicherzustellen, dass die Differenzen der fuer
 *		einen Punkt gemittelten Werte nicht groesser als 2pi werden.
 *		Wenn keine Phasen projiziert werden, hier NULL uebergeben.
 * double pangle: Winkel zwischen x-Achse und der Achse, auf die projiziert
 *		  wird (in Radian!)
 * short rmin, rmax:
 *		minmaler/maximaler Abstand der projizierten Punkte zum Zentrum
 * short stripw: Streifenbreite, d.h. maximaler Abstand der projizierten
 *		 Punkte von dr Achse, auf die projiziert wird.
 *		 Wenn keine Begrenzung gewuenscht, stripw = 0 setzen!
 *
 * dazu noch eine globale Variable:
 * struct FitsFile *MaskFits:
 *	Wenn != NULL, ein Frame, in dem Pixel, die ignoriert werden sollen,
 *	durch Werte != 0 markiert sind.  Die Daten muessen als short vorliegen,
 *	der Pointer auf das Array muss in MaskFits->Buffer.s stehen.
 *
 * Nullpunkt des projizierten Arrays ist beim offset rmax+1
 *	=> 1D Arrays muessen mindestens 2*rmax+1 gross sein
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "fitsio.h"

#define PI	3.14159265359
#define ZWEIPI	6.28318530718


extern struct FitsFile *MaskFits;


static double force_phase_range( double val, double soll)
{
	while( val >  soll+PI)	val -= ZWEIPI;
	while( val <= soll-PI)	val += ZWEIPI;
	return val;
}

void Projektion( struct FitsFile *ValFits, struct FitsFile *DValFits,
		 double *v, double *dv, short *npp, double *psoll,
		 double pangle, short rmin, short rmax, short stripw )
{
	double	cosw, sinw, val;
	long	x,y,rq,offs, offs2D, midx,midy, rminq,rmaxq, omax;
	short	sizex= ValFits->n1, sizey= ValFits->n2;

  cosw = cos(pangle);	sinw = sin(pangle);
  midx = sizex/2;	midy = sizey/2;
  rminq= rmin*rmin;	rmaxq= rmax*rmax;	omax= 2*rmax+1;

  for( offs=0;  offs<=omax;  ++offs)
  {	v[offs]= dv[offs]= 0.0;
	npp[offs]=0;

	if(psoll)
	{ rq= offs-(rmax+1);
	  x= (long) floor((double)rq * cosw + 0.5);
	  y= (long) floor((double)rq * sinw + 0.5);
	  offs2D= (x+midx) + (y+midy)*sizex;
	  rq *= rq;

	  if( rq>=rminq && rq<=rmaxq && (!MaskFits || MaskFits->Buffer.s[offs2D] <= 1))
		psoll[offs]= ValFits->Buffer.d[offs2D];
	  else	psoll[offs]= 0.0;
	}
  }
/** Achtung: x und y sind relativ zur MITTE! **/

  y= -rmax;	if( y<-midy)  y= -midy;
  for( ;  y<midy && y<=rmax;  ++y)
  {
    x= -rmax;	if( x<-midx)  x= -midx;
    for( ;  x<midx && x<=rmax;  ++x)
    {
	rq= x*x + y*y;
	offs2D= (x+midx) + (y+midy)*sizex;

	if( rq>=rminq && rq<=rmaxq && (!MaskFits || MaskFits->Buffer.s[offs2D] <= 1))
	{							/** im erlaubten Bereich **/
	  if(stripw)			/** Christoph will nen begrenzten Streifen **/
	  {	offs= (long) floor((double)y * cosw - (double)x * sinw + 0.5);
		/** ^Abstand zur Projektionsachse **/
		if( offs > stripw || offs <-stripw)	continue;	/** next pnt **/
	  }
	  offs= rmax+1 + (long)floor((double)x * cosw + (double)y * sinw + 0.5);

	  if( offs>=0 && offs<=omax)
	  {	val= ValFits->Buffer.d[offs2D];
		if(psoll)  val= force_phase_range(val,psoll[offs]);
		v[offs] += val;

		if(DValFits)  val= DValFits->Buffer.d[offs2D];	/** delta given **/
		dv[offs] += val*val;
		npp[offs]++;
	  }
	}
     }
  }
  /** ganzes Array aufsummiert, jetzt berechne MW und StdAbw **/
  /** Note: v[0] meaningful, even if rmin>0 **/

  for( offs=0;  offs<=omax;  ++offs)
   if( npp[offs])
   {	val= (double) npp[offs];
	v[offs] /= val;		/** Mean = Sum/N **/

	if(DValFits)		/** delta given **/
	{	dv[offs]= sqrt( (npp[offs]>1) ? (dv[offs] / (val*(val-1.0))) : dv[offs]);  }
	else
	{  if( npp[offs]>1)
	   {	val= (dv[offs]/val - v[offs]*v[offs]) / (val-1.0);
		dv[offs]= (val>0.0) ? sqrt(val) : 0.0;
	   }	/** durch Rundungsfehler gibt's manchmal was <0 statt ==0 **/
	   else	dv[offs]= 1.0;
	}
   }
}
