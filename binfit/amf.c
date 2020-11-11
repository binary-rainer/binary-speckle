/* amf - amoeba fit
 *
 * based on bff.c
 *
 * Last Change: Wed Jul 10 14:09:27 2002
 *
 * $Id$
 * $Log$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_VALUES_H
# include <values.h>
#else
# include <float.h>
#endif
#include <unistd.h>
#include "fitsio.h"
#include "fit.h"

/** define this if you want to use the Numerical-Recipes amoeba **/
#undef NUMREC
//#define NUMREC 1

#ifdef NUMREC
/** aus "nrutil.h" **/

float  *vector(long nl, long nh);
int   *ivector(long nl, long nh);
float **matrix(long nrl, long nrh, long ncl, long nch);

void free_vector(float *v, long nl, long nh);
void free_ivector(int *v, long nl, long nh);
void free_matrix(float **m, long nrl, long nrh, long ncl, long nch);

/*
 * I don't know what's happening exactly, but a K&R function
 * declaration is different from an ANSI function declaration, even if
 * all the parameters are the same.  Thus, we can avoid a warning at
 * compile time by adding a prototype for amoeba(), but we get a
 * segmentation fault at runtime.  Take your pick.
 *
 * void amoeba(float **p, float y[], int ndim, float ftol, float (*funk)(float *), int *nfunk);
 */

#else
#include "gnamoeba.h"
#endif

/*#define DEBUG 1*/

/** this is in values.h, but some systems don't have that... **/
#ifndef MAXDOUBLE
#define	MAXDOUBLE	DBL_MAX
#endif

#define VERBOSE_LEVEL	6
#define MAX_COMP	2	/** Max. # of companions **/
#define FTOL		0.00001

extern char *EVisFName, *EPhaFName, *CSVisFNm, *CSPhaFNm,
	    *fitten_out, *bff_out, *CSDVisFNm, *CSDPhaFNm;


short verbose_level= VERBOSE_LEVEL;

double BestFit[MAX_PARAM], BestVisF;

struct Range Ranges[MAX_PARAM]=
{	{ 0.0, 359.9, 9, 0.0, 1.0   },		/** CE_ANGLE	**/
	{ 0.0,   1.0, 9, 0.0, 0.01  },		/** CE_DIST	**/
	{ 0.0, 359.9, 9, 0.0, 0.2   },		/** ANGLE1	**/
	{ 0.0,  63.9, 9, 0.0, 0.02  },		/** DIST1	**/
	{ 0.0,   1.0, 9, 0.0, 0.002 },		/** BRGTN1	**/
};

char	*VisFName=NULL, *DVisFName=NULL, *PhaFName=NULL, *DPhaFName=NULL,
	*MaskFName=NULL, *Title=NULL;
struct FitsFile *VisFits=NULL, *DVisFits=NULL, *PhaFits=NULL, *DPhaFits=NULL,
		*MaskFits=NULL;

double	*VisBuf=NULL, *DVisBuf=NULL, *PhaBuf=NULL, *DPhaBuf=NULL,
	*MVisBuf=NULL,*MPhaBuf=NULL, *EVisBuf=NULL,*EPhaBuf=NULL;
short	*MaskBuf=NULL;

double VisFak, DVisScale=1.0, DPhaScale=1.0, amoeba_ftol= FTOL;

long rad_min=0, radius=0, naxis1, naxis2;

double besterrvis=MAXDOUBLE, besterrpha=MAXDOUBLE, besterrtot=MAXDOUBLE;


/******************************************************************************/

static double atan2deg(double y, double x)
{
	double w;

  if((w= DEG(atan2(y,x))) < 0.0)  w += 360.0;
  return w;
}

/******************************************************************************/

