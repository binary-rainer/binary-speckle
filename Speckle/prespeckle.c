/* $Id: prespeckle.c,v 1.2 1997/06/20 09:40:34 koehler Exp koehler $
 *
 * Last Change: Thu Mar 24 22:47:33 2016
 *
 * 2012-Sep-14:
 * Bug in ReadFitsCube():
 * did not use pclose() if first file is a pipe
 *
 * 2012-MAR-07:
 * Fixed bug in ShiftAndAdd():
 * OutFits and MapFits "share" the header buffer
 * when OutFits is written, the header might have to be re-allocated
 * => MapFits' header pointer becomes corrupted
 *
 * $Log: prespeckle.c,v $
 * Revision 1.2  1997/06/20  09:40:34  koehler
 * ReadFitsCube() rewrite: Area spec., LUT
 *
 * Revision 1.1  1996/01/14  18:53:42  koehler
 * Initial revision
 */

#include "prespeckle.h"
#include <float.h>
//#include <values.h>
#include <unistd.h>
#ifdef __sun
/**# include <sys/byteorder.h>**/
#define ntohl(x)	(x)
#define ntohs(x)	(x)
#else
# include <netinet/in.h>
#endif

#undef DEBUG_FLAT

struct FitsFile *Fits= NULL;
char CurFilename[256];		/** struct FitsFile contains only a pointer **/
char histline[80];

int Verbose=0, Interact=1, DefBitpix=0;


/******************************************************************************/

static void RemoveFrames(char *opts)
{
	double sumfr, sum, sum2, low, high, cnt;
	float *dptr;
	long fr,i, framesz;

  if( !Fits) {	printf(" Nothing loaded - nothing to remove!\a\n");	return; }

  if( Fits->n3<=1)
  {		printf(" Only one frame - nothing to remove!\a\n");	return; }

  printf(" Remove frames:");

  sum= sum2= 0.0;	framesz= Fits->n1 * Fits->n2;
  for( fr=0;  fr<Fits->n3;  ++fr)
  { dptr= &(Fits->Buffer.f[fr * framesz]);
    for( i=0, sumfr= 0.0;  i<framesz;  ++i)	sumfr += dptr[i];
    sumfr /= (double) framesz;			/** sumfr = mean of ONE frame **/
    sum += sumfr;	sum2 += sumfr * sumfr;
  }
  cnt= (double) Fits->n3;	sum /= cnt;	/** mean **/
  sum2= sqrt((sum2 - cnt*sum*sum) / (cnt-1.0));	/** var. of single value **/
  low = sum - 3.0 * sum2;	high= sum + 3.0 * sum2;

  if(Verbose)	printf(" Mean value of frames: %.1f +- %.1f\n",sum,sum2);
  printf(" limits set to [ %.1f ; %.1f ] (3.0 sigma)\n",low,high);
  sprintf(histline,"prespeckle: rm frames, lim.[%.2g;%.2g]",low,high);
  FitsAddHistKeyw( Fits,histline);

  for( fr=0;  fr<Fits->n3;  ++fr)
  { dptr= &(Fits->Buffer.f[fr * framesz]);
    for( i=0, sumfr= 0.0;  i<framesz;  ++i)	sumfr += dptr[i];
    sumfr /= (double) framesz;			/** sumfr = mean of ONE frame **/
    if(Verbose)	printf(" Frame%4ld: mean %.1f\n",fr,sumfr);

    if( sumfr < low || sumfr > high)
    {	printf(" Frame%4ld: mean %.1f out of limits\n", fr,sumfr);
	--Fits->n3;		/** cube shrinks by one frame **/
	if( fr < Fits->n3)	/** here, n3 is the number of the last frame IN the cube **/
		memmove( dptr, dptr+framesz, (Fits->n3-fr) * framesz * sizeof(float));
    }
  }
  /** We leave the buffer untouched - it might be a bit oversized now **/
}


/*************************************************************************/

static void SumFrames(char *num)
{
	float *src, *dst;
	long  sfr,fr,i, n=2, framesz;

  if( !Fits)
  {	printf(" Nothing loaded - nothing to add up!\a\n");  return;	}

  if(num)
    if( (n= atol(num)) <= 0)
    {	fprintf(stderr," Bad number %ld\n",n);	return; }

  dst= Fits->Buffer.f;
  framesz= Fits->n1 * Fits->n2;
  for( sfr=1;  sfr < Fits->n3; )
  { src= &(Fits->Buffer.f[sfr * framesz]);
    if( src!=dst)  memcpy(dst,src,framesz*sizeof(float));

    for( ++sfr, fr=1;  fr<n && sfr<Fits->n3;  ++sfr, ++fr)
    {	src += framesz;
	for( i=0;  i<framesz;  ++i)	dst[i] += src[i];
    }
    dst += framesz;
  }
  Fits->n3 /= n;
  /** We leave the buffer untouched - it will be a bit oversized now **/
}


/*************************************************************************/

