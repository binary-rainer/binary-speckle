/* $Id: FitsOpenR.c,v 1.1 1997/07/19 17:41:29 koehler Exp koehler $
 * $Log: FitsOpenR.c,v $
 * Revision 1.1  1997/07/19  17:41:29  koehler
 * Initial revision
 *
 * Created:     Sat Jun 28 16:49:41 1997 by Koehler@Lisa
 * Last change: Fri Sep 14 15:26:22 2012
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

#ifdef __sun
/**# include <sys/byteorder.h>**/
#define ntohl(x)	(x)
#define ntohs(x)	(x)
#else
# include <netinet/in.h>
#endif

/************************************************************************
 * Routine to check for End-Of-File and Error				*
 ************************************************************************/

short FitsCheckError(struct FitsFile *ff)
{
  if( feof(ff->fp))
  {	fprintf(stderr,"\n End of file while reading Fits-file!\a\n");
	ff->flags |= FITS_FLG_EOF;
	return 1;
  }
  if( ferror(ff->fp))
  {	fprintf(stderr,"\n Error while reading Fits-file!\a\n");
	ff->flags |= FITS_FLG_ERROR;
	return 2;
  }
  return 0;
}

/************************************************************************
 * Routines to read a dataword and swap, if necessary			*
 ************************************************************************/

unsigned char ReadUByte(FILE *fp)
{	unsigned char data;

  fread( &data,sizeof(unsigned char),1,fp);
  return data;
}

