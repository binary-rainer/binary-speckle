/* bff - brute force fit
 *
 * Last Change: Wed Jul 10 14:11:27 2002
 *
 * $Id: bff.c,v 1.5 1998/08/06 09:48:21 koehler Exp koehler $
 * $Log: bff.c,v $
 * Revision 1.5  1998/08/06 09:48:21  koehler
 * vor Auslagern von Read/WriteBff
 *
 * Revision 1.4  1996/09/14  17:56:47  koehler
 * Vor Erweiterung auf mehr als 1 Begleiter
 *
 * Revision 1.3  1996/04/24  12:16:22  koehler
 * vor Umstellung auf "Phase-Shift-FT"
 *
 * Revision 1.2  1996/01/14  16:09:44  koehler
 * more or less usable version (without delta-files)
 *
 * Revision 1.1  1995/10/27  10:09:22  koehler
 * Initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_VALUES_H
# include <values.h>
#else
# include <float.h>
# define MAXDOUBLE	DBL_MAX
#endif
#include <unistd.h>
#include "fitsio.h"
#include "fit.h"

/*#define DEBUG 1*/

#define VERBOSE_LEVEL	6
#define MAX_COMP	2	/** Max. # of companions **/

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

double VisFak, DVisScale=1.0, DPhaScale=1.0, amoeba_ftol=0.0;

long rad_min=0, radius=0, naxis1, naxis2;

double besterrvis=MAXDOUBLE, besterrpha=MAXDOUBLE, besterrtot=MAXDOUBLE;

/*****************************************************************************/

static void ShiftCenter( double *phabuf)
{
	long halfx= naxis1/2, halfy= naxis2/2, ix,iy;
	double rad_angle, xc,yc, rad_quad= (double)(radius * radius), x,y;

  rad_angle= RAD(Modell[CE_ANGLE]);	/** xc,yc ist unser shift-vector **/
  xc= Modell[CE_DIST] * cos(rad_angle) * ZWEIPI / (double)naxis1;
  yc= Modell[CE_DIST] * sin(rad_angle) * ZWEIPI / (double)naxis2;

	/***** Im Folgenden beziehen sich x und y auf den Fourierraum! *****/

  if( (iy= halfy-radius) < 0)  iy=0;
  for( ;  iy <= halfy+radius && iy < naxis2;  ++iy)
  {	y= (double)(iy - halfy);

    if( (ix= halfx-radius) < 0)  ix=0;
    for( ;  ix <= halfx+radius && ix < naxis1;  ++ix)
    {	x= (double)(ix - halfx);

	if( x*x + y*y <= rad_quad )
		phabuf[ix + naxis1*iy] -= xc*x + yc*y;
    }
  }
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
	char *ce_names[MAX_PARAM]= { "Center-Angle", "Center-Distance" },
	     *co_names[3]= { "Companion-Angle", "Comp.-Distance", "Comp-Brightness" };

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
  {	printf(" %-15s: %10.5f ... %10.5f, step %f, precision %f\n",
			co_names[(i-(CE_DIST+1))%3],
			Ranges[i].min, Ranges[i].max, Ranges[i].step, Ranges[i].prec);
  }
  printf("     max. radius: %ld\n\n",radius);
}