static void SubtractFrame(char *file)
{
	struct FitsFile *SkyFits=NULL;
	float *subuff=NULL;
	long i,fr, framesz;

  if( !Fits) {	printf(" Nothing loaded - nothing to subtract!\a\n");	return; }

  printf(" Subtracting frame: ");
  framesz= Fits->n1 * Fits->n2;

  if( file)
  {	if( !(SkyFits= FitsOpenR(file)))  return;
	if( (SkyFits->n1 != Fits->n1) || (SkyFits->n2 != Fits->n2))
	{	fprintf(stderr," Sorry, frame sizes not equal.\a\n");
		FitsClose(SkyFits);	return;
	}
	/*no overflow check: sprintf(histline,"prespeckle: subtract sky file %s",file);*/
	strcpy( histline, "prespeckle: subtract ");
	strncpy(histline+21, file, 59); 	/** do not write beyond end of buffer **/
	printf("file %s\n",file);
  }
  if( !(subuff= AllocBuffer( framesz*sizeof(float),"frame to subtract")))
  {	if( SkyFits)	FitsClose(SkyFits);
	return;
  }
  if( SkyFits)
  {	if( SkyFits->naxis > 2)
		fprintf(stderr," Warning: reading only first frame.\a\n");

	SkyFits->flags &= ~(FITS_FLG_EOF|FITS_FLG_ERROR);
	for( i=0;  i<framesz;  ++i)
	{ subuff[i]= (float) (*SkyFits->ReadDblPixel)(SkyFits);

	  if( SkyFits->flags & (FITS_FLG_EOF|FITS_FLG_ERROR))
	  {	fprintf(stderr," Not enough pixels to read!\a\n");
		FitsClose(SkyFits);	return;
	  }
	}
	FitsClose(SkyFits);
  }
  else	/** no sky, so use mean of cube **/
  {	float num= (float) Fits->n3;

	sprintf(histline,"prespeckle: subtract mean frame of cube");
	printf("mean frame of data itself\n");
	for( i=0;  i<framesz;  ++i)	subuff[i]= 0.0;
	for( fr=0;  fr<Fits->n3;  ++fr)
	    for( i=0;  i<framesz;  ++i)
		subuff[i] += Fits->Buffer.f[fr*framesz + i];

	for( i=0;  i<framesz;  ++i)	subuff[i] /= num;
  }
  FitsAddHistKeyw( Fits,histline);
  /***** jetzt aber los *****/

  for( fr=0;  fr<Fits->n3;  ++fr)
     for( i=0;  i<framesz;  ++i)
	Fits->Buffer.f[fr*framesz + i] -= subuff[i];

  /***** das war's auch schon ****/
  free(subuff);
}


/*************************************************************************/

static void FlatfieldCorr(char *flatname)
{
	struct FitsFile *FlatFits=NULL;
	float val, sum, *flatbuf= NULL;
	long i,x,y,fr, framesz;

  if( !Fits) {	printf(" Nothing loaded - nothing to flat!\a\n");	return; }

  if( !(FlatFits= FitsOpenR(flatname)))  return;

  if( (FlatFits->SA_xmin > Fits->SA_xmin) || (FlatFits->SA_ymin > Fits->SA_ymin)
   || (FlatFits->SA_xmax < Fits->SA_xmax) || (FlatFits->SA_ymax < Fits->SA_ymax))
  {
    fprintf(stderr,
	"prespeckle: WARNING! Flatfield \"%s\" does not cover entire data frame!\a\n",
	flatname);
    fprintf(stderr,
	    "            SaveArea of data: %d - %d, %d - %d",
	    Fits->SA_xmin, Fits->SA_xmax, Fits->SA_ymin, Fits->SA_ymax);
    fprintf(stderr,
	    "            SaveArea of flat: %d - %d, %d - %d",
	    FlatFits->SA_xmin, FlatFits->SA_xmax, FlatFits->SA_ymin, FlatFits->SA_ymax);
  }
  printf(" Dividing by flatfield %s:\n",flatname);
  sprintf(histline,"prespeckle: flat %.63s",flatname);	/* writing more chars would be deadly! */
  framesz= Fits->n1 * Fits->n2;

  if( !(flatbuf= AllocBuffer(framesz * sizeof(float),"flatfield")))
  {	FitsClose(FlatFits);	return;		}

  for( i=0;  i<framesz;  ++i)	flatbuf[i]= 1.0;

  sum= 0.0;
  for( y=0;  y<FlatFits->n2;  ++y)
    for( x=0;  x<FlatFits->n1;  ++x)
    {		long xfr, yfr;	/** pos in data frame **/

	val= (float) FitsReadDouble(FlatFits);
	xfr= x + FlatFits->SA_xmin - Fits->SA_xmin;
	yfr= y + FlatFits->SA_ymin - Fits->SA_ymin;

	if((xfr>=0) && (xfr<Fits->n1) && (yfr>=0) && (yfr<Fits->n2))
	{ if( val==0.0)
	  { sum += 1.0;
	    fprintf(stderr,
		"prespeckle: WARNING! Flatfield at (%ld,%ld) is 0.0, using 1.0!\a\n",x,y);
	  }
	  else
	  { sum += val;
	    flatbuf[xfr + Fits->n1 * yfr] = val;
	  }
	}
    }
  printf("	mean of flatfield is %g\n",sum/(float)framesz);

#ifdef DEBUG_FLAT
  printf("	writing Testflat.fits for debugging\n");
  FlatFits->n1= Fits->n1;
  FlatFits->n2= Fits->n2;
  FitsWriteAny( "Testflat.fits",FlatFits,flatbuf,-32);
#endif

  /***** and now divide the data *****/

  for( fr=0;  fr<Fits->n3;  ++fr)
    for( i=0;  i<framesz;  ++i)
	Fits->Buffer.f[fr*framesz + i] /= flatbuf[i];

  FitsAddHistKeyw( Fits,histline);
  free(flatbuf);
  FitsClose(FlatFits);
}


/*************************************************************************/