static void SubtractCompans(void)	/** NOTE: Changes contents of DVisBuf! **/
{
	double vis,pha, real,imag;
	long i;

  FT_Modell( MVisBuf,MPhaBuf,naxis1,naxis2,naxis1+naxis2);	/** radius > frame **/

  for( i=0;  i<naxis1*naxis2;  ++i)
  {
	vis= VisBuf[i]*VisFak - MVisBuf[i];
	pha= PhaBuf[i]	      -	MPhaBuf[i];

	real= vis * cos(pha) + 1.0;	/** die 1.0 ist der neue Primary **/
	imag= vis * sin(pha);

	MVisBuf[i]= sqrt(real*real + imag*imag);
	MPhaBuf[i]= atan2(imag,real);
  }
  /** now re-normalize visibility and delta vis**/

  real= MVisBuf[ naxis1 * (1+naxis2) / 2];	/** middle of frame = freq. 0,0 **/

  for( i=0;  i<naxis1*naxis2;  ++i)
  {	MVisBuf[i] /= real;
	if( DVisFits)	DVisBuf[i] *= VisFak/real;
  }
}

/******************************************************************************/

void PrintBestFit(void)
{	long c;

  printf("\n %7.3f"	  " %6.3f "	   " %7.3f"	   "%7.3f"	  " %6.4f ",
	BestFit[CE_ANGLE],BestFit[CE_DIST],BestFit[ANGLE1],BestFit[DIST1],BestFit[BRGTN1] );

  for( c=3;  c<3*N_comp;  c+=3)
  {	printf("\n                 %7.3f%7.3f %6.4f ",
			BestFit[ANGLE1+c], BestFit[DIST1+c], BestFit[BRGTN1+c] );
  }
  printf(" %.3f %.3f %.3f\n", besterrvis, besterrpha, besterrtot );
}

void PrintRanges(void)
{
	long i;
	char *ce_names[2]= { "Center-X", "Center-Y" },
	     *co_names[3]= { "Companion-X", "Companion-Y", "Comp-Brightness" };

  printf("\n Visibility: %s", VisFName?  VisFName:"(none)");
  printf("\n delta vis.: %s",DVisFName? DVisFName:"(none)");
  if( DVisScale!=1.0)	printf( " * %f", DVisScale);
  printf("\n      Phase: %s", PhaFName?  PhaFName:"(none)");
  printf("\n delta pha.: %s",DPhaFName? DPhaFName:"(none)");
  if( DPhaScale!=1.0)	printf( " * %f", DPhaScale);
  printf("\n");

  for( i=0;  i<=CE_DIST;  ++i)
	printf(" %-15s: %10.5f ... %10.5f\n", ce_names[i], Ranges[i].min, Ranges[i].max );
  for( i=CE_DIST+1;  i<N_param;  ++i)
  {	printf(" %-15s: %10.5f ... %10.5f\n",
			co_names[(i-(CE_DIST+1))%3], Ranges[i].min, Ranges[i].max );
  }
  printf("          radius: %ld...%ld\n\n",rad_min,radius);
}


/*****************************************************************************/

short n_amparam;
int *paratypes;
float **simplex;
float *steps, *errvals;


