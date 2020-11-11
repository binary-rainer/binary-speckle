/* $Id$
 * $Log$
 * Created:     Wed May 13 18:23:10 1998 by Koehler@hex
 * Last change: Thu Dec  7 14:08:22 2000
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <tk.h>
#include<X11/Xlib.h>
#include<X11/Xutil.h>

#undef COLOR_DEBUG

/********************** from lut.h by Stefan Eckardt **********************/

#define MAXCOLORVALUE 0xffff
#define RESERVE 10


/********************************* LUT stuff *********************************/

static void Setlut( unsigned short *table, int max_colors,
				double x0, double y0, double x1, double y1)
{
	int realx0,realy0;
	int realx1,realy1;
	int incry;
	int count;

  /** x == 1.0 sollte letzter existierender Farbe entsprechen **/
  realx0= (int)((double)(max_colors-1) * x0);
  realx1= (int)((double)(max_colors-1) * x1);
  realy0= (int)((double)(MAXCOLORVALUE)* y0);
  realy1= (int)((double)(MAXCOLORVALUE)* y1);

  if (realx0 > realx1)
  {	int tmp=realx1; realx1=realx0; realx0=tmp;	}

  incry= (realx1==realx0) ? 0 : (realy1-realy0)/(realx1-realx0);

  for( count= realx0;  count <= realx1;  count++ )
	table[count]= realy0 + (incry*(count-realx0));
}


void FreeColors(Display *display, Colormap colormap,
		unsigned long *colors, int num_colors)
{
	int i;
	long c;

  for( i=0;  i<num_colors;  ++i)
  {	c= colors[i];
	XFreeColors(display,colormap,&c,1,0L);
  }
}


long AllocColors(Display *display, Colormap colormap,
		 unsigned long *colors, int min_colors, int max_colors, char *lutname)
{
	unsigned short *Red, *Green, *Blue;
	XColor xcol;
	int i;

  if( !(Red= malloc(3*(max_colors+RESERVE)*sizeof(short))))	return 0;
  Green= Red   + max_colors+RESERVE;
  Blue = Green + max_colors+RESERVE;

  for(;;)	/** end condition in the middle **/
  {
    if( strcmp(lutname,"blackbody")==0)
    {				/* x0	y0   x1	  y1	*/
	Setlut( Red  , max_colors, 0.0 ,0.0, 0.5 ,1.0 );
	Setlut( Red  , max_colors, 0.5 ,1.0, 1.0 ,1.0 );
	Setlut( Green, max_colors, 0.0 ,0.0, 0.25,0.0 );
	Setlut( Green, max_colors, 0.25,0.0, 0.50,1.0 );
	Setlut( Green, max_colors, 0.5 ,1.0, 1.0 ,1.0 );
	Setlut( Blue , max_colors, 0.0 ,0.0, 0.5 ,0.0 );
	Setlut( Blue , max_colors, 0.5 ,0.0, 1.0 ,1.0 );
    }
    else if( strcmp(lutname,"heat")==0)
    {				/* x0	y0   x1	  y1	*/
	Setlut( Red  , max_colors, 0.0 ,0.0, 0.25,0.0 );
	Setlut( Red  , max_colors, 0.25,0.0, 0.5 ,1.0 );
	Setlut( Red  , max_colors, 0.5 ,1.0, 1.0 ,1.0 );
 	Setlut( Green, max_colors, 0.0 ,0.0, 0.5 ,0.0 );
	Setlut( Green, max_colors, 0.5 ,0.0, 0.75,1.0 );
	Setlut( Green, max_colors, 0.75,1.0, 1.0 ,1.0 );
	Setlut( Blue , max_colors, 0.0 ,0.0, 0.25,1.0 );
	Setlut( Blue , max_colors, 0.25,1.0, 0.5 ,0.0 );
	Setlut( Blue , max_colors, 0.5 ,0.0, 0.75,0.0 );
	Setlut( Blue , max_colors, 0.75,0.0, 1.0 ,1.0 );
    }
    else if( strcmp(lutname,"rainbow")==0)
    {				/* x0	y0   x1	  y1	*/
	Setlut( Red  , max_colors, 0.0 ,1.0, 0.33,0.0 );
	Setlut( Red  , max_colors, 0.33,0.0, 0.67,0.0 );
	Setlut( Red  , max_colors, 0.67,0.0, 1.0 ,1.0 );
	Setlut( Green, max_colors, 0.0 ,0.0, 0.33,1.0 );
	Setlut( Green, max_colors, 0.33,1.0, 0.67,0.0 );
	Setlut( Green, max_colors, 0.67,0.0, 1.0 ,0.0 );
	Setlut( Blue , max_colors, 0.0 ,0.0, 0.33,0.0 );
	Setlut( Blue , max_colors, 0.33,0.0, 0.67,1.0 );
	Setlut( Blue , max_colors, 0.67,1.0, 1.0 ,0.0 );
    }
    else	/** default: grayscale **/
    {
	Setlut( Red  , max_colors, 0.0,0.0, 1.0,1.0 );
	Setlut( Green, max_colors, 0.0,0.0, 1.0,1.0 );
	Setlut( Blue , max_colors, 0.0,0.0, 1.0,1.0 );
    }
   /*
    * Allocate the colors
    */
    for( i= 0; i < max_colors; i++)
    {	xcol.red  = Red[i];
	xcol.green= Green[i];
	xcol.blue = Blue[i];
#ifdef COLOR_DEBUG
	fprintf(stderr,"color %d: %02x %02x %02x\n",i,xcol.red,xcol.green,xcol.blue);
#endif
	if( XAllocColor(display,colormap,&xcol) == 0)
		break;
	else colors[i] = xcol.pixel;
    }
#ifdef COLOR_DEBUG
	fprintf(stderr,"got %d colors\n",i);
#endif
    if( i == max_colors || max_colors <= min_colors)	break;
    /** end if we got all we wanted, or if we asked only for the minimum **/

    FreeColors(display,colormap,colors,i);
    max_colors = i;		/** try to get what we got with the right lut **/
    XSync(display,False);	/** wait until the colors are actually free **/
  }
  free(Red);
  return i;
}