static void SubtractBaseline(char *coord)
{
	float *frptr;
	double base, cnt, minb,maxb;
		/** we add small pixels to a big sum, so we need maximum precision **/
	int xmin=-1, xmax=-1, ymin=-1, ymax=-1;
	long fr,x,y;

  if( !Fits) {	printf(" Nothing loaded - nothing to subtract!\a\n");	return; }

  printf(" Subtracting baseline: ");

  if( coord && *coord)
  {	sscanf(coord,"%i %i %i %i", &xmin, &ymin, &xmax, &ymax);
	printf("excluding area (%d,%d)-(%d,%d)\n", xmin,ymin, xmax,ymax);
	sprintf(histline,"prespeckle: subtract base (excl. %d,%d-%d,%d)",
							xmin,ymin, xmax,ymax);
  }
  else
  {	printf("using whole frame\n");
	sprintf(histline,"prespeckle: subtract base (whole frame)");
  }
  FitsAddHistKeyw( Fits,histline);

  minb=  DBL_MAX;	maxb= -DBL_MAX;

  for( fr=0;  fr<Fits->n3;  ++fr)
  {	frptr= Fits->Buffer.f + (fr * Fits->n1 * Fits->n2);
	base= cnt= 0.0;
	for( x=0;  x<Fits->n1;  ++x)
	  for( y=0;  y<Fits->n2;  ++y)
	    if( x<xmin || x>xmax || y<ymin || y>ymax)
	    {	base += frptr[x + Fits->n1*y];	cnt += 1.0;	}
	if(Verbose)	printf(" Frame %ld: sum = %g, cnt = %g\n",fr,base,cnt);
	base /= cnt;
	if( minb > base)  minb= base;
	if( maxb < base)  maxb= base;

	for( x=0;  x<Fits->n1*Fits->n2;  ++x)	frptr[x] -= base;
  }
  printf(" subtracted baselines: min = %g, max = %g\n",minb,maxb);
}


/*************************************************************************/

static void SubtractBackground(char *coord)
{
	float *frptr;
	double b0, bx, by, cnt, b0m, bxm, bym;
	double SumX, SumY, SumXX, SumXY, SumYY, SumP, SumXP, SumYP;
		/** we add small pixels to a big sum, so we need maximum precision **/
	double D_xx, D_xy, D_yy, D_xp, D_yp;
	int xmin=-1, xmax=-1, ymin=-1, ymax=-1;
	long fr,x,y;

  if( !Fits) {	printf(" Nothing loaded - nothing to subtract!\a\n");	return; }

  printf(" Subtracting background: ");

  if( coord && *coord)
  {	sscanf(coord,"%i %i %i %i", &xmin, &ymin, &xmax, &ymax);
	printf("excluding area (%d,%d)-(%d,%d)\n", xmin,ymin, xmax,ymax);
	sprintf(histline,"prespeckle: subtract bg (excl. %d,%d-%d,%d)",
							xmin,ymin, xmax,ymax);
  }
  else
  {	printf("using whole frame\n");
	sprintf(histline,"prespeckle: subtract bg (whole frame)");
  }
  FitsAddHistKeyw( Fits,histline);

  b0m= bxm= bym= 0.0;

  for( fr=0;  fr<Fits->n3;  ++fr)
  { frptr= Fits->Buffer.f + (fr * Fits->n1 * Fits->n2);
    SumX= SumY= SumXX= SumXY= SumYY= SumP= SumXP= SumYP= cnt= 0.0;

    for( x=0;  x<Fits->n1;  ++x)
      for( y=0;  y<Fits->n2;  ++y)
	if( x<xmin || x>xmax || y<ymin || y>ymax)
	{
	  SumX  += (double) x;		SumY  += (double) y;
	  SumXX += (double)(x*x);	SumYY += (double)(y*y);
	  SumXY += (double)(x*y);
	  SumP  += (double)    frptr[x + Fits->n1*y];
	  SumXP += (double)x * frptr[x + Fits->n1*y];
	  SumYP += (double)y * frptr[x + Fits->n1*y];
	  cnt += 1.0;
	}
    if(Verbose>2)
	printf(	" Frame %ld:	N_pix= %g\n"
		"	SumX = %g	SumY = %g\n"
		"	SumXX= %g	SumYY= %g	SumXY= %g\n"
		"	SumP = %g	SumXP= %g	SumYP= %g\n",
		fr, cnt, SumX,SumY,SumXX,SumYY,SumXY, SumP,SumXP,SumYP );

    D_xx= SumX * SumX - cnt * SumXX;
    D_xy= SumX * SumY - cnt * SumXY;
    D_yy= SumY * SumY - cnt * SumYY;
    D_xp= SumX * SumP - cnt * SumXP;
    D_yp= SumY * SumP - cnt * SumYP;

    if(Verbose>1)
	printf(	" Frame %ld:\n"
		"	D_xx= %g	D_yy= %g	D_xy= %g\n"
		"	D_xp= %g	D_yp= %g\n",
		fr, D_xx, D_yy, D_xy, D_xp, D_yp);

    bx = (D_xp*D_yy - D_yp*D_xy) / (D_xx*D_yy - D_xy*D_xy);	bxm += bx;
    by = (D_xp*D_xy - D_yp*D_xx) / (D_xy*D_xy - D_yy*D_xx);	bym += by;
    b0 = (SumP - bx*SumX - by*SumY) / cnt;			b0m += b0;

    if(Verbose)  printf(" Frame %ld: %.3g + %.3g * x + %.3g * y\n",fr,b0,bx,by);

    for( x=0;  x<Fits->n1;  ++x)
      for( y=0;  y<Fits->n2;  ++y)
	frptr[x + Fits->n1*y] -= b0 + bx * (double)x + by * (double)y;
  }
  printf(" subtracted background: mean b_0 = %g\n", b0m/Fits->n3);
  printf("                        mean b_x = %g\n", bxm/Fits->n3);
  printf("                        mean b_y = %g\n", bym/Fits->n3);
}


/*************************************************************************/