void ScanParamRange(int param_num)
{
	double errvis,errpha;
	long i;
	int param_next= param_num+1;

  for(	Modell[param_num]= Ranges[param_num].min;
		Modell[param_num] <= Ranges[param_num].max;
			Modell[param_num] += Ranges[param_num].step	)
  {
    if( verbose_level >= param_num - ANGLE1 + 4)
    {	printf("\r %.1f,"	 " %.2f,"	 " %.1f,"	" %.2f,"	" %.3f\t",
		Modell[CE_ANGLE],Modell[CE_DIST],Modell[ANGLE1],Modell[DIST1],Modell[BRGTN1] );
	fflush(stdout);
    }
    if( param_next < N_param)
    {	ScanParamRange(param_next);	}
    else
    {	Modell[CE_DIST]= 0.0;	/** erstmal in die Mitte **/
	FT_Modell( MVisBuf,MPhaBuf,naxis1,naxis2,radius);
#ifdef DEBUG
	fprintf(stderr,"FT ");
#endif
	if( PhaFits)		/** nur Phase haengt vom SP ab **/
	{ FindBestCenter(PhaBuf,DPhaBuf,MPhaBuf,MaskBuf,naxis1,naxis2,radius);
	  if( Modell[CE_ANGLE]< Ranges[CE_ANGLE].min) Modell[CE_ANGLE]= Ranges[CE_ANGLE].min;
	  if( Modell[CE_ANGLE]> Ranges[CE_ANGLE].max) Modell[CE_ANGLE]= Ranges[CE_ANGLE].max;

	  if( Modell[CE_DIST] < Ranges[CE_DIST].min)  Modell[CE_DIST]= Ranges[CE_DIST].min;
	  if( Modell[CE_DIST] > Ranges[CE_DIST].max)  Modell[CE_DIST]= Ranges[CE_DIST].max;

	  if( verbose_level >= 7)
		printf("SP: (%.3g %.3g), ",Modell[CE_ANGLE],Modell[CE_DIST]);

	  if( Modell[CE_DIST] != 0.0)	ShiftCenter(MPhaBuf);
	}
#ifdef DEBUG
	fprintf(stderr,"Ce ");
#endif
	if( VisFits)
		VisFak= FindVisFak(VisBuf,DVisBuf,MVisBuf,MaskBuf,naxis1,naxis2,rad_min,radius);

	CalcErrors( &errvis, &errpha);

	if( errvis + errpha < besterrtot)
	{ for( i=0;  i<MAX_PARAM;  ++i)	 BestFit[i]= Modell[i];
	  BestVisF= VisFak;
	  besterrvis= errvis;	besterrpha= errpha;	besterrtot= errvis + errpha;
	  if( verbose_level >= 3)
	  {	printf("\r New best fit: ");	PrintBestFit();	}
	}
    }	/** if( param_next < MAX_PARAM )...else **/
  }	/** for( param= min...max)	**/
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
	ErrFits.bitpix= -32;	ErrFits.naxis= 2;
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
	long arg, i, framesz;

  fprintf(stderr,"\n bff - brute force fit, compiled on " __DATE__
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
	if( (fgets(line,99,stdin)==NULL) || line[0]!='y')	return 12;
    }
  }
  if( PhaFName)
  { if((PhaFits= FitsOpenR( PhaFName)))
    {	naxis1= PhaFits->n1;	naxis2= PhaFits->n2;	}
    else
    {	fprintf(stderr,"\a Do You want me to continue?  ");
	if( (fgets(line,99,stdin)==NULL) || line[0]!='y')	return 12;
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

  do{	for( i=0;  i<N_param;  ++i)
	{	if( Ranges[i].max < Ranges[i].min)
		{	double swap= Ranges[i].max;
			Ranges[i].max= Ranges[i].min;
			Ranges[i].min= swap;
		}
		if((Ranges[i].step= (Ranges[i].max - Ranges[i].min) /
					(double)(Ranges[i].npnts-1)) <= 0.0)
			Ranges[i].step= Ranges[i].prec;
	}
	if( verbose_level >= 2)  PrintRanges();
	if( verbose_level >= 3)
	 printf("               Ce-Angle Ce-Dist   Angle    Dist    Brightn.  Err-V Err-P Err-Tot\n");
	/**	" New best fit: 123.1234 0.123456  123.1234 12.1234 0.123456  1.123 1.123 1.123"	**/

	ScanParamRange(ANGLE1);		/** Schwerpunkt wird berechnet **/
	end=1;
	for( i= ANGLE1;  i<MAX_PARAM;  ++i)
	{				/** wenn BestFit am Rand, Intervall vergroessern **/
	  if( (Ranges[i].min == BestFit[i]) || (Ranges[i].max == BestFit[i]))
		Ranges[i].step= Ranges[i].max - Ranges[i].min;

	  if( Ranges[i].min != Ranges[i].max)	/** wenn's gleich ist, sollen wir nicht scannen **/
	  {	Ranges[i].min= BestFit[i] - Ranges[i].step;
		Ranges[i].max= BestFit[i] + Ranges[i].step;
		if( Ranges[i].step > Ranges[i].prec)	end=0;
	  }
	}
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
    }while( !end);

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
