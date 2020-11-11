/* $Id$
 * $Log$
 * Created:     Sun Aug  3 19:04:26 1997 by Koehler@Lisa
 * Last change: Sun Jun 18 12:48:01 2000
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include "fit.h"
#include <float.h>

/********************************************************************************
 * Theorie zum Visibility-Faktor:						*
 * Wir moechten:								*
 *	  / Model_i - VF*Data_i \ 2          /  Model_i       Data_i \ 2  	*
 * Sum_i | --------------------- |  = Sum_i | ----------- - --------- |  = min	*
 *	  \	 VF*Sigma_i	/            \ VF*Sigma_i    Sigma_i /     	*
 *										*
 * d/dVF =>									*
 *	      / Model_i - VF*Data_i \     d   /  Model_i       Data_i \	  	*
 * Sum_i 2 * | --------------------- | * --- | ----------- - --------- |	*
 *	      \	    VF * Sigma_i    /    dVF  \ VF*Sigma_i    Sigma_i /     	*
 *										*
 *		/ Model_i - VF*Data_i \       /    Model_i     \  !		*
 * = 2 * Sum_i | --------------------- | * - | ---------------- | = 0.0		*
 *		\	VF*Sigma_i    /       \ VF^2 * Sigma_i /		*
 *										*
 * * -VF^3/2:									*
 *		/ Model_i - VF*Data_i \     / Model_i \  !			*
 *	 Sum_i | --------------------- | * | --------- | = 0.0			*
 *		\	Sigma_i	      /     \ Sigma_i /				*
 *										*
 *		/ Model_i \ 2			/ Data_i * Model_i \		*
 *	 Sum_i | --------- |	=   VF * Sum_i | ------------------ |		*
 *		\ Sigma_i /			\     Sigma_i^2    /		*
 *										*
 *		Sum_i (Model_i / Sigma_i)^2					*
 * =>	VF = ------------------------------------				*
 *	      Sum_i (Data_i*Model_i / Sigma_i^2)				*
 *										*
 ********************************************************************************/

double FindVisFak(double *VisBuf, double *DVisBuf, double *MVisBuf,
		  short *MaskBuf, long naxis1, long naxis2, long rmin, long rmax)
{
	double SumMM=0.0, SumDM=0.0, DVis2;
	long halfx= naxis1/2, halfy= naxis2/2, rmin_q= rmin*rmin, rmax_q= rmax*rmax;
	long xc,yc, q, i;

  if( (yc= -rmax) < -halfy)	yc= -halfy;
  for( ;  yc <= rmax && yc < halfy;  ++yc)
  {
    if( (xc= -rmax) < -halfx)	xc= -halfx;
    i= (xc+halfx) + naxis1 * (yc+halfy);

    for( ;  xc <= rmax && xc < halfx;  ++xc, ++i)
    {
	q= xc*xc + yc*yc;
	if( (q >= rmin_q) && (q <= rmax_q) && (MaskBuf==NULL || MaskBuf[i]==0))
	{
	  DVis2= DVisBuf? DVisBuf[i] : 0.01;	DVis2 *= DVis2;
	  SumMM += MVisBuf[i] * MVisBuf[i] / DVis2;
	  SumDM +=  VisBuf[i] * MVisBuf[i] / DVis2;
	}
    }
  }
#ifdef DEBUG
	fprintf(stderr,"VF ");
#endif
  return SumMM / SumDM;
}
