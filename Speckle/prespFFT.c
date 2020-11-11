/* $Id: prespFFT.c 1.1 1995/03/05 17:09:36 koehler Exp koehler $
 *
 * Last Change: Wed Jul 10 18:12:26 2002
 *
 * $Log: fitsFFT.c $
 * Revision 1.1  1995/03/05  17:09:36  koehler
 * Initial revision
 *
 * this file is Hellware!
 */

#include "prespeckle.h"
#ifdef DOUBLE_FFTW
# include "drfftw.h"
#else
# include "rfftw.h"
#endif

/*****************************************************************************/

static void FindCenter(double *buffer, unsigned short sizex, unsigned short sizey,
				short *centx, short *centy)
{	long  framesz;
	unsigned short x,y;
	float sum,sumx,sumy, help;

  *centx= *centy= 0;

  framesz= sizex * sizey;

  sum= sumx= sumy= 0.0;
  for( x=0;  x<sizex;  ++x)
    for( y=0;  y<sizey;  ++y)
    {	help= buffer[x+y*sizex];
	sum  += help;
	sumx += (float)x * help;
	sumy += (float)y * help;
    }
  if(Verbose>1)	printf("sum=%g, sumx=%g, sumy=%g, ",sum,sumx,sumy);

  *centx= (short)(sumx/sum + 0.5);	/** 1.Pixel = (0,0) **/
  *centy= (short)(sumy/sum + 0.5);
}


/*****************************************************************************/

static void CenterPhase(double *PhaBuf, long sizex, long sizey)
{
	float shftx, shfty;
	long xm,ym, x,y;

  xm= sizex/2;	ym= sizey/2;

  shftx= (PhaBuf[ xm+1 + sizex*ym  ] - PhaBuf[ xm-1 + sizex*ym  ]) / 2.0;
  shfty= (PhaBuf[ xm + sizex*(ym+1)] - PhaBuf[ xm + sizex*(ym-1)]) / 2.0;

  if( shftx != 0.0 || shfty != 0.0)
  { if( Verbose>1)  printf(" CenterPhase: shift x = %g, y = %g\n",shftx,shfty);

    for( x=0;  x<sizex;  ++x)
      for( y=0;  y<sizey;  ++y)
	PhaBuf[ x + sizex*y] -= shftx * (float)(x-xm) + shfty * (float)(y-ym);
  }
}


/*****************************************************************************/

static void Help_FFT(void)
{
 printf(" Usage: FFT [-help]\n"
	"  or	FFT [nocenter] [expav=name] [pwr=name] [pwrav=name] [pwrsq=name]\n"
	"				    [pha=name] [phaav=name] [phasq=name]\n"
	"\n where:\tnocenter	suppresses centering the object\n"
	"	expav		filename for average frame of expanded image frames\n"
	"	pwr...		filenames for power of fourier transform (R*R+I*I)\n"
	"	pha...		filenames for phase of fourier transform\n"
	"	pwr/pha		filename for whole data cube\n"
	"	...av		filename for average frame\n"
	"	...sq		filename for sum of squares\n\n");
}