float amoeba_funk(float *simpnt)
{
	double errvis,errpha;
	short i;

#ifdef NUMREC
  for( i=1;  i<=n_amparam;  ++i)  Modell[paratypes[i]] = simpnt[i];
#else
  for( i=0;  i<n_amparam;  ++i)  Modell[paratypes[i]] = simpnt[i];
#endif

  FT_Modell( MVisBuf,MPhaBuf,naxis1,naxis2,radius);
  if( VisFits)
	VisFak= FindVisFak(VisBuf,DVisBuf,MVisBuf,MaskBuf,naxis1,naxis2,rad_min,radius);
  CalcErrors( &errvis, &errpha);

  if( verbose_level >= 4)
  {
    //printf(	" %.3f,"	" %.3f,"	" %.3f,"      " %.3f,"	    " %.3f,",
	printf(	" %.5f,"	" %.5f,"	" %.5f,"      " %.5f,"	    " %.5f,",
		Modell[CE_POSX],Modell[CE_POSY],Modell[POSX1],Modell[POSY1],Modell[BRGTN1] );
	//printf(" %.3f, Errs: %.2f, %.2f\n",VisFak,errvis,errpha);
	printf(" %.3f, Errs: %.5f, %.2f\n",VisFak,errvis,errpha);
	fflush(stdout);
  }
  if( errvis + errpha < besterrtot)
  {	double x, y;

    x= Modell[CE_POSX];	 y= Modell[CE_POSY];
    BestFit[CE_DIST] = sqrt(x*x + y*y);
    BestFit[CE_ANGLE]= atan2deg(y,x);

    for( i=0;  i<3*N_comp;  i+=3)
    {
	x= Modell[POSX1+i];	y= Modell[POSY1+i];

	BestFit[DIST1+i] = sqrt(x*x + y*y);
	BestFit[ANGLE1+i]= atan2deg(y,x);
	BestFit[BRGTN1+i]= Modell[BRGTN1+i];
    }
    BestVisF= VisFak;
    besterrvis= errvis;	 besterrpha= errpha;	besterrtot= errvis + errpha;
    if( verbose_level >= 3)
    {	printf("\r New best fit: ");	PrintBestFit();	}
  }
  return errvis + errpha;
}