static void ExtractSubframe(char *coord)
{
	float *src, *dest;
	int  xmin, xmax, ymin, ymax;
	long fr,x,y;

  if( !Fits) {	printf(" Nothing loaded - nothing to extract!\a\n");	return; }

  printf(" Extracting Subframe: ");

  if( !coord)
  {	fprintf(stderr,"Please specify the desired area!\a\n");		return; }

  sscanf(coord,"%i %i %i %i", &xmin, &ymin, &xmax, &ymax);

  if( xmax >= Fits->n1)	 { xmin -= xmax-(Fits->n1-1);  xmax= Fits->n1-1;  }
  if( ymax >= Fits->n2)	 { ymin -= ymax-(Fits->n2-1);  ymax= Fits->n2-1;  }
	/** max'-min' = (size-1)-min' == max-min => min' = (size-1)-max+min **/

  if( xmin<0)	{ xmax -= xmin;	 xmin=0;  }
  if( ymin<0)	{ ymax -= ymin;	 ymin=0;  }

  printf("area (%d,%d)-(%d,%d)\n", xmin,ymin, xmax,ymax);
  sprintf(histline,"prespeckle: extr.subframe (%d,%d)-(%d,%d)",	xmin,ymin, xmax,ymax);
  FitsAddHistKeyw( Fits,histline);

  dest= Fits->Buffer.f;		/** we copy in one buffer **/

  for( fr=0;  fr<Fits->n3;  ++fr)
  {
    for( y=ymin;  y<=ymax;  ++y)
    {	src= Fits->Buffer.f + ((fr*Fits->n2 + y) * Fits->n1);

	for( x=xmin;  x<=xmax;  ++x)	*dest++ = src[x];
    }
  }
  Fits->n1= xmax+1 - xmin;	/** incl. xmax **/
  Fits->n2= ymax+1 - ymin;	/** incl. xmax **/
  /** We leave the buffer untouched - it might be a bit oversized now **/
}


/*************************************************************************/
/**				File I/O				**/
/*************************************************************************/

/***************************** TestFile ***************************************/

static short TestFile(char *fname)	/** returns 0 if ok to write 'fname' **/
{
	FILE *fp;
	char line[80];

  if( Interact==0 || !(fp= fopen(fname,"r")))	return 0;	/** does not exist, so GA **/
  fclose(fp);

  printf("\n \"%s\" already exists, do You want to overwrite it?  \a",fname);
  if( fgets(line,79,stdin) && line[0]!='y')	return 1;
			/** user is here and did not confirme write **/
  printf(" Ok, it's Your data.\n");
  return 0;
}


/***************************** ReadFitsCube ***********************************/

static float ReadPixel(struct FitsFile *ff,float *scaleLUT)
{
  /** in FitsOpenR.c: **/
	extern unsigned char ReadUByte(FILE *fp);
	extern unsigned short ReadUShort(FILE *fp);
	extern short ReadShort(FILE *fp);

  if(scaleLUT)
  {	float val;

	if( ff->bitpix==8)
	{	val= scaleLUT[ ReadUByte(ff->fp) ];	}
	else
	if( ff->flags & (FITS_FLG_SHARP|FITS_FLG_NEW_SHARP))
	{	val= scaleLUT[ ReadUShort(ff->fp) ];	}
	else
	{	val= scaleLUT[ ReadShort(ff->fp) + 0x8000L]; }	/** min -> 0 **/

	if( feof(ff->fp))	ff->flags |= FITS_FLG_EOF;
	if( ferror(ff->fp))	ff->flags |= FITS_FLG_ERROR;
	return val;
  }
  else return (float) (*ff->ReadDblPixel)(ff);
}


char WindMill[]= "|/-\\";

static char *get_limit(char *str, char endch, short *limp)
{
	*limp= atoi(str);
	return strchr(str,endch);
}

struct InputFile {
	char *fname;
	short xmin,xmax, ymin,ymax, frmin,frmax;
};