int FourierTrafo( char *opts)
{
	struct FitsFile *OutFits;
	char	*hdrline, *ExpAvName=NULL,
		*PwrName=NULL, *PwrAvName=NULL, *PwrSqName=NULL,
		*PhaName=NULL, *PhaAvName=NULL, *PhaSqName=NULL;

	rfftwnd_plan rfft_plan;
	fftw_real    *ExpBuf= NULL;	/** this should be double, but doesn't have to **/
	fftw_complex *FFTBuf= NULL;

	double	*ExpAvBuf=NULL;
	float 	*PwrBuf = NULL, *PhaBuf = NULL;
	double	*PwrAvBuf=NULL, *PwrSqBuf=NULL;
	double	*PhaAvBuf=NULL,	*PhaSqBuf=NULL;
	short	nocentr=0, centx, centy, nameprtd=0;

	unsigned long sizex,sizey,sizext, framesz, fr, i, froffs;
	unsigned long midx, midy;	/** where freq=0,0 should be in output **/
	unsigned long x,y;

  if( opts==NULL || *opts==0 || !strncmp(opts,"-h",2))	{ Help_FFT(); return 1; }

  if( !Fits) {	printf(" Nothing loaded - nothing to transform!\a\n");	return 2; }

  if( Fits->bitpix > -32)
	printf(" Warning: You should use bitpix -32 or -64 for fouriertransformed data!\a\n");

  sizex= sizey= 1;
  /********************** Parse Opts **********************/
  {	char *argp, *valp;

    if(Verbose)  printf(" FFT-opts: %s\n",opts);

    for( argp= strtok(opts," \t");  argp;  argp= strtok(NULL," \t"))
    {
	if(Verbose)  printf(" FFT-arg: %s\n",argp);
	if((valp= strchr(argp,'=')))  valp++;	/** point to char after '=' **/

	     if( !strncmp(argp,"nocen",5))	nocentr=1;
	else if( !strncmp(argp,"size",4))
	{	sizex= atol(valp);
		if( sizex < 1)	sizex= 1;
		sizey= sizex;
	}
	else if( !strncmp(argp,"exp",3))
	{	ExpAvName= valp? valp : "FFT.fits_expa_mean";
		if(Verbose)  printf(" Name of expanded average frame: %s\n",ExpAvName);
	}
	else if( !strncmp(argp,"pwrav",5))
	{	PwrAvName= valp? valp : "FFT.fits_pwr_mean";
		if(Verbose)  printf(" Name of average power frame: %s\n",PwrAvName);
	}
	else if( !strncmp(argp,"pwrsq",5))
	{	PwrSqName= valp? valp : "FFT.fits_pwr_sum2";
		if(Verbose)  printf(" Name of sum2 power frame: %s\n",PwrSqName);
	}
	else if( !strncmp(argp,"pwr",3))
	{	PwrName= valp? valp : "FFT.fits_pwr";
		if(Verbose)  printf(" Name of power cube: %s\n",PwrName);
	}
	else if( !strncmp(argp,"phaav",5))
	{	PhaAvName= valp? valp : "FFT.fits_pha_mean";
		if(Verbose)  printf(" Name of average phase frame: %s\n",PhaAvName);
	}
	else if( !strncmp(argp,"phasq",5))
	{	PhaSqName= valp? valp : "FFT.fits_pha_sum2";
		if(Verbose)  printf(" Name of sum2 phase frame: %s\n",PhaSqName);
	}
	else if( !strncmp(argp,"pha",3))
	{	PhaName= valp? valp : "FFT.fits_pha";
		if(Verbose)  printf(" Name of phase cube: %s\n",PhaName);
	}
	else
	{	fprintf(stderr," Unknown option: %s\a\n\n",argp);
		Help_FFT();	return 10;
	}
    }
  }

  while( sizex < Fits->n1)  sizex <<= 1;
  while( sizey < Fits->n2)  sizey <<= 1;
  framesz= sizex * sizey;

  if(ExpAvName)	ExpAvBuf = AllocBuffer(framesz*sizeof(double),"expanded average");
  if(PwrAvName)	PwrAvBuf = AllocBuffer(framesz*sizeof(double),"average power");
  if(PwrSqName)	PwrSqBuf = AllocBuffer(framesz*sizeof(double),"sum2 of power");
  if(PhaAvName)	PhaAvBuf = AllocBuffer(framesz*sizeof(double), "average phase");
  if(PhaSqName)	PhaSqBuf = AllocBuffer(framesz*sizeof(double), "sum2 of phase");
  if(PwrName)	PwrBuf   = AllocBuffer(framesz*Fits->n3*sizeof(float),"power");
  if(PhaName)	PhaBuf   = AllocBuffer(framesz*Fits->n3*sizeof(float),"phase");

  if( !(ExpBuf= AllocBuffer(framesz*sizeof(fftw_real),"expansion buffer"))
   || !(FFTBuf= AllocBuffer(framesz*sizeof(fftw_complex),"FFT buffer"))
   || !(OutFits= CopyFits(Fits)))
  {
	free(ExpBuf);	free(ExpAvBuf);	 free(FFTBuf);
	free(PwrBuf);	free(PwrAvBuf);	 free(PwrSqBuf);
	free(PhaBuf);	free(PhaAvBuf);	 free(PhaSqBuf);
	return 21;
  }
  /****************************************************************************
   * Create the magic for FFTW
   * note that FFTW stores arrays in row-major format (the last dimension's
   * index varies most quickly).  This is the opposite of FITS-images...
   * FFT_FORWARD = exponent has sign -1
   * rfftw2d_create_plan() should never fail, but we check anyway.
   ****************************************************************************/

  if( !(rfft_plan= rfftw2d_create_plan(sizey,sizex,
			FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE|FFTW_OUT_OF_PLACE)))
  {
	fprintf(stderr,"Strange: can't create fftw-plan!\a\n");	 return 22;
  }

  /********** Clear Average Buffers **********/

  for( i=0;  i<framesz;  ++i)
  {	if( ExpAvBuf)	ExpAvBuf[i]= 0.0;
	if( PwrAvBuf)	PwrAvBuf[i]= 0.0;
	if( PhaAvBuf)	PhaAvBuf[i]= 0.0;
	if( PwrSqBuf)	PwrSqBuf[i]= 0.0;
	if( PhaSqBuf)	PhaSqBuf[i]= 0.0;
  }

  /********** Ok, now let's do the things we are here for **********/

  if( sizex != Fits->n1 || sizey != Fits->n2)	printf(" Expanding &");
  printf(" Transforming...\n");
  midx  = sizex/2;	midy = sizey/2;
  sizext= sizex/2 + 1;	/** the x-size of the transformed array **/

  for( fr=froffs=0;  fr<Fits->n3;  ++fr)
  {	/** fr	   counts the Source-Frame (in normal space) **/
	/** froffs Pixel-Offset to frame in dest-buffer (in F-Space) **/

	float *srcptr= &Fits->Buffer.f[fr*Fits->n1*Fits->n2];
				/** Ptr to Source, WARNING: framesz != n1*n2 **/

	/***** copy frame and expand, if necessary *****/

	if( sizex != Fits->n1 || sizey != Fits->n2)
	{	unsigned long offx, offy;

	  offx= (sizex-Fits->n1)/2;	offy= (sizey-Fits->n2)/2;
	  for( i=0;  i<framesz;  ++i)	ExpBuf[i]= 0.0;		/** erst mal saubermachen **/

	  for( y=0;  y<Fits->n2;  ++y)
	    for( x=0;  x<Fits->n1;  ++x)
		ExpBuf[ (x+offx) + sizex*(y+offy)]= *srcptr++;
	  expand_frame( ExpBuf, sizex,sizey, Fits->n1,Fits->n2);
	}
	else
	  for( i=0;  i<framesz;  ++i)	ExpBuf[i]= *srcptr++;	/** copy to double **/

	if(ExpAvBuf)  for( i=0; i<framesz; ++i)  ExpAvBuf[i] += ExpBuf[i];


	/***** search center of light and move it to center of frame *****/
	if( !nocentr)
	{ FindCenter(ExpBuf,sizex,sizey,&centx,&centy);
	  if( centx<0 || centx>=sizex || centy<0 || centy>=sizey)
	  {	if( !nameprtd)
		{	fprintf(stderr," File %s:\n",Fits->filename);	nameprtd=1;	}
		fprintf(stderr," frame %ld: center (%d,%d) out of frame - skipping frame!\a\n",
			fr, centx,centy);
		continue;
	  }
	  if(Verbose>1)  printf(" Frame %ld: center at (%d,%d)\n",fr,centx,centy);

	  /** NOTE: Due to a bug, the frames were never centered before the FFT **
	   ** Since nobody noticed, this does not seem to be important...	**/
	}


	/********** do FFT **********/

	rfftwnd_one_real_to_complex(rfft_plan,ExpBuf,FFTBuf);

	/***** Re/Im -> Pwr/Pha, add to sums *****/
	for( x=0;  x<=midx && x<sizext;  ++x)
	  for( y=0;  y<sizey;  ++y)
	  {	double Pwr, Pha, Real, Imag;

		Real= FFTBuf[x + y*sizext].re;
		Imag= FFTBuf[x + y*sizext].im;
		Pwr= Real * Real + Imag * Imag;

		/*** set negative half of array (larger, contains midx==x) **/
		i = (midx-x) + ((midy-y)%sizey) * sizex;
		if(PwrBuf)	PwrBuf[i + froffs]= Pwr;	/** power-cube **/
		if(PwrAvBuf)	PwrAvBuf[i] += Pwr;		/** sum(power) **/
		if(PwrSqBuf)	PwrSqBuf[i] += Pwr*Pwr;		/** sumsq(pwr) **/

		if( PhaBuf || PhaAvBuf || PhaSqBuf)
			ExpBuf[i]= Pha= (Pwr != 0.0) ? -atan2(Imag,Real) : 0.0;

		/*** set positive half of array **/
		if (x>0 && x<midx)
		{
		  i = (midx+x) + ((midy+y)%sizey) * sizex;
		  if(PwrBuf)	PwrBuf[i + froffs]= Pwr;	/** power-cube **/
		  if(PwrAvBuf)	PwrAvBuf[i] += Pwr;		/** sum(power) **/
		  if(PwrSqBuf)	PwrSqBuf[i] += Pwr*Pwr;		/** sumsq(pwr) **/

		  if( PhaBuf || PhaAvBuf || PhaSqBuf)
			ExpBuf[i]= -Pha;
		}
	  }
	if( PhaBuf || PhaAvBuf || PhaSqBuf)
	{ if( !nocentr)	 CenterPhase( ExpBuf,sizex,sizey);

	  for( i=0;  i<framesz;  ++i)
	  {	double Pha= ExpBuf[i];

		if(PhaBuf)   PhaBuf[i + froffs]= Pha;	/** phase-cube **/
		if(PhaAvBuf) PhaAvBuf[i] += Pha;	/** sum(phase) **/
		if(PhaSqBuf) PhaSqBuf[i] += Pha*Pha;	/** sum(phase) **/
	  }
	}
	froffs += framesz;	/** next frame in buffers **/
  }
/*  if( sizex != Fits->n1 || sizey != Fits->n2)*/
	free(ExpBuf);

/********** Now write the fourier transformed data **********/

  OutFits->n1= sizex;	OutFits->n2= sizey;	OutFits->n3= froffs/framesz;

  FitsAddHistKeyw( OutFits,"FFT");
  hdrline= FitsFindEmptyHdrLine( OutFits);
				/** hdrlines muessen ihre Vorgaenger ueberschreiben! */
  if( hdrline)	memcpy( hdrline,"COMMENT = this line is in use                 ",46);

  if( PwrBuf)
  {	if( hdrline)
		memcpy( hdrline,"COMMENT = Power of FFT = Real*Real + Imag*Imag",46);
	FitsWriteAny( PwrName, OutFits, PwrBuf,BITPIX_FLOAT);
  }
  if( PhaBuf)
  {	if( hdrline)
		memcpy(hdrline,	"COMMENT = Phase of FFT                        ",46);
	FitsWriteAny( PhaName, OutFits, PhaBuf,BITPIX_FLOAT);
  }
  if( PwrAvBuf || PhaAvBuf)			/***** the average of the FFT *****/
  {	char  ncoaddline[88];
	short n3save= OutFits->n3;	/** FitsWriteAny() sets n3 to 1, if naxis==2 **/

	for( i=0;  i<framesz;  ++i)
	{	if( PwrAvBuf)	PwrAvBuf[i] /= (double)n3save;
		if( PhaAvBuf)
		{	PhaAvBuf[i] /= (float)n3save;
			if( !nocentr)	CenterPhase( PhaAvBuf,sizex,sizey);
		}
	}
	sprintf(ncoaddline,"NCOADDS = %20d   / %-45s",
				n3save, "number of frames added for this average");
	FitsSetHeaderLine( OutFits,"NCOADDS = ",ncoaddline);
	OutFits->naxis=2;

	if( PwrAvBuf)
	{ if(hdrline)
		memcpy(hdrline,"COMMENT = Average power of FFT = R*R + I*I    ",46);
	  OutFits->bitpix= BITPIX_DOUBLE;
	  FitsWriteAny( PwrAvName, OutFits, PwrAvBuf,BITPIX_DOUBLE);
	  OutFits->bitpix= Fits->bitpix;
	}
	if( PhaAvBuf)
	{ if(hdrline)
		memcpy(hdrline,"COMMENT = Average phase of FFT                ",46);
	  FitsWriteAny( PhaAvName, OutFits, PhaAvBuf,BITPIX_DOUBLE);
	}
	OutFits->n3= n3save;
  }
  if( PwrSqBuf || PhaSqBuf)			/***** the sum of the squares *****/
  {	char ncoaddline[88];

	sprintf(ncoaddline,"NCOADDS = %20d   / %-45s",
				OutFits->n3, "number of frames added in this sum");
	FitsSetHeaderLine( OutFits,"NCOADDS = ",ncoaddline);
	OutFits->naxis=2;

	if( PwrSqBuf)
	{ if(hdrline)
		memcpy(hdrline,"COMMENT = Sum of squares of FFT= (R*R + I*I)^2",46);
	  OutFits->bitpix= BITPIX_DOUBLE;	/** range of this obj^4 is ca. 10^28 **/
	  FitsWriteAny( PwrSqName, OutFits, PwrSqBuf,BITPIX_DOUBLE);
	  OutFits->bitpix= Fits->bitpix;
	}
	if( PhaSqBuf)
	{ if(hdrline)
		memcpy(hdrline,"COMMENT = Sum of squares of phase of FFT      ",46);
	  FitsWriteAny( PhaSqName, OutFits, PhaSqBuf,BITPIX_DOUBLE);
	}
  }
  if(ExpAvBuf)				/***** and the average of the expanded frames *****/
  {	OutFits->naxis=2;
	OutFits->bitpix= 16;	/** that's enough! **/

	if(hdrline)
		memcpy(hdrline,"COMMENT = Sum of expanded frames              ",46);
	FitsWriteAny( ExpAvName, OutFits, ExpAvBuf,BITPIX_DOUBLE);
  }
  FitsClose(OutFits);	/** free Buffers **/
  printf("\n");

  free(FFTBuf);	 free(ExpAvBuf);
  free(PwrBuf);	 free(PwrAvBuf);  free(PwrSqBuf);
  free(PhaBuf);	 free(PhaAvBuf);  free(PhaSqBuf);
  return 0;
}
