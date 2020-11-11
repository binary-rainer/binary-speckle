/* $Id$
 * $Log$
 * Created:     Fri Jan 25 13:51:25 2002 by Koehler@hex.ucsd.edu
 * Last change: Fri Jan 25 14:58:51 2002
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include "prespeckle.h"

/*************************************************************************/
/** expand frame up to given size, creating a seamless transition	**/
/** between left/right and top/bottom edge.				**/
/** This code was written by Norbert Weitzel in the late 20th century	**/
/** All attempts to make it readable failed				**/
/*************************************************************************/

void expand_frame(
		double *bild,		/** der framebuffer **/
		long dimm, long dimn,	/** die Groesse, die's werden soll (m=x, n=y) **/
		long M, long N	)	/** die Groesse, die's schon hat **/
{
	long	deln,delm, delnh,delmh;
	long	n,m,na,nb,ma,mb, lNM2,lNM3;
	float	mit1,mit2,fak,fak2;
	double	pig;	/** Schweine im Ortsraum **/

  pig = 2.0*asin(1.0);

  deln= dimn - N;	delnh= deln / 2;
  delm= dimm - M;	delmh= delm / 2;

  na = delnh;
  nb = N + delnh - 1;
  ma = delmh;
  mb = M + delmh - 1;

  while (((ma > 0) || (mb < (dimm-1))) || ((na > 0) || (nb < (dimn-1))))
  {
     if (ma > 0)
     {  fak = 1.0 - cos((double) ((pig*(delmh-ma))/(delmh+1)));
        for (n=(na+1);n<nb;++n)
        {  lNM2= n*dimm;
           mit1= ( bild[lNM2-dimm+ma] +  bild[lNM2+ma] +  bild[lNM2+dimm+ma])/3.0;
           mit2= ( bild[lNM2-dimm+mb] +  bild[lNM2+mb] +  bild[lNM2+dimm+mb])/3.0;
	   bild[lNM2 + ma - 1] = (2.0*mit1 + fak*mit2)/(2.0 + fak);
        }
     }
     if (mb < (dimm-1))
     {  fak = 1.0 - cos((double) ((pig*(mb - M - delmh + 1))/(delmh+1)));
        for (n=(na+1);n<nb;++n)
        {  lNM2= n*dimm;
           mit1= ( bild[lNM2 -dimm +ma] +  bild[lNM2 + ma] +  bild[lNM2 + dimm + ma])/3.0;
           mit2= ( bild[lNM2 -dimm +mb] +  bild[lNM2 + mb] +  bild[lNM2 + dimm + mb])/3.0;
           bild[lNM2 + mb + 1] = (2.0*mit2 + fak*mit1)/(2.0 + fak);
        }
     }
     if (na > 0)
     {  fak = 1.0 - cos((double) ((pig*(delnh-na))/(delnh+1)));
        lNM2 = na*dimm;
        lNM3 = nb*dimm;
        for (m=(ma+1);m<mb;++m)
        {  mit1 = ( bild[lNM2 + m - 1] +  bild[lNM2 + m] +  bild[lNM2 + m + 1])/3.0;
           mit2 = ( bild[lNM3 + m - 1] +  bild[lNM3 + m] +  bild[lNM3 + m + 1])/3.0;
           bild[lNM2 - dimm + m] = (2.0*mit1 + fak*mit2)/(2.0 + fak);
        }
     }
     if (nb < (dimn-1))
     {  fak = 1.0 - cos((double) ((pig*(nb - N - delnh + 1))/(delnh+1)));
        lNM2 = na*dimm;
        lNM3 = nb*dimm;
        for (m=(ma+1);m<mb;++m)
        {  mit1 = ( bild[lNM2 + m - 1] +  bild[lNM2 + m] +  bild[lNM2 + m + 1])/3.0;
           mit2 = ( bild[lNM3 + m - 1] +  bild[lNM3 + m] +  bild[lNM3 + m + 1])/3.0;
           bild[lNM3 + dimm + m] = (2.0*mit2 + fak*mit1)/(2.0 + fak);
        }
     }

     if (ma > 0)
     { fak = 1.0 - cos((double) ((pig*(delmh-ma))/(delmh+1)));
       fak2= 1.0 - cos((double) ((pig*(nb - N - delnh + 1))/(delnh+1)));
       lNM2= na*dimm;
       lNM3= nb*dimm;
       mit1 = (2.0*( bild[lNM3 - dimm + ma] +  bild[lNM3 + ma]) + bild[lNM2 + ma]*fak2)/(4.0 + fak2);
       mit2 = (2.0*( bild[lNM3 - dimm + mb] +  bild[lNM3 + mb]) + bild[lNM2 + mb]*fak2)/(4.0 + fak2);
       bild[lNM3 + ma - 1] = (2.0*mit1 + fak*mit2)/(2.0 + fak);

       fak2 = 1.0 - cos((double) ((pig*(delnh - na))/(delnh+1)));
       mit1 = (2.0*( bild[lNM2 + ma] +  bild[lNM2 + dimm + ma]) + bild[lNM3 + ma]*fak2)/(4.0 + fak2);
       mit2 = (2.0*( bild[lNM2 + mb] +  bild[lNM2 + dimm + mb]) + bild[lNM3 + mb]*fak2)/(4.0 + fak2);
       bild[lNM2 + ma - 1] = (2.0*mit1 + fak*mit2)/(2.0 + fak);
     }
     if (mb < (dimm-1))
     { fak  = 1.0 - cos((double) ((pig*(mb - M - delmh + 1))/(delmh+1)));
       fak2 = 1.0 - cos((double) ((pig*(nb - N - delnh + 1))/(delnh+1)));
       lNM2 = na*dimm;
       lNM3 = nb*dimm;
       mit1 = (2.0*( bild[lNM3 - dimm + ma] +  bild[lNM3 + ma]) + bild[lNM2 + ma]*fak2)/(4.0 + fak2);
       mit2 = (2.0*( bild[lNM3 - dimm + mb] +  bild[lNM3 + mb]) + bild[lNM2 + mb]*fak2)/(4.0 + fak2);
       bild[lNM3 + mb + 1] = (2.0*mit2 + fak*mit1)/(2.0 + fak);

       fak2 = 1.0 - cos((double) ((pig*(delnh - na))/(delnh+1)));
       mit1 = (2.0*( bild[lNM2 + ma] +  bild[lNM2 + dimm + ma]) + bild[lNM3 + ma]*fak2)/(4.0 + fak2);
       mit2 = (2.0*( bild[lNM2 + mb] +  bild[lNM2 + dimm + mb]) + bild[lNM3 + mb]*fak2)/(4.0 + fak2);
       bild[lNM2 + mb + 1] = (2.0*mit2 + fak*mit1)/(2.0 + fak);
     }
     if (na > 0)
     {  fak  = 1.0 - cos((double) ((pig*(delnh-na))/(delnh+1)));
        fak2 = 1.0 - cos((double) ((pig*(mb - M - delmh + 1))/(delmh+1)));
        lNM2 = na*dimm;
        lNM3 = nb*dimm;
        mit1 = (2.0*( bild[lNM2 + mb - 1] +  bild[lNM2 + mb]) + bild[lNM2 + ma]*fak2)/(4.0 + fak2);
        mit2 = (2.0*( bild[lNM3 + mb - 1] +  bild[lNM3 + mb]) + bild[lNM3 + ma]*fak2)/(4.0 + fak2);
        bild[lNM2 - dimm + mb] = (2.0*mit1 + fak*mit2)/(2.0 + fak);

        fak2 = 1.0 - cos((double) ((pig*(delmh - ma))/(delmh+1)));
        mit1 = (2.0*( bild[lNM2 + ma] +  bild[lNM2 + ma + 1]) + bild[lNM2 + mb]*fak2)/(4.0 + fak2);
        mit2 = (2.0*( bild[lNM3 + ma] +  bild[lNM3 + ma + 1]) + bild[lNM3 + mb]*fak2)/(4.0 + fak2);
        bild[lNM2 - dimm + ma] = (2.0*mit1 + fak*mit2)/(2.0 + fak);
     }
     if (nb < (dimn-1))
     {  fak  = 1.0 - cos((double) ((pig*(nb - N - delnh + 1))/(delnh+1)));
        fak2 = 1.0 - cos((double) ((pig*(mb - M - delmh + 1))/(delmh+1)));
        lNM2 = na*dimm;
        lNM3 = nb*dimm;
        mit1 = (2.0*( bild[lNM2 + mb - 1] +  bild[lNM2 + mb]) + bild[lNM2 + ma]*fak2)/(4.0 + fak2);
        mit2 = (2.0*( bild[lNM3 + mb - 1] +  bild[lNM3 + mb]) + bild[lNM3 + ma]*fak2)/(4.0 + fak2);
        bild[lNM3 + dimm + mb] = (2.0*mit2 + fak*mit1)/(2.0 + fak);

        fak2 = 1.0 - cos((double) ((pig*(delmh - ma))/(delmh+1)));
        mit1 = (2.0*( bild[lNM2 + ma] +  bild[lNM2 + ma + 1]) + bild[lNM2 + mb]*fak2)/(4.0 + fak2);
        mit2 = (2.0*( bild[lNM3 + ma] +  bild[lNM3 + ma + 1]) + bild[lNM3 + mb]*fak2)/(4.0 + fak2);
        bild[lNM3 + dimm + ma] = (2.0*mit2 + fak*mit1)/(2.0 + fak);
     }

     if ((na > 0) && (ma > 0))
     {  lNM2 = na*dimm;
        lNM3 = nb*dimm;
        mit1 = ( bild[lNM2 - dimm + ma] +  bild[lNM2 + ma] +  bild[lNM2 + ma - 1])/3.0;
        mit2 = ( bild[lNM3 + ma] +  bild[lNM3 + ma - 1] +  bild[lNM2 + mb]
                   +  bild[lNM2 - dimm + mb] +  bild[lNM3 + mb])/5.0;
        fak  = 1.0 - cos((double) ((pig*(delnh-na))/(delnh+1)));
        fak2 = 1.0 - cos((double) ((pig*(delmh - ma))/(delmh+1)));
        if (fak > fak2) bild[lNM2 - dimm + ma - 1] = (2.0*mit1 + fak*mit2)/(2.0 + fak);
		else	bild[lNM2 - dimm + ma - 1] = (2.0*mit1 + fak2*mit2)/(2.0 + fak2);
     }
     if ((na > 0) && (mb < (dimm - 1)))
     {  lNM2 = na*dimm;
        lNM3 = nb*dimm;
        mit1 = ( bild[lNM2 - dimm + mb] +  bild[lNM2 + mb] +  bild[lNM2 + mb + 1])/3.0;
        mit2 = ( bild[lNM3 + mb] +  bild[lNM3 + mb + 1] +  bild[lNM2 + ma]
               +  bild[lNM2 - dimm + ma] +  bild[lNM3 + ma])/5.0;
        fak  = 1.0 - cos((double) ((pig*(delnh-na))/(delnh+1)));
        fak2 = 1.0 - cos((double) ((pig*(mb - M - delmh + 1))/(delmh+1)));
        if (fak > fak2) bild[lNM2 - dimm + mb + 1] = (2.0*mit1 + fak*mit2)/(2.0 + fak);
		else	bild[lNM2 - dimm + mb + 1] = (2.0*mit1 + fak2*mit2)/(2.0 + fak2);
     }
     if ((nb < (dimn - 1)) && (ma > 0))
     {  lNM2 = na*dimm;
        lNM3 = nb*dimm;
        mit1 = ( bild[lNM3 + ma - 1] +  bild[lNM3 + ma] +  bild[lNM3 + dimm + ma])/3.0;
        mit2 = ( bild[lNM3 + mb] +  bild[lNM2 + ma] +  bild[lNM2 + ma - 1]
               +  bild[lNM3 + dimm + mb] +  bild[lNM2 + mb])/5.0;
        fak  = 1.0 - cos((double) ((pig*(nb - N - delnh + 1))/(delnh+1)));
        fak2 = 1.0 - cos((double) ((pig*(delmh - ma))/(delmh+1)));
        if (fak > fak2) bild[lNM3 + dimm + ma - 1] = (2.0*mit1 + fak*mit2)/(2.0 + fak);
		else	bild[lNM3 + dimm + ma - 1] = (2.0*mit1 + fak2*mit2)/(2.0 + fak2);
     }
     if ((nb < (dimn - 1)) && (mb < (dimm - 1)))
     {  lNM2 = na*dimm;
        lNM3 = nb*dimm;
        mit1 = ( bild[lNM3 + dimm + mb] +  bild[lNM3 + mb] +  bild[lNM3 + mb + 1])/3.0;
        mit2 = ( bild[lNM2 + mb] +  bild[lNM2 + mb + 1] +  bild[lNM3 + ma]
               +  bild[lNM3 + dimm + ma] +  bild[lNM2 + ma])/5.0;
        fak  = 1.0 - cos((double) ((pig*(nb - N - delnh + 1))/(delnh+1)));
        fak2 = 1.0 - cos((double) ((pig*(mb - M - delmh + 1))/(delmh+1)));
        if (fak > fak2) bild[lNM3 + dimm + mb + 1] = (2.0*mit1 + fak*mit2)/(2.0 + fak);
		else	bild[lNM3 + dimm + mb + 1] = (2.0*mit1 + fak2*mit2)/(2.0 + fak2);
     }
     if (na > 0) --na;
     if (nb < (dimn - 1)) ++nb;
     if (ma > 0) --ma;
     if (mb < (dimm - 1)) ++mb;
  }
}