static void ReadFitsCube(char *cmdline)
{
	struct InputFile *File;
	struct FitsFile *ff;
	float	*bufptr, *scaleLUT=NULL;
	char	*str, sharp[10], hdrline[88];
	long	cubesz, wordsz, fl,fr,x,y;
	short	partial=0, xmin,xmax,ymin,ymax,frmin,frmax;

  if( !cmdline || *cmdline==0)
  {	printf(" Please tell me what to read.\a\n");	return;	}

  if(Fits)	FitsClose(Fits);

  /** count spaces in cmdline = max. number of files - 1 **/
  for( fl=1, str=cmdline;  (str= strchr(++str,' '));  fl++) ;

  if( !(File= AllocBuffer(fl * sizeof(struct InputFile), "List of Files"))) return;

  CurFilename[0]= 0;
  for(fl=0, str=cmdline, fr=0;  str!=NULL; )
  {
    File[fl].fname= str;
    if((str= strchr(str,' ')))		/** something after this filename? **/
    {	*str++ = 0;
	while(*str && *str==' ')  ++str;
    }
    if( !(ff= FitsOpenR(File[fl].fname)))	continue;	/** next file **/

    File[fl].xmin = 0;	File[fl].xmax = ff->n1-1;
    File[fl].ymin = 0;	File[fl].ymax = ff->n2-1;
    File[fl].frmin= 0;	File[fl].frmax= ff->n3-1;

    if( CurFilename[0]==0) {
	strcpy(CurFilename,File[fl].fname);		/** save filename somewhere save **/
	(ff->flags & FITS_FLG_PIPE)? pclose(ff->fp) : fclose(ff->fp);
	ff->fp= NULL;
	ff->filename= CurFilename;	/** was pointing to File[fl].fname **/
	Fits= ff;	/** and keep the header - would be cleaner to copy the hdr and use FitsClose()!!! **/
    }
    else FitsClose(ff);

    if( str && *str >= '0' && *str <= '9')		/** area given! **/
    { partial=1;
      if(  (str= get_limit(  str, '-', &File[fl].xmin))
	&& (str= get_limit(++str, ':', &File[fl].xmax))
	&& (str= get_limit(++str, '-', &File[fl].ymin))
	&& (str= get_limit(++str, ':', &File[fl].ymax))
	&& (str= get_limit(++str, '-', &File[fl].frmin)))
	    str= get_limit(++str, ' ', &File[fl].frmax);
      while(str && *str && *str==' ')  ++str;
    }
    if( fl>0 && ((File[fl].xmax - File[fl].xmin) != (xmax-xmin)
	      || (File[fl].ymax - File[fl].ymin) != (ymax-ymin)))
    {	printf("%s: size differs, skipping file\a\n",File[fl].fname); }
    else
    {	if( fl==0)		/** 1.file **/
	{ xmin = File[0].xmin;	xmax = File[0].xmax;
	  ymin = File[0].ymin;	ymax = File[0].ymax;
	}
	fr += File[fl].frmax + 1 - File[fl].frmin;
	++fl;
    }
  }
  File[fl].fname = NULL;

  cubesz= (long)(xmax +1 - xmin) * (long)(ymax +1 - ymin) * (long)fr;
  wordsz= FitsWordSize(Fits);
  if( !(bufptr= AllocBuffer(cubesz * sizeof(float),"data cube")))
  {	FitsClose(Fits);  Fits= NULL;	return; }
  Fits->Buffer.f= bufptr;
  Fits->n1 = xmax +1 - xmin;
  Fits->n2 = ymax +1 - ymin;
  Fits->n3 = fr;
  if(fr>1)  Fits->naxis= 3;

  /** Order is important in the following SA-stuff! **/
  Fits->SA_xmax= Fits->SA_xmin+xmax;
  Fits->SA_ymax= Fits->SA_ymin+ymax;
  Fits->SA_xmin += xmin;
  Fits->SA_ymin += ymin;

  if(partial)			/** area was given, set SAVEAREA **/
  {	memset( hdrline,' ',80);
	sprintf(hdrline,"SAVEAREA= '[%hd:%hd,%hd:%hd]' ",
			Fits->SA_xmin, Fits->SA_xmax, Fits->SA_ymin, Fits->SA_ymax);
	if( (str= strchr(hdrline,0)))  *str= ' ';
	FitsSetHeaderLine(Fits, "SAVEAREA= ", hdrline);
  }
  if( DefBitpix)  Fits->bitpix= DefBitpix;
  /*
   * read data from all files
   */
  for( fl=0;  File[fl].fname;  ++fl)
  {
    xmin = File[fl].xmin;	xmax = File[fl].xmax;
    ymin = File[fl].ymin;	ymax = File[fl].ymax;
    frmin= File[fl].frmin;	frmax= File[fl].frmax;

    printf(" reading %s, area %d-%d:%d-%d:%d-%d...\n",
		File[fl].fname, xmin,xmax, ymin,ymax, frmin,frmax);

    if( !(ff= FitsOpenR(File[fl].fname)))	continue;	/** next file **/

    if( ff->bitpix==8 || ff->bitpix==16)		/** scale-LUT possible **/
    {
	float val;
	long scLUTsz= 1 << ff->bitpix;

	if((scaleLUT= AllocBuffer(scLUTsz*sizeof(float),"bscale lookup table")))
	{ val= 0.0;
	  if( ff->bitpix==16 && !(ff->flags&(FITS_FLG_SHARP|FITS_FLG_NEW_SHARP)))
		val -= 0x8000;
	  for( x=0;  x<scLUTsz;  ++x, val+=1.0)
		scaleLUT[x]= val * ff->bscale + ff->bzero;
	}
    }

    /********** Wenn SHARP, dann statistics frame skippen **********/
    if( ff->flags & FITS_FLG_SHARP)
	fread( ff->Buffer.c, FitsWordSize(ff), ff->n1*ff->n2, ff->fp);

    if( ff->flags & FITS_FLG_NEW_SHARP)
	fprintf(stderr," SHARP file in disguise as FITS!\n");

    /********** Daten einlesen und gewuenschtes Area in Buffer **********/

    ff->flags &= ~(FITS_FLG_EOF|FITS_FLG_ERROR);
    for( fr=0; fr<ff->n3 && fr<=frmax; ++fr)
    { if( ff->flags & FITS_FLG_SHARP)	fread( sharp,1,8,ff->fp);
      for( y=0;  y<ff->n2;  ++y)
      { for( x=0; x<ff->n1;  ++x)
	{
	  if( xmin<=x && x<=xmax && ymin<=y && y<=ymax && frmin<=fr)
	  {
		*bufptr++ = ReadPixel(ff,scaleLUT);

		if( ff->flags & (FITS_FLG_EOF|FITS_FLG_ERROR))
		{ fprintf(stderr,"\n Error while reading %s!\a\n",ff->filename);
		  FitsClose(ff);    free(scaleLUT);
		  FitsClose(Fits);  Fits= NULL;
		  return;
		}
	  }
	  else
	  {	union anyvar dummy;
		fread( &dummy,wordsz,1,ff->fp);
	  }
	}
      }
      if(Verbose)	printf(" %c\r",WindMill[fr & 3]);
    }
    FitsClose(ff);
    free(scaleLUT);

    if(fl>0)
    {	sprintf(hdrline,"appended %s",File[fl].fname);
	FitsAddHistKeyw( Fits, hdrline);
    }
  }
}


/***************************** WriteFitsCube **********************************/

static void WriteFitsCube(char *fname)
{
  if( !Fits)	printf(" nothing to write\n");

  if( !fname || *fname==0)
  {	printf(" Please tell me where to write to.\a\n");  return;	}

  if( TestFile(fname))
  {	printf(" nothing written.\n");	return;	}	/** file exists **/

  strcpy(CurFilename,fname);
  FitsWriteAny( CurFilename,Fits,Fits->Buffer.f,-32);	/** will report errors itself **/
}


