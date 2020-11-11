/******************************************************************************
 * gnamoeba.h
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
 * Created:     Sat Jan 26 19:10:45 2002 by Koehler@cathy
 * Last change: Thu Jan 31 18:16:35 2002
 *
 ******************************************************************************/


#define DEFAULT_MAX_STEPS 2000

extern int gnamoeba_max_steps;

extern float **gnamoeba_alloc(int ndim);
extern void gnamoeba_free(float **p, int ndim);
extern int gnamoeba( float **p, float *y, int ndim, float ftol, float (*func)(float *));
