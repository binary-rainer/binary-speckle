/* $Id$
 * $Log$
 * Created:     Sat Jun 28 17:06:11 1997 by Koehler@Lisa
 * Last change: Tue Oct  6 13:29:32 2009
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
#include <unistd.h>
#include <sys/types.h>
#ifdef __sun
/**# include <sys/byteorder.h>**/
#define htonl(x)	(x)
#define htons(x)	(x)
#else
# include <netinet/in.h>
#endif


/****************************************************************
 * FitsWriteAny(filename,FitsFile,buffer,bufbitpix)		*
 * Writes the data in FitsFile and buffer into a file named	*
 * filename.  Bitpix of the file is given by FitsFile->bitpix,	*
 * The type of the data in the buffer is given by bufbitpix	*
 * Returns 0 if successful, 1 if an error occured		*
 ****************************************************************/

short FitsWriteAny( char *name, struct FitsFile *ff, void *dummy, short bufbitpix)
{
	char line[88];
	union anypntr buff;
	/*union anyvar data;*/
	double min, max;
	short err=0, retrycnt=0;
	long  i, cubesz, filesz;

  buff.c= dummy;
  if( ff->naxis<3)	ff->n3= 1;
  cubesz= ff->n1 * ff->n2 * ff->n3;

  if( !ff->Header)	FitsMakeSimpleHeader(ff);
  do
  { ff->filename= name? name : "(stdout)";	err=0;
    if( !(ff->fp= rfopen( name,"wb")))
    {	fprintf(stderr,"Can't open output file %s\n",ff->filename);	}
    else
    {	printf(" Writing %s (%d x %d x %d pixels)\n",
				ff->filename, ff->n1,ff->n2,ff->n3);

	sprintf(line,"BITPIX  = %20d   / %-45s",ff->bitpix,"Data type");
	FitsSetHeaderLine( ff, "BITPIX  = ", line);

	sprintf(line,"NAXIS   = %20d   / %-45s",ff->naxis,"Number of axes");
	FitsSetHeaderLine( ff, "NAXIS   = ", line);
	sprintf(line,"NAXIS1  = %20d   / %-45s",ff->n1,"number of pixels along 1.axis");
	FitsSetHeaderLine( ff, "NAXIS1  = ", line);
	sprintf(line,"NAXIS2  = %20d   / %-45s",ff->n2,"number of pixels along 2.axis");
	FitsSetHeaderLine( ff, "NAXIS2  = ", line);
	if( ff->naxis > 2)
	{	sprintf(line,"NAXIS3  = %20d   / %-45s",ff->n3,"number of pixels along 3.axis");
		FitsSetHeaderLine( ff, "NAXIS3  = ", line);
	}else	FitsSetHeaderLine( ff, "NAXIS3  = ", NULL);

	if( (bufbitpix < 0 && ff->bitpix > bufbitpix)
	 || (bufbitpix > 0 && ff->bitpix < bufbitpix && ff->bitpix > 0))
	{ FindMinMax( buff,bufbitpix,cubesz,&min,&max);

	  ff->bzero = (min+max)/2.0;
	  ff->bscale= (max-min);
	  if( ff->bitpix==BITPIX_UBYTE)			/** tape= 0...255 **/
	  {		ff->bzero= min;		ff->bscale /= 255.0;  }
	  else if( ff->bitpix==BITPIX_SHORT)	ff->bscale /= 65536.0 - 2.0;
	  else if( ff->bitpix==BITPIX_LONG)	ff->bscale /= 65536.0*65536.0 - 2.0;
	  else if( ff->bitpix==BITPIX_FLOAT)
	  {	if( max > FLT_MAX || min < -FLT_MAX)
		{ ff->bscale /= FLT_MAX * 0.99;
		  /** We don't use the full float-range, just to be on the safe side **/
		}
		else
		{ ff->bzero = 0.0;	/** data range fits into float **/
		  ff->bscale= 1.0;
		}
	  }
	  if((bufbitpix>0 && ff->bscale<1.0) || ff->bscale == 0.)
	  {	ff->bscale= 1.0;	ff->bzero = floor(ff->bzero);	}
	  else if( ff->bscale > 1.0)
		fprintf(stderr,"    WARNING: BSCALE = %g in %s!\n",
				ff->bscale, ff->filename);
	  printf("    min = %g, max = %g, BSCALE = %g, BZERO = %g\n",
			min,	max,		ff->bscale, ff->bzero);
	}
	else			/** fitsfile hat range >= buffer **/
	{ ff->bzero = 0.0;
	  ff->bscale= 1.0;
	}
	sprintf(line,"BSCALE  = %20g   / %-45s",ff->bscale,"Real = Tape * BSCALE + BZERO");
	FitsSetHeaderLine( ff, "BSCALE  = ", line);

	sprintf(line,"BZERO   = %20g   / %-45s",ff->bzero, "Real = Tape * BSCALE + BZERO");
	FitsSetHeaderLine( ff, "BZERO   = ", line);

	/*fprintf(stderr,"header size is %ld\n",ff->HeaderSize);*/
	/*fwrite( ff->Header,1,ff->HeaderSize,stdout);*/

	if (fwrite( ff->Header,1,ff->HeaderSize,ff->fp) < ff->HeaderSize)
	{	fprintf(stderr," Error while writing header\a\n");  err=1; }

	switch(ff->bitpix)
	{ case 8:
		for( i=0;  i<cubesz;  ++i)
		{ unsigned char data= (unsigned char)
				      ((DoubleOfAnyp(buff,bufbitpix,i)-ff->bzero) / ff->bscale);
		  if( fwrite( &data,sizeof(unsigned char),1,ff->fp)<1) break;
		}
		filesz= sizeof(unsigned char);
		break;
	  case 16:
		for( i=0;  i<cubesz;  ++i)
		{
		  union
		  {	short s;
			unsigned char b[2];
		  } data;
		  data.s= (short)((DoubleOfAnyp(buff,bufbitpix,i)-ff->bzero) / ff->bscale);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  {
			unsigned char hlp;
			hlp= data.b[0];  data.b[0]= data.b[1];  data.b[1]= hlp;
		  }
#endif
		  if( fwrite( &data.s,sizeof(short),1,ff->fp) < 1)  break;
		}
		filesz= sizeof(short);
		break;
	  case 32:
		for( i=0;  i<cubesz;  ++i)
		{
		  union
		  {	int l;
			unsigned char b[4];
		  } data;
		  data.l= (long)
			  ((DoubleOfAnyp(buff,bufbitpix,i)-ff->bzero) / ff->bscale);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  {
			unsigned char hlp;
			hlp= data.b[0];  data.b[0]= data.b[3];  data.b[3]= hlp;
			hlp= data.b[1];  data.b[1]= data.b[2];  data.b[2]= hlp;
		  }
#endif
		  /** FITS expects 4 bytes, so let's write 4 bytes and not sizeof(int) **/
		  if( fwrite( &data.l,4,1,ff->fp) < 1)  break;
		}
		filesz= sizeof(long);
		break;
	  case -32:
		for( i=0;  i<cubesz;  ++i)
		{
		  union
		  {	float f;
			unsigned char b[4];
		  } data;
		  data.f= (float)
			  ((DoubleOfAnyp(buff,bufbitpix,i)-ff->bzero) / ff->bscale);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  {
			unsigned char hlp;
			hlp= data.b[0];  data.b[0]= data.b[3];  data.b[3]= hlp;
			hlp= data.b[1];  data.b[1]= data.b[2];  data.b[2]= hlp;
		  }
#endif
		  if( fwrite( &data.f,sizeof(float),1,ff->fp) < 1)  break;
		}
		filesz= sizeof(float);
		break;
	  default:
	  case -64:
		for( i=0;  i<cubesz;  ++i)
		{
		  union
		  {	double d;
			unsigned char b[8];
		  } data;
		  data.d= ((DoubleOfAnyp(buff,bufbitpix,i)-ff->bzero) / ff->bscale);

#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  {
			unsigned char hlp;
			hlp= data.b[0];  data.b[0]= data.b[7];  data.b[7]= hlp;
			hlp= data.b[1];  data.b[1]= data.b[6];  data.b[6]= hlp;
			hlp= data.b[2];  data.b[2]= data.b[5];  data.b[5]= hlp;
			hlp= data.b[3];  data.b[3]= data.b[4];  data.b[4]= hlp;
		  }
#endif
		  if( fwrite( &data.d,sizeof(double),1,ff->fp) < 1)  break;
		}
		filesz= sizeof(double);
		break;
	}
	filesz *= cubesz;
	if( i<cubesz)
	{	fprintf(stderr," Error while writing!\a\n");	err=1;	}
	else
	if( (i= filesz%2880)!=0)	/** no multiple of 2880 **/
	{
		filesz += ff->HeaderSize;
		filesz += 2880 - i;		/** round up **/
		if( 0==fseek(ff->fp,filesz-sizeof(unsigned char),SEEK_SET))
		{	unsigned char data= 42;
			/*printf(" Padding with %ld bytes\n",2880-i);*/
			fwrite( &data,sizeof(unsigned char),1,ff->fp);
		} /** NOTE: this won't work for remote writes! **/
	}
	if(name)				/** don't close stdout!!! **/
	  if( rfopen_pipe? pclose(ff->fp) : fclose(ff->fp))
	  {	fprintf(stderr," Error while closing file!\a\n");  err=1;	}

	ff->fp= NULL;
	printf(" Writing of %s finished\n",ff->filename);
    }
    if(err && retrycnt<42)
    {	fprintf(stderr," Retry write?  ");
	if( !fgets(line,80,stdin))	/** no answer - maybe no user there **/
	{	FILE *mailfp;

	  fprintf(stderr,"no answer - I'll sleep a while (%d. retry)\n",retrycnt);
	  if( (retrycnt==0) && (mailfp= popen("mailx -s fitswrite-panic $USER\n","w")))
	  { fprintf(mailfp,
		"!!!PANIC!!!PANIC!!!PANIC!!!PANIC!!!PANIC!!!PANIC!!!PANIC!!!\n"
		"Write to %s failed,\n"
		"I'm going to sleep for a while and try it again.\n"
		"Please kill me if you don't want me to retry.\n\n"
		"				Your process no. %ld\n\n"
		"!!!PANIC!!!PANIC!!!PANIC!!!PANIC!!!PANIC!!!PANIC!!!PANIC!!!\n",
		name, (long) getpid());
	    pclose(mailfp);
	  }
	  sleep(3600);	++retrycnt;
	}
	else if( line[0]=='n' || line[0]=='N')  return 1;
    }
  }while(err && retrycnt<43);
  return 0;
}