void Run_amoeba(void)
{
	int para, pnt, c;
	double Dr, Dt;	/** step radial/tangential **/

  n_amparam= N_param;
  if( !PhaFits)	n_amparam -= 2;

#ifdef NUMREC
  simplex  =  matrix( 1, n_amparam+1, 1, n_amparam);
  errvals  =  vector( 1, n_amparam+1);
  steps	   =  vector( 1, n_amparam );
  paratypes= ivector( 1, n_amparam );

  para= 1;
  for( c=0;  c<N_comp;  ++c)
  {	int off= c*3;

    Dr= Ranges[DIST1+off].max-Ranges[DIST1+off].min;
    Dt= RAD(Ranges[ANGLE1+off].max-Ranges[ANGLE1+off].min) * Ranges[DIST1+off].min;

    paratypes[para] = POSX1+off;
    simplex[1][para]= Ranges[DIST1+off].min * cos(RAD(Ranges[ANGLE1+off].min));
    steps[para]= steps[para+1]= sqrt((Dr*Dr + Dt*Dt) / 2.0);
    ++para;

    paratypes[para] = POSY1+off;
    simplex[1][para]= Ranges[DIST1+off].min * sin(RAD(Ranges[ANGLE1+off].min));
    ++para;

    paratypes[para] = BRGTN1+off;
    simplex[1][para]= Ranges[BRGTN1+off].min;
    steps[para]= Ranges[BRGTN1+off].max-Ranges[BRGTN1+off].min;
    ++para;
  }
  if( PhaFits)
  { Dr= Ranges[CE_DIST].max-Ranges[CE_DIST].min;
    Dt= RAD(Ranges[CE_ANGLE].max-Ranges[CE_ANGLE].min) * Ranges[CE_DIST].min;

    paratypes[para] = CE_POSX;
    simplex[1][para]= Ranges[CE_DIST].min * cos(RAD(Ranges[CE_ANGLE].min)); /** cx **/
    steps[para]= steps[para+1]= sqrt((Dr*Dr + Dt*Dt) / 2.0);
    ++para;

    paratypes[para] = CE_POSY;
    simplex[1][para]= Ranges[CE_DIST].min * sin(RAD(Ranges[CE_ANGLE].min)); /** cy **/
  }
  errvals[1]= amoeba_funk(simplex[1]);

  printf("\nSIMPLEX (%d parameters:\n",n_amparam);
  for( pnt=2;  pnt<=n_amparam+1;  ++pnt)
  { printf(" point %d: ",pnt);
    for( para=1;  para<=n_amparam;  ++para)
    {	simplex[pnt][para]= simplex[1][para];
	/*printf("%g, ",simplex[pnt][para]);*/
    }
    simplex[pnt][pnt-1] += steps[pnt-1];	/** ändere 1.param bei 2.pnt usw.**/
    errvals[pnt]= amoeba_funk(simplex[pnt]);
  }
  /***** Jetzt kommt der schleimige Teil *****/
  {	int nfunk;

    printf(" calling amoeba\n");
    amoeba( simplex,errvals,n_amparam,FTOL,amoeba_funk,&nfunk);
    printf(" amoeba finished\n");
  }
  free_ivector( paratypes,1,n_amparam);
  free_vector( steps,   1, n_amparam );
  free_vector( errvals, 1, n_amparam );
  free_matrix( simplex, 1, n_amparam+1, 1, n_amparam);

#else	/** not NUMREC **/

  if(	!(simplex = gnamoeba_alloc(n_amparam)) ||
	!(errvals = malloc((n_amparam+1)*sizeof(float))) ||
	!(steps   = malloc( n_amparam * sizeof(float))) ||
	!(paratypes= malloc(n_amparam * sizeof(int)))	)
  {
	fprintf(stderr,"Out of memory\n");	exit(42);
  }
  para= 0;
  for( c=0;  c<N_comp;  ++c)
  {	int off= c*3;

    Dr= Ranges[DIST1+off].max-Ranges[DIST1+off].min;
    Dt= RAD(Ranges[ANGLE1+off].max-Ranges[ANGLE1+off].min) * Ranges[DIST1+off].min;

    paratypes[para] = POSX1+off;
    simplex[0][para]= Ranges[DIST1+off].min * cos(RAD(Ranges[ANGLE1+off].min));
    steps[para]= steps[para+1]= sqrt((Dr*Dr + Dt*Dt) / 2.0);
    ++para;

    paratypes[para] = POSY1+off;
    simplex[0][para]= Ranges[DIST1+off].min * sin(RAD(Ranges[ANGLE1+off].min));
    ++para;

    paratypes[para] = BRGTN1+off;
    simplex[0][para]= Ranges[BRGTN1+off].min;
    steps[para]= Ranges[BRGTN1+off].max-Ranges[BRGTN1+off].min;
    ++para;
  }
  if( PhaFits)
  { Dr= Ranges[CE_DIST].max-Ranges[CE_DIST].min;
    Dt= RAD(Ranges[CE_ANGLE].max-Ranges[CE_ANGLE].min) * Ranges[CE_DIST].min;

    paratypes[para] = CE_POSX;
    simplex[0][para]= Ranges[CE_DIST].min * cos(RAD(Ranges[CE_ANGLE].min)); /** cx **/
    steps[para]= steps[para+1]= sqrt((Dr*Dr + Dt*Dt) / 2.0);
    ++para;

    paratypes[para] = CE_POSY;
    simplex[0][para]= Ranges[CE_DIST].min * sin(RAD(Ranges[CE_ANGLE].min)); /** cy **/
  }
  errvals[0]= amoeba_funk(simplex[0]);

  printf("\nSIMPLEX (%d parameters:\n",n_amparam);
  for( pnt=1;  pnt<=n_amparam;  ++pnt)
  { printf(" point %d: ",pnt);
    for( para=0;  para<n_amparam;  ++para)
    {	simplex[pnt][para]= simplex[0][para];
	/*printf("%g, ",simplex[pnt][para]);*/
    }
    simplex[pnt][pnt-1] += steps[pnt-1];	/** ändere 1.param bei 2.pnt usw.**/
    errvals[pnt]= amoeba_funk(simplex[pnt]);
  }
  /***** Jetzt kommt der schleimige Teil *****/
  printf(" calling gnamoeba\n");
  gnamoeba( simplex,errvals,n_amparam,amoeba_ftol,amoeba_funk);
  printf(" gnamoeba finished\n");

  /** amoeba_func checks for best fit, so we don't need the simplex any more **/

  free(paratypes);
  free(steps);
  free(errvals);
  gnamoeba_free(simplex,n_amparam);

#endif	/** NUMREC **/
}


