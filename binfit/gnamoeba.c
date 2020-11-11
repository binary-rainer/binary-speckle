/******************************************************************************
 * gnamoeba.c
 *
 * Copyright 2002 Rainer Koehler (rkoehler@ucsd.edu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ******************************************************************************
 *
 * Created:     Fri Jan 25 21:59:02 2002 by Koehler@cathy
 * Last change: Thu Jan 31 18:15:25 2002
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "gnamoeba.h"

/*
 * if you want gnamoeba to abort after more or less steps,
 * set this variable before calling it.
 */
int gnamoeba_max_steps= DEFAULT_MAX_STEPS;


/*****************************************************************************
 * gnamoeba_free:
 * frees arrays allocated by gnamoeba_alloc
 */

void gnamoeba_free(float **p, int ndim)
{
	int i;

  if(p)
  { for( i=0;  i<=ndim;  ++i)
    {
	if( p[i]==NULL) break;	/** if alloc failed, this is where it happened **/
	free(p[i]);
    }
    free(p);
  }
}

/*****************************************************************************
 * gnamoeba_alloc:
 * allocates simplex, i.e. array of (ndim+1) pointers,
 * each pointing to array of ndim floats
 * returns NULL if out-of-memory
 */

float **gnamoeba_alloc(int ndim)
{
	float **p;
	int i;

  if((p= malloc((ndim+1)*sizeof(float *))))
  { for( i=0;  i<=ndim;  ++i)
      if( !(p[i]= malloc(ndim*sizeof(float))))
      {	gnamoeba_free(p,ndim);	return NULL;  }
  }
  return p;
}

/*****************************************************************************
 * new_point():
 * create new point at c p_hi + (1-c) p_bar,
 * where p_bar = (centroid of points with j != hi) = SUM(p_j)/ndim
 */

static float new_point( float **p, float *new, int ndim, int hi,
			float c, float (*func)(float *))
{
	int i,j;
	float c2= (1.-c)/(float)ndim;

  for(i=0;  i<ndim;  ++i)
  { new[i]= c * p[hi][i]; 	/** initialize with c * p_hi **/

    for(j=0;  j<=ndim;  ++j)		/** all points  **/
      if( j != hi)			/** except p_hi **/
	new[i] += c2 * p[j][i];		/** add (1-c) * p_j / ndim **/
  }
  return (*func)(new);
}

/*****************************************************************************
 * gnamoeba - wiggle around in parameter space to find minimum
 *
 * p points to array of ndim+1 ptrs, each pointing to array of ndim floats
 * y points to array of ndim+1 floats
 * ftol: criterion for halting the procedure
 *	 will stop if 2*|max-min|/(|max|+|min|) < ftol
 * func: pointer to function to be minimized
 *
 * returns number of steps taken
 */

#define REPLACE_HI  { sw_p= p[hi];  p[hi]= pnew;  pnew= sw_p;  y[hi]= ynew; }

int gnamoeba( float **p, float *y, int ndim, float ftol, float (*func)(float *))
{
	int i, j, hi, lo, steps=0;
	float *sw_p, *pnew, ynew;

  if( !(pnew= malloc(ndim*sizeof(float))))
  {	fprintf(stderr,"not enough memory for gnamoeba!\a\n");
	return -1;
  }

  for(;;)
  {
    /** find lowest and highest points **/

    for( hi= lo= 0, i=1;  i<=ndim;  ++i)
    {
	if( y[i] < y[lo])  { lo= i; }
	if( y[i] > y[hi])  { hi= i; }
    }

    /** did we move around often enough? **/
    if( ++steps > gnamoeba_max_steps)  break;
#ifdef DEBUG
    printf("%d step, hi=%d (%g), lo=%d (%g):\n",steps,hi,y[hi],lo,y[lo]);
#endif

    /** are we close enough to a minimum? **/

    if( (y[hi] == 0.0) ||		/** highest point == zero is good enough **/
	(2.*fabs(y[hi]-y[lo]) / (fabs(y[hi])+fabs(y[lo])) < ftol))
		break;
	/** of course, y should be >= 0, so the fabs are just for protection **/

    /** 1.step - reflection: look at -p_hi + 2 p_bar **/

    if((ynew= new_point(p,pnew,ndim,hi,-1.,func)) <= y[lo])
    {
	REPLACE_HI	/** new minimum, replace p_hi (hi becomes the real lo now!) **/

	/** 2.step - expansion: look at 2 p_hi - p_bar **/

	if((ynew= new_point(p,pnew,ndim,hi,2.,func)) < y[hi])
	{
	  REPLACE_HI	/** an even better minimum, replace p_hi and restart **/
	}
    }
    else		   /** so alpha-point is no new minimum **/
    {	if( ynew < y[hi])	/** is it better than old max.? **/
	{
	  REPLACE_HI

	  /** do we have a new maximum? **/
	  for( i=0;  i<=ndim;  ++i)
	    if( i != hi)
	    {	printf("%f,",y[i]);
		if(ynew <= y[i])  break;
	    }
	  if(i <= ndim) continue;  /** we have a new max, so this step was successful **/
	}
	/** the new point is just another maximum, so try **/
	/** 3.step - contraction: look at 1/2 p_hi + 1/2 p_bar **/

	if((ynew= new_point(p,pnew,ndim,hi,0.5,func)) <= y[hi])
	{
	  REPLACE_HI	/** found a better point **/
	}
	else	/** failed contraction, replace all points and restart **/
	{
	  for(i=0;  i<=ndim;  ++i)	/** replace p_i by (p_i + p_lo)/2. **/
	    if(i != lo)
	    {	for(j=0;  j<ndim;  ++j)
		{ p[i][j] += p[lo][j];	p[i][j] /= 2.;	}
		y[i]= (*func)(p[i]);
	    }
	}
    }
  }

  /** leave lowest point at p[0] **/
  sw_p= p[lo];  p[lo]= p[0];  p[0]= sw_p;
  ynew= y[lo];  y[lo]= y[0];  y[0]= ynew;

  free(pnew);
  return steps;
}
