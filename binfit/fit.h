/* $Id: fit.h,v 1.1 1996/10/31 09:45:06 koehler Exp koehler $
 * $Log: fit.h,v $
 * Revision 1.1  1996/10/31  09:45:06  koehler
 * Initial revision
 *
 * Created:     Sat Sep 21 18:44:29 1996 by Koehler@Lisa
 * Last change: Tue Jul  9 19:06:51 2002
 *
 * This file is Hellware!
 * If You find it useful and decide to use it (in whole or in parts),
 * You are required to give me your soul!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#ifndef PI
# define PI	3.14159265359
#endif
#define ZWEIPI	6.28318530718

#define RAD(d)	((d) * (PI/180.0))
#define DEG(d)	((d) * (180.0/PI))


#define MAX_COMP	2	/** Max. # of companions **/

#define CE_ANGLE	0	/** Schwerpunkt Winkel	**/
#define CE_DIST		1	/**      ''	Abstand **/
#define ANGLE1		2	/** 1. Begleiter Winkel	 **/
#define DIST1		3	/** 1.	  ''	 Abstand **/
#define BRGTN1		4	/** 1.	  ''	 Hell.-Verh. **/
/** 2. Begleiter Winkel = [ANGLE1+3] **/
#define MAX_PARAM	(ANGLE1+3*MAX_COMP)

#define CE_POSX	CE_ANGLE
#define CE_POSY	CE_DIST
#define POSX1	ANGLE1
#define POSY1	DIST1


struct Range
{	double	min,max;
	short	npnts;
	double	step,prec;
};

extern struct Range Ranges[MAX_PARAM];

extern short verbose_level;
extern short N_comp, N_param, RefIsMult;
extern double Modell[MAX_PARAM];
extern double PrimFlux;		/** flux of primary component **/

extern void FT_Modell( double *VisBuf, double *PhaBuf, long sizex, long sizey, long rad);
extern double FindVisFak(double *VisBuf, double *DVisBuf, double *MVisBuf,
			 short *MaskBuf, long naxis1, long naxis2, long rmin, long rmax);
extern void FindBestCenter(double *PhaBuf, double *DPhaBuf, double *MPhaBuf,
				short *MaskBuf,	long sizex, long sizey, long rmax);
extern void CalcErrors( double *evis, double *epha);
extern short ReadParaFile(char *fname);
extern void WriteParaFile(short all_done);
extern void WriteFitten(void);