/***************************** WriteMeanFits **********************************/

static void WriteMeanFits(char *fname)
{
	struct FitsFile MnFits;
	double sum;
	char line[88];
	long framesz, fr,i;

  if( !Fits)	printf(" nothing to write\n");

  if( !fname || *fname==0)
  {	printf(" Please tell me where to write to.\a\n");  return;	}

  if( TestFile(fname))
  {	printf(" nothing written.\n");	return;	}	/** file exists **/

  MnFits= *Fits;	/** make a copy **/
  MnFits.naxis= 2;	MnFits.n3= 1;

  framesz= Fits->n1 * Fits->n2;
  if( !(MnFits.Buffer.d= AllocBuffer((framesz*sizeof(double)),"mean frame")))
	return;

  /** meine Fits-Routinen gehen davon aus, dass der Header separat allociert
	wurde, d.h. man kann ihn rumkopieren und free()en, soviel man will!	*/
  if( !(MnFits.Header= AllocBuffer(MnFits.HeaderSize+10,"FITS Header")))
  {	free(MnFits.Buffer.d);	return; 	}

  memcpy( MnFits.Header, Fits->Header, MnFits.HeaderSize);	/** copy Header **/
  FitsAddHistKeyw( &MnFits,"prespeckle: mean frame");

  for( sum= 0.0, i=0;  i<framesz;  ++i)
  {	MnFits.Buffer.d[i]= 0.0;
	for( fr=0;  fr<Fits->n3;  ++fr)
		MnFits.Buffer.d[i] += Fits->Buffer.f[ fr * framesz + i ];
	MnFits.Buffer.d[i] /= (double)Fits->n3;
	sum += MnFits.Buffer.d[i];
  }
  sprintf(line,"NCOADDS = %20d   / %-45s",
			Fits->n3, "number of frames added for this average");
  FitsSetHeaderLine( &MnFits,"NCOADDS = ",line);

  sprintf(line,"INTEGRAL= %20g   / %-45s", sum, "sum of pixel values");
  FitsSetHeaderLine( &MnFits, "INTEGRAL= ", line);

  FitsAddHistKeyw( &MnFits, "prespeckle: computed mean frame");
  FitsWrite(fname,&MnFits,MnFits.Buffer.d);	/** will report errors itself **/
}


/****************************** Shift And Add *********************************/

static void FindMaxPos( float *buf, long xbord, long ybord,
			long xstart, long xend, long ystart, long yend,
			unsigned long *xmax, unsigned long *ymax )
{	float max;
	unsigned long x, y;

  if( xstart<0)	xstart=0;	if( xend > Fits->n1)	xend= Fits->n1;
  if( ystart<0)	ystart=0;	if( yend > Fits->n2)	yend= Fits->n2;

  max= buf[xstart + ystart*Fits->n1];	*xmax= xstart;	*ymax= ystart;

  for( y=ystart;  y<yend;  ++y)
    for( x=xstart;  x<xend;  ++x)
	if( max < buf[x + Fits->n1*y])
	{	max= buf[x + Fits->n1*y];	*xmax= x;  *ymax= y;	}
}

static void FindCenter(float *buf, unsigned long *centx, unsigned long *centy)
{
	long   x,y;
	double sum,sumx,sumy, help;

  sum= sumx= sumy= 0.0;
  for( y=0;  y<Fits->n2;  ++y)
    for( x=0;  x<Fits->n1;  ++x)
    {	help= buf[x + y*Fits->n1];
	sum  += help;
	sumx += (double)x * help;
	sumy += (double)y * help;
    }
  *centx= (unsigned long)(sumx/sum + 0.5);
  *centy= (unsigned long)(sumy/sum + 0.5);
}

static void Help_saa(void)
{
  printf(" USAGE: saa [-help]\n"
	"  or:	saa [-center] [-xbr x] [-ybr y] OutName [MapName]\n"
	" where:\t-center\t	uses center of light instead of maximum\n"
	"	-xbord x	width of unused strips at left/right side [2]\n"
	"	-ybord y	width of unused strips at top/bottom side [2]\n"
	"	-xbr x		radius of search box in x [10]\n"
	"	-ybr y		radius of search box in y [10]\n"
	"	OutName		gives name of output [saa.fits]\n"
	"	MapName		writes map with positions of max pixels\n");
}