/******************************************************************************/

static short InitDeltaBuf( struct FitsFile *DFits, double *DBuf, double Scale, char *what)
{
	double val;
	long i,x,y, framesz= naxis1*naxis2;

  if( DFits)
  { DFits->flags &= ~(FITS_FLG_EOF|FITS_FLG_ERROR);
    for( i=0;  i<framesz;  ++i)
    {	if( (val= FitsReadDouble(DFits) * Scale) < 0.0)	 val= -val;
	if( val < 0.001)
	{ x= (i%DFits->n1) - DFits->n1/2;
	  y= (i/DFits->n1) - DFits->n2/2;
	  if( x*x + y*y <= radius * radius)
		fprintf(stderr," Delta of %s at (%ld,%ld) = %g, using 0.001\n",
				what, x+DFits->n1/2, y+DFits->n2/2, val	);
	  val= 0.001;
	}
	*DBuf++ = val;
	if( DFits->flags & (FITS_FLG_EOF|FITS_FLG_ERROR))  return i+1;	/** >0 **/
    }
  }
  else
  {	fprintf(stderr," no delta for %s given, using konstant value 0.01\n",what);
	for( i=0;  i<framesz;  ++i)	*DBuf++ = 0.01;
  }
  return 0;	/** all right **/
}


/******************************************************************************/

static short ReadMask(void)
{
	long i, framesz= naxis1*naxis2, num0s=0;
	double v;

  if( verbose_level >= 1)  printf(" Reading %s...\n",MaskFName);
  MaskFits->flags &= ~(FITS_FLG_EOF|FITS_FLG_ERROR);

  for( i=0;  i<framesz;  ++i)
  {
	v= FitsReadDouble( MaskFits);
	if( MaskFits->flags & (FITS_FLG_EOF|FITS_FLG_ERROR))
	{ fprintf(stderr," Error reading %s\a\n",MaskFName);	return 1;  }

	if( (MaskBuf[i]=
		(v>32760.0)? 32765 : ((v<-32760.0)? -32765 : (short)v)) == 0 )
	  num0s++;
  }
  if( num0s < framesz/10)	/** wenigstens 10% sollten gut sein **/
  {	fprintf(stderr,	" Only %ld pixels are not masked out.\n"
			" That's not enough, ignoring mask.\a\n",num0s );
	return 2;
  }
  return 0;	/** all right **/
}

/*****************************************************************************/

static void WriteErrFits( struct FitsFile *SrcFits, char *Name, double *data)
{
	struct FitsFile ErrFits;

  if( Name)
  {	ErrFits.flags= 0;
	ErrFits.filename= Name;	ErrFits.fp= NULL;
	ErrFits.bitpix= -64;	ErrFits.naxis= 2;
	ErrFits.n1= naxis1;	ErrFits.n2= naxis2;	ErrFits.n3= 1;
	strcpy( ErrFits.object, SrcFits->object);
	ErrFits.bscale= 1.0;	ErrFits.bzero= 0.0;
	ErrFits.Header= VisFits->Header;
	ErrFits.HeaderSize= VisFits->HeaderSize;

	FitsWriteAny( Name,&ErrFits,data,-64);
  }
}

/******************************************************************************/

