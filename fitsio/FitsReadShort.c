/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:50:43 1997 by Koehler@Lisa
 * Last change: Tue Oct  6 13:35:00 2009
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <sys/types.h>
#ifdef __sun
/**# include <sys/byteorder.h>**/
#define ntohl(x)	(x)
#define ntohs(x)	(x)
#else
# include <netinet/in.h>
#endif
#include "fitsio.h"

/************************************************************************
 * FitsReadShort(FitsFile)						*
 * Reads the next pixel value out of FitsFile, applies bscale and bzero,*
 * converts it to short (silently truncating to values short can hold),	*
 * and returns the result.						*
 ************************************************************************/

short FitsReadShort(struct FitsFile *ff)
{
	union anyvar data;
	unsigned short shdata;	/** for SHARP...sigh **/
	double dval;

  switch(ff->bitpix)
  { case 8:	fread( &data.c,sizeof(unsigned char),1,ff->fp);
		dval= (double)data.c;	break;
    case 16:	if( ff->flags & FITS_FLG_SHARP)
		{ fread( &shdata,sizeof(unsigned short),1,ff->fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  shdata= ntohs(shdata);
#endif
		  dval= (double)shdata;
		}
		else
		{ fread( &data.s,sizeof(short),1,ff->fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  data.s= ntohs(data.s);
#endif
		  dval= (double)data.s;
		}
		break;
    case 32:	fread( &data.l,4,1,ff->fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		data.l= ntohl(data.l);
#endif
		dval= (double)data.l;
		break;
    case -32:	fread( &data.f,sizeof(float),1,ff->fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		data.l= ntohl(data.l);
#endif
		dval= (double)data.f;
		break;
    case -64:	fread( &data.d,sizeof(double),1,ff->fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		{
			unsigned char hlp;
			hlp= data.byte[0];  data.byte[0]= data.byte[7];  data.byte[7]= hlp;
			hlp= data.byte[1];  data.byte[1]= data.byte[6];  data.byte[6]= hlp;
			hlp= data.byte[2];  data.byte[2]= data.byte[5];  data.byte[5]= hlp;
			hlp= data.byte[3];  data.byte[3]= data.byte[4];  data.byte[4]= hlp;
		}
#endif
		dval= (double) data.d;
		break;
    default:	dval= 0.0;
  }
  if( feof(ff->fp))
  {	fprintf(stderr,"\n End of file while reading Fits-file!\a\n");
	ff->flags |= FITS_FLG_EOF;
  }
  if( ferror(ff->fp))
  {	fprintf(stderr,"\n Error while reading Fits-file!\a\n");
	ff->flags |= FITS_FLG_ERROR;
  }

  dval= dval * ff->bscale + ff->bzero;
       if( dval > SHRT_MAX)  return SHRT_MAX;
  else if( dval < SHRT_MIN)  return SHRT_MIN;
  else			     return (short)dval;
}