static void ShiftAndAdd(char *opts)
{
	struct FitsFile OutFits, MapFits;
	char *OutName, *MapName= NULL;
	short Center=0;
	unsigned long framesz, size, xbord=2, ybord=2, xbr=10,ybr=10;
	unsigned long fr,x,y,x0,y0,xto,yto;

  if( opts && !strncmp(opts,"-h",2))	{ Help_saa();	return;	}

  if( !Fits) {	printf(" nothing to shift\n");  return; }

  /** Parse Opts **/
  {	char *argp, *next=NULL;

    for( argp= strtok(opts," \t");  argp;  )
    {
	if( !strncmp(argp,"-c",2))  { Center= 1; }
	else
	{  next= strtok(NULL," \t");	/** remaining opts have an argument **/

		if( !strcmp(argp,"-xbord") && next)  xbord= atol(next);
	   else if( !strcmp(argp,"-ybord") && next)  ybord= atol(next);
	   else if( !strcmp(argp,"-xbr")   && next)  xbr= atol(next);
	   else if( !strcmp(argp,"-ybr")   && next)  ybr= atol(next);
	   else if( argp[0]=='-')
	   {	printf(" Unknown option: %s\a\n",argp);
		Help_saa();	return;
	   }
	   else	break;
	}
	argp= strtok(NULL," \t");
    }
    OutName= argp? argp : "saa.fits";
    if(next)  MapName= next;
  }
  if( TestFile(OutName) || (MapName && TestFile(MapName)))
  {	printf(" nothing written.\n");	return;	}	/** file exists **/

  OutFits= *Fits;	/** make a copy **/
  OutFits.naxis= 2;	OutFits.n3= 1;

  if(MapName)	{ MapFits= OutFits;	MapFits.bitpix= BITPIX_SHORT;	}

  framesz= OutFits.n1 * OutFits.n2;
  size = sizeof(double);	if(MapName) size += sizeof(short);

  if( !(OutFits.Buffer.d= AllocBuffer(framesz*size,"saa frame")))  return;

  /** meine Fits-Routinen gehen davon aus, dass der Header separat allociert
	wurde, d.h. man kann ihn rumkopieren und free()en, soviel man will!	*/
  if( !(OutFits.Header= AllocBuffer(Fits->HeaderSize+10,"FITS Header")))
  {	free(OutFits.Buffer.d);		return; 	}

  memcpy( OutFits.Header, Fits->Header, OutFits.HeaderSize);	/** copy Header **/
  FitsAddHistKeyw( &OutFits, Center? "prespeckle: Shift And Add (center)" :
					"prespeckle: Shift And Add (max)" );
  if( MapName)
  {	MapFits.Buffer.s= (short *)(OutFits.Buffer.d + framesz);  }

  /** initialize buffers by copying first frame **/
  if(Center) FindCenter( Fits->Buffer.f, &xto, &yto);
	else FindMaxPos( Fits->Buffer.f, xbord,ybord, 0,Fits->n1, 0,Fits->n2, &xto,&yto);

  x0= xto;  y0= yto;	/** prepare for search around max **/

  if( Verbose)  fprintf(stderr," frame   0: (%3ld,%3ld)\n",xto,yto);

  for( x=0;  x<framesz;  ++x)
  {	OutFits.Buffer.d[x]= Fits->Buffer.f[x];
	if(MapName)   MapFits.Buffer.s[x]= 0;
  }
  if( MapName)  ++MapFits.Buffer.s[ xto + Fits->n1*yto];

  /** and now loop over rest of file **/
  for( fr=1;  fr<Fits->n3;  ++fr)
  {	long dx,dy;
	float *FrmBuf= &Fits->Buffer.f[fr*framesz];

    if(Center) FindCenter( FrmBuf, &x0,&y0);
	else   FindMaxPos( FrmBuf, xbord,ybord, x0-xbr,x0+xbr, y0-ybr,y0+ybr, &x0,&y0);
		/** We search in the vicinity of the last frame's max **/

    if( MapName)  ++MapFits.Buffer.s[ x0 + Fits->n1*y0];
    if( Verbose)  fprintf(stderr," frame%4ld: (%3ld,%3ld)\n",fr,x0,y0);
    dx= x0-xto;	 dy= y0-yto;

    for( y=0;  y<Fits->n2;  ++y)
      for( x=0;  x<Fits->n1;  ++x)
	OutFits.Buffer.d[ ((x-dx)%Fits->n1) + Fits->n1 * ((y-dy)%Fits->n2)]
			+= FrmBuf[ x + Fits->n1*y];
  }

  /** Write the stuff to file **/
  FitsWriteAny( OutName, &OutFits, OutFits.Buffer.d, BITPIX_DOUBLE);

  if( MapName)
  {	char *hdrline;

    /** do not copy Header ptr earlier, because FitsWriteAny() might Re-allocate it! **/
    MapFits.Header    = OutFits.Header;
    MapFits.HeaderSize= OutFits.HeaderSize;

    if( (hdrline= FitsFindEmptyHdrLine( &MapFits)))
	memcpy( hdrline,"COMMENT = Positions of brightest pixel",38);
    FitsWriteAny( MapName, &MapFits, MapFits.Buffer.s, BITPIX_SHORT);
  }
  free(OutFits.Buffer.d);
  free(OutFits.Header);
}


/**************************** Header-Manipulation *****************************/

static void Header(char *opt)
{
	char *ptr, *keyw=NULL;

  if( !Fits)  {	printf(" no file - no header\n");  return; }

  if( opt && *opt)
  { keyw= opt;
    if( (opt= strchr(keyw,'=')) || (opt= strchr(keyw,'+')))
    {	char keyword[12], line[82], code= *opt;

	*opt= 0;
	sprintf(keyword,"%-8.8s= ",keyw);
	sprintf(line,"%-8.8s= %-70.70s",keyw,opt+1);

	if( code=='=')
	{ FitsSetHeaderLine(Fits,keyword,line);	}
	else
	{ if( !(ptr= FitsFindEmptyHdrLine(Fits)))
	  {
	    if( !(ptr= findkeyword(Fits,"COMMENT   ")))
	    {	printf(" no space in header for %s...\n",keyw);	 return; }
	    ptr -= 10;	/** findkeyword() liefert ptr auf value! **/
	  }
	  memcpy(ptr,line,80);
	}
	return;
    }
  }

  for( ptr= Fits->Header; ;  ptr += 80)
  {	if( !keyw || (strncmp(ptr,keyw,strlen(keyw))==0))
		printf("%.80s\n",ptr);
	if( strncmp(ptr,"END     ",8)==0)  break;
  }
}

/********************************* Invert **********************************/

static void InvertCube(char *opt)
{
	long i;

  if( !Fits)	printf(" nothing to pervert\n");

  for( i=0;  i< Fits->n1*Fits->n2*Fits->n3;  ++i)
		Fits->Buffer.f[i]= 1.0 / Fits->Buffer.f[i];
}