int main( int argc, char **argv)
{
	FILE *bfplotfp= NULL;
	char line[100];
	short end, wait=0, plot=0;
	long arg, framesz;

  fprintf(stderr,"\n amf - amoeba fit, compiled on " __DATE__
		 " ======================\n\n");

  for( arg=1;  arg<argc;  ++arg)
  {	if( argv[arg][0] != '-')  break;

	if( argv[arg][1] == 'w')  wait=1;
   else	if( argv[arg][1] == 'p')  plot=1;
   else	if( argv[arg][1] == 'P')  plot=2;
   else	if( argv[arg][1] == 'v')  verbose_level= atoi( &argv[arg][2]);
   else
	{ if( argv[arg][1] != 'h')
		fprintf(stderr," unknown option: %s\a\n",argv[arg]);
	  fprintf(stderr,
		" Usage: %s [-wait] [-plot] [-v#] [parameter-file]\n"
		" Where:\t-wait	tells it to wait after fitting\n"
		"	-plot	tells it to run bfplot after fitting\n"
		"	-Plot	tells it to run bfplot continously\n"
		"	-v#	sets the verboseness level to #,\n"
		"		#=0: be quiet\n"
		"		  1: startup & close msg, final best fit\n"
		"		  2: + scan ranges\n"
		"		  3: + every new best fit\n"
		"		  4: + every position angle\n"
		"		  5: + every distance\n"
		"		  6: + every brightness ratio\n"
		"		  7: + position of center\n\n",	argv[0]);
	  return 5;
	}
  }
  if( arg>=argc)
  {	printf(" Please enter the name of the Parameter-file:  ");
	fgets(line,99,stdin);
	if( ReadParaFile(line))	 return 10;
  }
  else	if( ReadParaFile(argv[arg]))  return 11;

  if( VisFName)
  { if((VisFits= FitsOpenR( VisFName)))
    {	naxis1= VisFits->n1;	naxis2= VisFits->n2;	}
    else
    {	fprintf(stderr,"\a Do You want me to continue?  ");
	if( (fgets(line,99,stdin)==NULL) || line[0]!='y')  return 12;
    }
  }
  if( PhaFName)
  { if((PhaFits= FitsOpenR( PhaFName)))
    {	naxis1= PhaFits->n1;	naxis2= PhaFits->n2;	}
    else
    {	fprintf(stderr,"\a Do You want me to continue?  ");
	if( (fgets(line,99,stdin)==NULL) || line[0]!='y')  return 12;
    }
  }
  if( !VisFits && !PhaFits)
  {	fprintf(stderr," No data - exiting!\a\n");	return 12;	}

  if( VisFits && PhaFits && ((VisFits->n1 != PhaFits->n1) || (VisFits->n1 != PhaFits->n1)))
  {	fprintf(stderr," Frame sizes of visibility and phase not equal!\a\n");
	FitsClose(VisFits);	FitsClose(PhaFits);	return 13;
  }
  framesz= naxis1 * naxis2;

  if( VisFits && DVisFName && (DVisFits= FitsOpenR(DVisFName)))		/** ohne Vis kein DVis **/
  { if( (VisFits->n1 != DVisFits->n1) || (VisFits->n1 != DVisFits->n1))
    {	fprintf(stderr," Frame sizes of visibility and its delta not equal!\a\n");
	FitsClose(DVisFits);	DVisFits= NULL;
    }
  }
  if( PhaFits && DPhaFName && (DPhaFits= FitsOpenR(DPhaFName)))		/** ohne Pha kein DPha **/
  { if( (PhaFits->n1 != DPhaFits->n1) || (PhaFits->n1 != DPhaFits->n1))
    {	fprintf(stderr," Frame sizes of phase and its delta not equal!\a\n");
	FitsClose(DPhaFits);	DVisFits= NULL;
    }
  }
  if( MaskFName && (MaskFits= FitsOpenR(MaskFName)))
  { if( (MaskFits->n1 != naxis1) || (MaskFits->n2 != naxis2))
    {	fprintf(stderr," Frame size of mask does not match, ignoring it!\a\n");
	FitsClose(MaskFits);	MaskFits= NULL;
    }
  }

  if( !(VisBuf= AllocBuffer( (8*sizeof(double)+sizeof(short)) * framesz,
						"data buffers")))	goto exit;
	DVisBuf=  VisBuf + framesz;
	PhaBuf = DVisBuf + framesz;
	DPhaBuf=  PhaBuf + framesz;
	MVisBuf= DPhaBuf + framesz;
	MPhaBuf= MVisBuf + framesz;	/** letzter pntr, der immer != NULL ist! **/
	if( EVisFName)	EVisBuf= MPhaBuf + framesz;
	if( EPhaFName)	EPhaBuf= MPhaBuf + 2*framesz;
	if( MaskFName)	MaskBuf= (short *)(MPhaBuf + 3*framesz);

  if( VisFits)
  { if( verbose_level >= 1)  printf(" Reading %s...\n",VisFName);
    if( FitsReadDoubleBuf(VisFits,VisBuf,framesz) < framesz)
    {	fprintf(stderr," Error reading %s\a\n",VisFName);	goto exit;	}

    if( InitDeltaBuf( DVisFits,DVisBuf,DVisScale,"visibility"))	goto exit;
  }
  if( PhaFits)
  { if( verbose_level >= 1)  printf(" Reading %s...\n",PhaFName);
    if( FitsReadDoubleBuf(PhaFits,PhaBuf,framesz) < framesz)
    {	fprintf(stderr," Error reading %s\a\n",PhaFName);	goto exit;	}

    if( InitDeltaBuf( DPhaFits,DPhaBuf,DPhaScale,"phase"))	goto exit;
  }
  if( MaskFits)  if( ReadMask())  MaskBuf= NULL;

  if( verbose_level >= 2)  PrintRanges();
  if( verbose_level >= 3)
	 printf(" Cntr-X Cntr-Y Pos. X   Pos. Y  Bright. VisFak\n");
	/**	" 3.123, 0.123, 123.123, 012.123, 0.123, 0.123	" **/

  Run_amoeba();		/** Schwerpunkt wird berechnet **/

  if( verbose_level >= 1)  printf("\n");
  if( fitten_out)
  {	WriteFitten();
	if(plot>1)
	{ if( !bfplotfp)
	  {	sprintf(line,"bfplot %s\n",fitten_out);
		bfplotfp= popen(line,"w");
	  }
	  else
	  {	fprintf(bfplotfp,"read %s\n",fitten_out);  fflush(bfplotfp);	}
	}
  }
  if( bff_out)	WriteParaFile(end);
  if(VisFits && EVisBuf)	WriteErrFits(VisFits,EVisFName,EVisBuf);
  if(PhaFits && EPhaBuf)	WriteErrFits(PhaFits,EPhaFName,EPhaBuf);
  if( CSVisFNm || CSPhaFNm)
  {
    SubtractCompans();
    if(VisFits && CSVisFNm)  WriteErrFits(VisFits,CSVisFNm,MVisBuf);
    if(PhaFits && CSPhaFNm)  WriteErrFits(PhaFits,CSPhaFNm,MPhaBuf);
    if(DVisFits && CSDVisFNm) WriteErrFits(DVisFits,CSDVisFNm,DVisBuf);
    if(DPhaFits && CSDPhaFNm) WriteErrFits(DPhaFits,CSDPhaFNm,DPhaBuf);
  }

  if( verbose_level >= 1)
  {	printf("\n Best Fit:     ");	PrintBestFit();	 printf("\n");	}

exit:
  if( VisBuf)
  {	if( verbose_level >= 1)  printf(" freeing buffers\n");
	free(VisBuf);
  }
#define PrintAndClose(ff)  \
	{ if( verbose_level >= 1)  printf(" closing %s\n",ff->filename);\
	  FitsClose(ff);\
	}
  if( VisFits)	PrintAndClose( VisFits);
  if( PhaFits)	PrintAndClose( PhaFits);
  if(DVisFits)	PrintAndClose(DVisFits);
  if(DPhaFits)	PrintAndClose(DPhaFits);

  if(wait)
  {	printf("\n\a Hit return to continue.\n");	fflush(stdout);
	fgets(line,99,stdin);
  }
  if(bfplotfp)	{ fprintf(bfplotfp,"quit\n");	pclose(bfplotfp); }

  if(plot==1 && fitten_out)
  {	strcpy(line,"bfplot ");	 strcat(line,fitten_out);	system(line);	}
  return 0;
}
