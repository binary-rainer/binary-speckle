/* $Id$
 * $Log$
 * Created:     Sun Apr 15 22:34:36 2001 by Koehler@cathy
 * Last change: Thu Jan 24 11:59:03 2002
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <ctype.h>	/** for isalpha() **/
#include "fitsio.h"

int main( int argc, char **argv)
{
	struct FitsFile *Fits, *ff;
	unsigned char *buffy;
	char line[88], *str;
	long arg, nfiles= argc-1, nframes, framesz, wordsz, i;

  if(nfiles < 1 || !strncmp(argv[1],"-h",2))
  {	printf("Usage: %s file1 [file2 ...]\n",argv[0]);
	return 10;
  }

  if( !(Fits= FitsOpenR(argv[1])))	return 15;
  (Fits->flags & FITS_FLG_PIPE) ? pclose(Fits->fp) : fclose(Fits->fp);
  Fits->fp= NULL;

  /** count frames in all files **/
  nframes= Fits->n3;
  for( arg=2;  arg<argc;  ++arg)
  {	if( (ff= FitsOpenR(argv[arg])))
	{ if( (Fits->bitpix == ff->bitpix)
	   && (Fits->n1 == ff->n1) && (Fits->n2 == ff->n2)
	   && (Fits->bscale == ff->bscale) && (Fits->bzero == Fits->bzero))
	  {
		printf("%s: %d frames\n",argv[arg],ff->n3);
		nframes += ff->n3;
	  }
	  FitsClose(ff);
	}
  }
  Fits->naxis= 3;	Fits->n3= nframes;

  /** allocate buffer for all frames **/
  framesz= Fits->n1 * Fits->n2;
  wordsz = FitsWordSize(Fits);
  if( !(Fits->Buffer.c= AllocBuffer(nframes * framesz * wordsz,"data buffer")))
  {	FitsClose(Fits);	return 20;	}

  /** now read all the files' data **/

  buffy= Fits->Buffer.c;
  for( arg=1;  arg<argc;  ++arg)
  { if( !(ff= FitsOpenR(argv[arg])))
    {	printf("Can't read file %s\a\n",argv[arg]);	}
    else
    {
	if( Fits->bitpix != ff->bitpix)
	{ printf("File %s has different bitpix, skipping it\a\n",argv[arg]); }
	else
	if( Fits->n1 != ff->n1 || Fits->n2 != ff->n2)
	{ printf("File %s has different frame size\a\n",argv[arg]); }
	else
	if( Fits->bscale != ff->bscale || Fits->bzero != ff->bzero)
	{ printf("File %s has different frame bscale/bzero\a\n",argv[arg]); }
	else
	{ printf("reading %s...\n",argv[arg]);
	  if( fread(buffy, wordsz, framesz*ff->n3, ff->fp) < framesz*ff->n3)
		printf("Error while reading %s\a\n",argv[arg]);

	  buffy += wordsz * framesz * ff->n3;
	  sprintf(line,"included %s",argv[arg]);
	  FitsAddHistKeyw( Fits,line);
	}
	FitsClose(ff);
    }
  }

  /** figure out how to call the cube... **/

  str= strstr(argv[1],".fits");
  if (str) { strncpy(line,argv[1],str-argv[1]);  line[str-argv[1]]= 0; }
      else { strcpy( line,argv[1]); }

  printf("line is %s\n",line);
  /** line is now something like "junk1234" **/

  for(i=0;  i<strlen(argv[1]) && isalpha((int)(argv[1][i]));  ++i)
  {
  /** old code used to find non-matching chars:
    for(arg=2;  arg<argc;  ++arg)  if (argv[1][i] != argv[arg][i])  break;
    if (arg < argc)  break;
  **/
  }
  /** i is the index of the first not-alpha char **/

  strcat(line,"-");
  strcat(line,argv[argc-1]+i);	/** line is now "junk1234-1245.fits[...]" **/
  i = strlen(line)-3;
  if( !strcmp( line+i, ".gz"))  line[i]= 0;	/** dump ".gz" at end **/

  /** and now write the stuff **/

  if( !(Fits->fp= rfopen(line,"w")))
  {	fprintf(stderr,"Can't open output file %s\n", line);	}
  else
  {	printf(" Writing %s (%d x %d x %d pixels)\n",
				line, Fits->n1,Fits->n2,Fits->n3);

	if( rfopen_pipe) Fits->flags |=  FITS_FLG_PIPE;
		else	 Fits->flags &= ~FITS_FLG_PIPE;

	sprintf(line,"NAXIS   = %20d   / %-45s",Fits->naxis,"Number of axes");
	FitsSetHeaderLine( Fits, "NAXIS   = ", line);
	sprintf(line,"NAXIS3  = %20d   / %-45s",Fits->n3,"number of pixels along 3.axis");
	FitsSetHeaderLine( Fits, "NAXIS3  = ", line);

	if( (fwrite( Fits->Header, 1, Fits->HeaderSize, Fits->fp) < Fits->HeaderSize)
	 || (fwrite( Fits->Buffer.c, wordsz, framesz*Fits->n3, Fits->fp) < framesz*Fits->n3))
		printf("Error while writing!\a\n");
  }
  FitsClose(Fits);
  return 0;
}