unsigned short ReadUShort(FILE *fp)
{
	union
	{ unsigned short s;
	  unsigned char b[2];
	} data;

  fread( &data.s,sizeof(unsigned short),1,fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
  {
	unsigned char hlp;
	hlp= data.b[0];  data.b[0]= data.b[1];  data.b[1]= hlp;
  }
#endif
  return data.s;
}

short ReadShort(FILE *fp)
{
	union
	{ short s;
	  unsigned char b[2];
	} data;

  fread( &data.s,sizeof(short),1,fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
  {
	unsigned char hlp;
	hlp= data.b[0];  data.b[0]= data.b[1];  data.b[1]= hlp;
  }
#endif
  return data.s;
}

long ReadLong(FILE *fp)
{
	union
	{ int l;
	  unsigned char b[4];
	} data;

	fread( &data.l,4,1,fp);	/** FITS has 4 bytes, not sizeof(int) **/
#if defined(i386) || defined(__x86_64) || defined(__alpha)
  {
	unsigned char hlp;
	hlp= data.b[0];  data.b[0]= data.b[3];  data.b[3]= hlp;
	hlp= data.b[1];  data.b[1]= data.b[2];  data.b[2]= hlp;
  }
#endif
  return data.l;
}

float ReadFloat(FILE *fp)
{
	union
	{ float f;
	  unsigned char b[4];
	} data;

  fread( &data.f,sizeof(float),1,fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
  {
	unsigned char hlp;
	hlp= data.b[0];  data.b[0]= data.b[3];  data.b[3]= hlp;
	hlp= data.b[1];  data.b[1]= data.b[2];  data.b[2]= hlp;
  }
#endif
  return data.f;
}

double ReadDouble(FILE *fp)
{
	union
	{ double d;
	  unsigned char b[8];
	} data;

  fread( &data.d,sizeof(double),1,fp);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
  {
	unsigned char hlp;
	hlp= data.b[0];  data.b[0]= data.b[7];  data.b[7]= hlp;
	hlp= data.b[1];  data.b[1]= data.b[6];  data.b[6]= hlp;
	hlp= data.b[2];  data.b[2]= data.b[5];  data.b[5]= hlp;
	hlp= data.b[3];  data.b[3]= data.b[4];  data.b[4]= hlp;
  }
#endif
  return data.d;
}


/************************************************************************
 * Routines to read a pixel						*
 ************************************************************************/

static double ReadDoubleFromChar(struct FitsFile *ff)
{
	return( (double)ReadUByte(ff->fp) * ff->bscale + ff->bzero);
}

static double ReadDoubleFromUShort(struct FitsFile *ff)
{
	return( (double)ReadUShort(ff->fp) * ff->bscale + ff->bzero);
}

static double ReadDoubleFromShort(struct FitsFile *ff)
{
	return( (double)ReadShort(ff->fp) * ff->bscale + ff->bzero);
}

static double ReadDoubleFromLong(struct FitsFile *ff)
{
	return( (double)ReadLong(ff->fp) * ff->bscale + ff->bzero);
}

static double ReadDoubleFromFloat(struct FitsFile *ff)
{
	return( (double)ReadFloat(ff->fp) * ff->bscale + ff->bzero);
}

static double ReadDoubleFromDouble(struct FitsFile *ff)
{
	return( (double)ReadDouble(ff->fp) * ff->bscale + ff->bzero);
}

/************************************************************************
 * FitsOpenR(filename)							*
 * Opens file named "filename" for reading, reads Header and allocates	*
 * and initializes struct FitsFile according to the file (except Buffer	*
 * which is set to NULL. The file will be positioned at the first byte	*
 * of image data.							*
 * Is able to detect SHARP-Fits-format (by skipping the first 4 bytes   *
 * and treating the rest as a normal FITS-Header). In this case, the	*
 * file will be positioned at the first byte of the statistics frame.	*
 * n3 will be the number of image frames!				*
 * Returns pointer to struct FitsFile or NULL if something went wrong.	*
 ************************************************************************/

struct FitsFile *FitsOpenR( char *filename)
{
	struct FitsFile *ff=NULL;
	FILE *fp=NULL;
	char *keyval, ch, skip[10];
	int nrd;

  if( !(fp= rfopen(filename,"r")))
  {	fprintf(stderr,"\n Can't open \"%s\" !\n",filename);	return NULL;	}

  if( !(ff= AllocBuffer(sizeof(struct FitsFile),"FitsFile")))
  {	if(filename)	rfopen_pipe? pclose(fp) : fclose(fp);
	return NULL;
  }
  ff->flags=0;
  if( (ff->filename= filename)==NULL)
  {	ff->flags |= FITS_FLG_DONT_CLOSE;
	ff->filename= "(stdin)";
  }
  if( rfopen_pipe)	ff->flags |= FITS_FLG_PIPE;
  ff->fp= fp;
  ff->HeaderSize= 2880;	 /** allociert wird ein bisschen mehr fuer 0 am Ende **/
  ff->Buffer.c= NULL;

  if( !(ff->Header= AllocBuffer(ff->HeaderSize+10,"FitsHeader")))
  {	FitsClose(ff);		return NULL;	}

  nrd= fread( ff->Header,sizeof(char),2880,fp);	/** 1.record einlesen **/
  if (nrd < 2880) fprintf(stderr,"Read only %d chars for header\n",nrd);

  ff->Header[2880]= 0;	/** deshalb muss etwas mehr allociert werden **/

  ch= ff->Header[40];	ff->Header[40]= 0;	/** vorher sollte das T stehen **/

  //  fprintf(stderr,"Header: '%s'\n",ff->Header);
  if( strncmp("SIMPLE  = ",ff->Header,10) || (strchr(ff->Header,'T') == NULL))
  { ff->Header[40]= ch;
    memmove(ff->Header,ff->Header+4,2880-4);	/** try to skip SHARP-junk-word **/
    ch= ff->Header[40];  ff->Header[40]= 0;	/** vorher sollte das T stehen **/
    if( strncmp("SIMPLE  = ",ff->Header,10) || !strchr(ff->Header,'T'))
    {	fprintf(stderr,"\n %s is neither in FITS- nor in SHARP-format\n",filename);
	FitsClose(ff);
	return NULL;
    }
    fprintf(stderr,"\n %s smells like a SHARP-file. We'll see if I can handle this\n",
		filename);
    ff->flags |= FITS_FLG_SHARP;
    fread( ff->Header+(2880-4),1,4,fp);	/** read missing word **/
  }
  ff->Header[40]= ch;	/** der Header sollte hinterher so aussehen wie vorher **/

  while( !FitsFindKeyw( ff,"END       "))
  {	/** END noch nicht gefunden - noch'n header record **/
	ff->HeaderSize += 2880;
#ifdef DEBUG
	fprintf(stderr," increasing header size to %ld\n",ff->HeaderSize);
#endif
	if( !(ff->Header= realloc(ff->Header,ff->HeaderSize+10)))
	{	fprintf(stderr,"\n No memory for header of %s (%ld bytes)\n",
							filename,ff->HeaderSize);
		FitsClose(ff);	 return NULL;
	}
	if( !fread( ff->Header + ff->HeaderSize - 2880,sizeof(char),2880,fp))
	{	fprintf(stderr,"\n End of header in %s not found\n",filename);
		FitsClose(ff);	 return NULL;
	}
	ff->Header[ff->HeaderSize]= 0;	/** deshalb muss etwas mehr allociert werden **/
  }
  if( !(keyval= FitsFindKeyw( ff,"BITPIX  = ")))
  {	fprintf(stderr,"\n no BITPIX-value in %s\n",filename);	FitsClose(ff);	 return NULL;	}
  ff->bitpix= atoi(keyval);

  if( !(keyval= FitsFindKeyw( ff,"NAXIS   = ")))
  {	fprintf(stderr,"\n no NAXIS in %s\n",filename);		FitsClose(ff);	 return NULL;	}

  ff->naxis = atoi(keyval);
  if( ff->naxis < 2)
  {	fprintf(stderr,"\n not enough axes in %s\n",filename);	FitsClose(ff);	 return NULL;	}

  if( !(keyval= FitsFindKeyw( ff,"NAXIS1  = ")))
  {	fprintf(stderr,"\n no NAXIS1 in %s\n",filename);	FitsClose(ff);	 return NULL;	}
  ff->n1= atoi(keyval);

  if( !(keyval= FitsFindKeyw( ff,"NAXIS2  = ")))
  {	fprintf(stderr,"\n no NAXIS2 in %s\n",filename);	FitsClose(ff);	 return NULL;	}
  ff->n2= atoi(keyval);

  if( ff->naxis==2)	 ff->n3 = 1;
  else
  { if( !(keyval= FitsFindKeyw( ff,"NAXIS3  = ")))
    {	fprintf(stderr,"\n no NAXIS3 in %s\n",filename);	FitsClose(ff);	 return NULL;	}
    ff->n3= atoi(keyval);
    if( ff->flags & FITS_FLG_SHARP)	ff->n3--;
    if( ff->naxis>3)  fprintf(stderr,"\n reading only 1.cube out of %s\n",filename);
  }

  if( !(keyval= FitsFindKeyw(ff,"OBJECT  = ")))
  {	/*fprintf(stderr,"\n no objektname in %s, using 'unknown'\n",filename);*/
		strcpy(ff->object,"unknown             ");
  }
  else	memcpy(ff->object,keyval,20);
  ff->object[20] = '\0';

  ff->bscale= (keyval= FitsFindKeyw(ff,"BSCALE  = ")) ? atof(keyval) : 1.0;
#ifdef DEBUG
	fprintf(stderr," BSCALE = %g\n",ff->bscale);
#endif
  ff->bzero = (keyval= FitsFindKeyw(ff,"BZERO   = ")) ? atof(keyval) : 0.0;

  ff->SA_xmin= 1;  ff->SA_xmax= ff->n1;	 /** one-offset **/
  ff->SA_ymin= 1;  ff->SA_ymax= ff->n2;

  if( (ff->SA_hdrline= FitsFindKeyw(ff,"SAVEAREA= ")))
  {		/** str points to '[xmin:xmax,ymin:ymax]', I hope... **/

    if( sscanf(ff->SA_hdrline," \' [%hd :%hd ,%hd :%hd",
		&ff->SA_xmin, &ff->SA_xmax, &ff->SA_ymin, &ff->SA_ymax) < 4)
    {	fprintf(stderr,
		"  Scanning of SAVEAREA failed - maybe it will work, maybe it won't\a\n");
    }
  }

  if ( !(ff->flags & FITS_FLG_SHARP) && (ff->bitpix == BITPIX_SHORT) &&
	(keyval= FitsFindKeyw(ff,"INSTRUME= ")) && (strncmp(keyval,"'SHARPI  '",10) == 0))
  {
	/**fprintf(stderr," SHARP file in disguise as FITS!\n");**/
	ff->flags |= FITS_FLG_NEW_SHARP;
  }

  switch(ff->bitpix)
  { case 8:	ff->ReadDblPixel= ReadDoubleFromChar;	break;
    case 16:	if( ff->flags & FITS_FLG_SHARP)
			ff->ReadDblPixel= ReadDoubleFromUShort;
		else	ff->ReadDblPixel= ReadDoubleFromShort;
		break;
    case 32:	ff->ReadDblPixel= ReadDoubleFromLong;	break;
    case -32:	ff->ReadDblPixel= ReadDoubleFromFloat;	break;
    case -64:	ff->ReadDblPixel= ReadDoubleFromDouble;	break;
    default:	fprintf(stderr,"Unknown BITPIX-Value %d!\a\n",ff->bitpix);
		FitsClose(ff);
		return NULL;
  }


  if( ff->flags & FITS_FLG_SHARP)  fread(skip,1,8,fp);	/** 8 bytes junk **/
  return ff;
}