/********************************* Integrate **********************************/

static void IntegrateCube(char *opt)
{
	double sum;
	long i;

  if( !Fits)	printf(" nothing to integrate\n");

  for( sum=0.0, i=0;  i< Fits->n1*Fits->n2*Fits->n3;  ++i)  sum += Fits->Buffer.f[i];
  printf(" Sum of all pixels is %g\n",sum);
}


/******************************************************************************/

static void do_command(char *cmd)
{
	char *spc;

  while( *cmd==' ' || *cmd=='\t')  ++cmd;	/** skip leading spc **/
  spc = cmd+strlen(cmd)-1;
  if( *spc=='\n')  *spc= 0;	/** rm trailing newline **/

  if( *cmd=='q' || !strncmp(cmd,"exit",4) || !strncmp(cmd,"bye", 3))	/**** q = exit = bye ****/
  {
	printf(" Good Bye!\n\n");  fflush(stdout);	exit(0);
  }
  for( spc= strchr(cmd,' ');  spc && *spc && *spc==' ';  ++spc)	;

       if( !strncmp(cmd,"int",3))  IntegrateCube(spc);		/** int = integrate **/
  else if( !strncmp(cmd,"inv",3))  InvertCube(spc);		/** inv = invert    **/
  else if( !strncmp(cmd,"rmf",3))  RemoveFrames(spc);		/** rmf = rmframes  **/
  else if( !strncmp(cmd,"sum",3))  SumFrames(spc);		/** sum = sumframes **/
  else if( !strncmp(cmd,"bpm",3) || !strncmp(cmd,"badpixm",7))
				   CreateBadpixmask(spc);	/** bpm = badpixmask **/
  else if( !strncmp(cmd,"bad",3))  CorrectBadpix(spc);		/** bad = badpix **/
  else if( !strncmp(cmd,"sub",3))  SubtractFrame(spc);		/** sub = subtract = subsky **/
  else if( !strncmp(cmd,"fla",3))  FlatfieldCorr(spc);		/** fla = flatfield **/
  else if( !strncmp(cmd,"bas",3))  SubtractBaseline(spc);	/** bas = baseline  **/
  else if( !strncmp(cmd,"bac",3))  SubtractBackground(spc);	/** bac = background */
  else if( !strncmp(cmd,"ext",3))  ExtractSubframe(spc);	/** ext = extract   **/
  else if( !strncmp(cmd,"FFT",3))  FourierTrafo(spc);		/** FFT = Fouriertr.**/
  else if( !strncmp(cmd,"hea",3))  Header(spc);			/** hea = header    **/
  else if( !strncmp(cmd,"saa",3))  ShiftAndAdd(spc);		/** saa = Shift+Add **/
  else if( !strncmp(cmd,"wm",2) || !strncmp(cmd,"writemean",9))
				WriteMeanFits(spc);		/** wm = writemean **/
  else if( cmd[0]=='r')	ReadFitsCube(spc);		/** r = read  **/
  else if( cmd[0]=='w')	WriteFitsCube(spc);		/** w = write **/
  else if( cmd[0]=='v')
  {	if(spc)	 Verbose= atol(spc);
	else printf(" Verbose level is %d\n",Verbose);
  }
  else if( cmd[0]=='b')
  { if( spc)
    {	int bitp= atol(spc);

	if( bitp!=-64 && bitp!=-32 && bitp!=32 && bitp!=16 && bitp!=8)
		fprintf(stderr," Bad bitpix value %d!\a\n",bitp);
	else
	{	DefBitpix= bitp;	if( Fits)  Fits->bitpix= bitp; }
    }
    else printf(" Current default bitpix value is %d\n",DefBitpix);
  }
  else if( !strncmp(cmd,"-h" ,2) || cmd[0]=='?' || cmd[0]=='h')		/** ? = h = help **/
  {	printf(	" Available commands are:\n"
		"	exit   quit   bye\n"
		"	verbose n\n"
		"	bitpix n\n"
		"	read <filename> [xmin-xmax:ymin-ymax...]\t(first pixel is 0)\n"
		"	write <filename>\n"
		"	wm=writemean <filename>\n"
		"	saa [opts]\n"
		"	header [keyword [(=|+) value]]\n"
		"	rmframes\n"	/** not yet: [sigma=xx]\n **/
		"	sumframes [n]\n"
		"	badpixmask [opts] maskname\n"
		"	subtract [skyfile]\n"
		"	flatfield flatfile\n"
		"	badpix [opts]\n"
		"	baseline [xmin ymin xmax ymax]\n"
		"	background [xmin ymin xmax ymax]\n"
		"	extract xmin ymin xmax ymax\n"
		"	FFT opts\n"
		"	integrate\n"
		"	invert\n"	);
  }
  else if( !strncmp(cmd,"-v" ,2))  Verbose++;
  else if( !strncmp(cmd,"-ni",3))  Interact=0;
  else
	fprintf(stderr,"prespeckle: unknown command %s\a\n",cmd);
}

int main( int argc, char **argv)
{
	char line[256];
	long i;

  printf("prespeckle, compiled on " __DATE__
		 " ============================================\n\n");

  if( argc>1)
	for( i=1;  i<argc;  ++i)  do_command(argv[i]);

  for(;;)	/** forever **/
  {	if( isatty(fileno(stdin)))
	{ if( Fits)
		printf(	"\n%s (%s), %d x %d x %d pixels",
			Fits->filename, Fits->object, Fits->n1, Fits->n2, Fits->n3 );
	  else	printf(	"\nNo data read yet.");
	  printf("\nprespeckle> ");
	}
	if( !fgets(line,255,stdin))  break;	/** nobody there, maybe EOF **/

	do_command(line);
  }
  return 0;
}
