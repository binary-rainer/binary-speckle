/* $Id: mkflat.c,v 1.2 1995/03/05 17:09:36 koehler Exp koehler $
 * $Log: mkflat.c,v $
 * Revision 1.2  1995/03/05  17:09:36  koehler
 * ???
 *
 * Revision 1.1  1995/02/17  15:02:31  koehler
 * Initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsio.h"

/*
#define DEBUG	1
*/

#ifdef DEBUG
# define debug(f,a,b)	printf(f,a,b)
#else
# define debug(f,a,b)	;
#endif


struct FitsFile OutFits;
double *bright=NULL, *dark=NULL;
long framesz, nbright=0, ndark=0;

/*****************************************************************************/

void InitBuffers(struct FitsFile *InFits)
{
	long i;

  framesz= InFits->n1 * InFits->n2;

  if( !(bright= AllocBuffer( framesz*sizeof(double),"bright frames"))) exit(20);
  if( !(dark  = AllocBuffer( framesz*sizeof(double),"dark frames")))   exit(20);

  for( i=0;  i<framesz;  ++i)	bright[i]= dark[i]= 0.0;

  OutFits.n1= InFits->n1;	OutFits.n2= InFits->n2;
  FitsMakeSimpleHeader( &OutFits);
}


void Usage(char *prg)
{
  printf(" Usage: %s [-b bitpix] "
	 "[-bright file[s]] [-dark file[s]] [-o Outputfile]\n",prg);
}

int main( int argc, char **argv)
{
	struct FitsFile *InFits;
	char *Outname= "flatfield.fits", *hdrline;
	unsigned short *hist;
	union anypntr aptr;
	double val, min,max;
	long arg, fr, i;

  if(argc==1)	{  Usage(argv[0]);	return 10;	}

  fprintf(stderr,"\n mkflat, compiled on " __DATE__ " ==============================\n");

  OutFits.bitpix= -32;	/** Default value **/
  OutFits.naxis= 2;
  OutFits.n1= OutFits.n2= 0;	/** not yet known **/
  strcpy(OutFits.object, "flatfield");
  OutFits.Header= NULL;

  for( arg=1;  arg<argc;  ++arg)
  { debug(" argv[%ld] = %s",arg,argv[arg]);	printf("\n");

    if( !strncmp(argv[arg],"-br",3))
    { for( ++arg;  arg<argc && argv[arg][0] != '-';  ++arg)
      {
	if( !(InFits= fits_open_r(argv[arg])))	continue;
	printf(" Reading bright frame %s...\n",argv[arg]);
	if( InFits->naxis < 2)
	{	printf(" No frame in %s - skipping\a\n",argv[arg]);	}
	else
	{ if( !bright)	InitBuffers(InFits);

	  if( InFits->n1 != OutFits.n1 || InFits->n2 != OutFits.n2)
	  {	printf(" Frame sizes do not match - skipping\a\n");	}
	  else
	  { if( (hdrline= FitsFindEmptyHdrLine( &OutFits)))
	    {	sprintf(hdrline,"COMMENT = bright frame: %s",argv[arg]);
		for( i= strlen(hdrline);  i<80;  ++i)	hdrline[i]= ' ';
	    }
	    for( fr= (InFits->naxis>2) ? InFits->n3 : 1;  fr>0;  --fr)
	    {	for( i=0; i<framesz; ++i)  bright[i] += FitsReadDouble(InFits);
		nbright++;
	    }
	  }
	}
	fits_close(InFits);
      }
	--arg;
    }
    else
    if( !strncmp(argv[arg],"-da",3))
    { for( ++arg;  arg<argc && argv[arg][0] != '-';  ++arg)
      {
	if( !(InFits= fits_open_r(argv[arg])))	continue;
	printf(" Reading dark frame %s...\n",argv[arg]);
	if( InFits->naxis < 2)
	{	printf(" No frame in %s - skipping\a\n!",argv[arg]);	}
	else
	{ if( !dark)	InitBuffers(InFits);

	  if( InFits->n1 != OutFits.n1 || InFits->n2 != OutFits.n2)
	  {	printf(" Frame sizes do not match - skipping\a\n");	}
	  else
	  { if( (hdrline= FitsFindEmptyHdrLine( &OutFits)))
	    {	sprintf(hdrline,"COMMENT = dark frame: %s",argv[arg]);
		for( i= strlen(hdrline);  i<80;  ++i)	hdrline[i]= ' ';
	    }
	    for( fr= (InFits->naxis>2) ? InFits->n3 : 1;  fr>0;  --fr)
	    {	for( i=0; i<framesz; ++i)  dark[i] += FitsReadDouble(InFits);
		ndark++;
	    }
	  }
	}
	fits_close(InFits);
      }
	--arg;
    }
    else	/** kurze options MÜSSEN nach langen kommen (-b:-br)! **/
    if( !strncmp(argv[arg],"-b",2) && ++arg<argc)
					  OutFits.bitpix= atoi(argv[arg]);
    else
    if( !strncmp(argv[arg],"-o",2) && ++arg<argc)  Outname= argv[arg];
    else
    {	if( strncmp(argv[arg],"-h",2))
		fprintf(stderr," Unknown option: %s\a\n\n",argv[arg]);
	Usage(argv[0]);		return 5;
    }
  }

  if( nbright)
  {	val= (double)nbright;
	for( i=0; i<framesz; ++i)  bright[i] /= val;
  }
  else	printf("\n Warning: No bright frames found - using 0.0\a\n");

  if( ndark)
  {	val= (double)ndark;
	for( i=0; i<framesz; ++i)  dark[i] /= val;
  }
  else	printf("\n Warning: No dark frames found - using 0.0\a\n");

  for( i=0;  i<framesz;  ++i)	bright[i] -= dark[i];

  aptr.d= bright;  FindMinMax( aptr,-64,framesz, &min,&max);
  debug(" Min: %g, Max: %g\n",min,max);

  fr= (long)max - (long)min + 4;	/** bisschen mehr zur Sicherheit **/
  if( (hist= AllocBuffer( fr * sizeof(unsigned short),"histogramm")))
  {	for( i=0; i<fr;  ++i)	hist[i]= 0;
	for( i=0; i<framesz;  ++i)
		++hist[ (int)(bright[i]-min) ];

	/** die Anzahl der Pixel ist framesz, also ist das "median-Pixel" */
	/** das mit der Nummer framesz/2 in der Histogramm-liste	**/
	arg= framesz/2;
	for( i=0;  i<fr;  ++i)	/** jetzt den mittleren suchen **/
		if( (arg -= hist[i]) <= 0)  break;

	val= min + (double)i;		/** das ist der median des frames **/
	debug(" median: %g%c\n",val,' ');

	for( i=0;  i<framesz;  ++i)	bright[i] /= val;
	free(hist);
  }
  FitsAddHistKeyw( &OutFits,"created by mkflat");
  FitsWrite( Outname, &OutFits, bright);

  free(bright);
  free(dark);
  return 0;
}
