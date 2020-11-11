/* $Id$
 * $Log$
 * Created:     Tue Aug 25 21:13:31 1998 by Koehler@cathy
 * Last change: Tue Aug 25 21:58:49 1998
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

/************************************************************************
 * struct FitsFile *LoadFitsFile()					*
 * Opens file, allocates buffer, and loads data in Fits-File		*
 *									*
 * char *filename - the file to load					*
 * short bufbitpix - datatype of the buffer				*
 * short (*FrameCallBack)(struct FitsFile *, long fr, void *userdata)	*
 *		- function that will be called after a frame was loaded	*
 * void *userdata - some user-defined ptr passed to FrameCallBack()	*
 ************************************************************************/

struct FitsFile *LoadFitsFile(
	char *filename,
	short bufbitpix,
	short (*FrameCallBack)(struct FitsFile *, long fr, void *userdata),
	void *userdata	)
{
	struct FitsFile *ff;
	short wordsz;
	long fr, i, framesz;
	char sharp[10];

  switch(bufbitpix)
  { case 8:	wordsz= sizeof(unsigned char);	break;
    case 16:	wordsz= sizeof(short);	break;
    case 32:	wordsz=	sizeof(long);	break;
    case -32:	wordsz=	sizeof(float);	break;
    case -64:	wordsz=	sizeof(double);	break;
    default:	fprintf(stderr,"unknown BITPIX-Value %d\n",bufbitpix);
		return NULL;
  }
  if( !(ff = FitsOpenR(filename)))  return NULL;

  framesz= ff->n1 * ff->n2;
  if( !(ff->Buffer.c= AllocBuffer(framesz * ff->n3 * wordsz,"Fits buffer")))
  {	FitsClose(ff);	return NULL; }

  /********** Wenn SHARP, dann statistics frame skippen **********/
  if( ff->flags & FITS_FLG_SHARP)
	fread( ff->Buffer.c, sizeof(short), framesz, ff->fp);

  for( fr=0;  fr<ff->n3;  ++fr)
  {
    if( ff->flags & FITS_FLG_SHARP)	fread( sharp,1,8,ff->fp);

    switch(bufbitpix)
    {
      case 8:	{ unsigned char *frameptr= ff->Buffer.c + fr*framesz;
		  for( i=0;  i<framesz;  ++i)
			frameptr[i]= (unsigned char) (*ff->ReadDblPixel)(ff);
		  break;
		}
      case 16:	{ short *frameptr= ff->Buffer.s + fr*framesz;
		  for( i=0;  i<framesz;  ++i)
			frameptr[i]= (short) (*ff->ReadDblPixel)(ff);
		  break;
		}
      case 32:	{ long *frameptr= ff->Buffer.l + fr*framesz;
		  for( i=0;  i<framesz;  ++i)
			frameptr[i]= (long) (*ff->ReadDblPixel)(ff);
		  break;
		}
      case -32:	{ float *frameptr= ff->Buffer.f + fr*framesz;
		  for( i=0;  i<framesz;  ++i)
			frameptr[i]= (float) (*ff->ReadDblPixel)(ff);
		  break;
		}
      default:	{ double *frameptr= ff->Buffer.d + fr*framesz;
		  for( i=0;  i<framesz;  ++i)
			frameptr[i]= (*ff->ReadDblPixel)(ff);
		  break;
		}
    }
    if( FitsCheckError(ff))	break;
    if( FrameCallBack)
	if( (*FrameCallBack)(ff,fr,userdata) != 0)  break;
  }
  fclose(ff->fp);	ff->fp= NULL;
  return ff;
}
