/* $Id: quickvis.c,v 1.1 1996/02/01 20:16:13 koehler Exp koehler $
 * $Log: quickvis.c,v $
 * Revision 1.1  1996/02/01  20:16:13  koehler
 * Initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
//#include <values.h>
#include <unistd.h>
#include "fitsio.h"

/*#define DEBUG 1*/

struct FitsFile OutFits;
double *objsum=NULL, *oskysum=NULL, *refsum=NULL, *rskysum=NULL;
double *objsm2=NULL, *oskysm2=NULL, *refsm2=NULL, *rskysm2=NULL;
double	objcnt= 0.0,  oskycnt= 0.0,  refcnt= 0.0,  rskycnt= 0.0;
double	objcn2= 0.0,  oskycn2= 0.0,  refcn2= 0.0,  rskycn2= 0.0;

char WindMill[]= "|/-\\";

/******************************************************************************/

static short CheckFitsMatch( struct FitsFile *ff)
{
	long framesz,i;

  if( OutFits.naxis==0)		/** noch gar nix gelesen **/
  {
    framesz= ff->n1 * ff->n2;
    if( !(objsum= AllocBuffer(8*framesz*sizeof(double),"data buffers")))  return 1;

    OutFits= *ff;	/** copy struct FitsFile **/
    OutFits.fp= NULL;
    OutFits.naxis= 2;	OutFits.n3= 1;
    if( !(OutFits.Header= AllocBuffer(ff->HeaderSize,"FITS Header")))
    {	free(objsum);	objsum=NULL;	OutFits.naxis= 0;	return 2;	}
    memcpy( OutFits.Header,ff->Header,ff->HeaderSize);	/** copy Header Data **/

    oskysum= objsum  + framesz;
    refsum = oskysum + framesz;
    rskysum= refsum  + framesz;
    objsm2 = rskysum + framesz;
    oskysm2= objsm2  + framesz;
    refsm2 = oskysm2 + framesz;
    rskysm2= refsm2  + framesz;

    for( i=0;  i < 8*framesz;  ++i)	objsum[i]= 0.0;
  }
  else
  {
    if( OutFits.n1 != ff->n1 || OutFits.n2 != ff->n2)
    {	fprintf(stderr," %s: Frame size does not match!\a\n",ff->filename);
	return 3;
    }
  }
  return 0;	/** all right **/
}

/******************************************************************************/

static void ReadOneFile( char *fname, double **smbp, double *cnt, short sum)
	/** Note: CheckFitsMatch() may change objsum, oskysum etc.,	 **/
	/**	  so we have to pass a pointer to it instead of its value */
{
	struct FitsFile *ff;
	double *sumbuf, naveraged= 1.0, nadded= 1.0;
	char *keyw;
	long fr,framesz, i;

  if( !(ff= FitsOpenR(fname)) || CheckFitsMatch(ff))  { FitsClose(ff);	return; }

  printf(" reading %s, %d x %d x %d pixels", fname, ff->n1,ff->n2,ff->n3 );
  if( (keyw= FitsFindKeyw(ff,"NCOADDS = ")))
  { if( sum==0)		/** it's an average frame **/
    {	naveraged= atof(keyw);	nadded= 1.0;
 	printf(" (%g frames aver.)\n",naveraged );
    }
    else
    {	naveraged= 1.0;		nadded= atof(keyw);
	printf(" (%g frames added)\n",nadded );
    }
  }
  else	printf("\n");

  framesz= ff->n1 * ff->n2;
  sumbuf = *smbp;	/** NOW is it initialized! **/
  ff->flags &= ~(FITS_FLG_EOF|FITS_FLG_ERROR);
  for( fr=0;  fr<ff->n3;  fr++)
  { for( i=0;  i<framesz;  ++i)  sumbuf[i] += naveraged * FitsReadDouble(ff);

    if( ff->flags & (FITS_FLG_EOF|FITS_FLG_ERROR))
    {	fprintf(stderr," not enough data in %s, result may be wrong!\a\n",ff->filename);
	break;
    }
    if(fr>0)	printf(" %c\r", WindMill[fr&3] );
  }
  *cnt += naveraged * nadded * (double) fr;	/** as many frames as we actually read **/
  FitsClose(ff);
}

static int ReadFiles( char **argv, int argc, int arg,
			double **sumbp, double *cnt, double **sm2bp, double *cn2 )
	/** Note: CheckFitsMatch() may change objsum, oskysum etc.,	 **/
	/**	  so we have to pass a pointer to it instead of its value */
{
	char fname[260];

  for( ;  arg<argc && argv[arg][0] != '-';  ++arg)
  {
	strcpy( fname,argv[arg]);	strcat( fname,"_mean");
	ReadOneFile(fname,sumbp,cnt,0);

	strcpy( fname,argv[arg]);	strcat( fname,"_sum2");
	ReadOneFile(fname,sm2bp,cn2,1);

	/*	fprintf(stderr," sum @ middle: %g, sum2 @ middle: %g\n",
		(*sumbp)[OutFits.n1/2 + OutFits.n1 * OutFits.n2/2],
		(*sm2bp)[OutFits.n1/2 + OutFits.n1 * OutFits.n2/2]	);
	*/
  }
  return --arg;		/** main loop will increase it again **/
}


/******************************************************************************/

double CalcDeltaSq( double mean, double sum2, double cnt)
	/** mean = (sum_1^cnt (x)) / cnt, sum2 = sum_1^cnt (x*x) **/
{
  if( cnt > 1.0)
	return (sum2 - mean * mean * cnt) / (cnt * (cnt-1.0));
  else	return mean;
}


/******************************************************************************/

static void Usage(char *prg)
{
 printf(" USAGE: %s [-obj Object-file[s]] [-osky Object-Sky-file[s]]\n"
	"\t[-ref Reference-file[s]] [-rsky Reference-Sky-file[s]] [-o output] [-s sigma_out]\n"
	,prg);
}

static void Help(char *prg)
{
 Usage(prg);
 printf(" Where:\t-obj files	the datafiles containing the power of the object\n"
	"\t-osky files	the datafiles containing the power of the object-sky\n"
	"\t-ref files	the datafiles containing the power of the reference\n"
	"\t-rsky files	the datafiles containing the power of - Guess what!\n"
	"\t-o file	Output-Fitsfile for mean (will be created)\n"
	"\t-s file	Output-Fitsfile for sigma (will be created)\n\n" );
}


int main( int argc, char **argv)
{
	char *OutName="visibility.fits", *SigmName="delta_vis.fits", *args;
	double norm;
	short objflg, oskyflg, refflg, rskyflg;
	long i;
	int arg;

  printf("\nquickvis, compiled on " __DATE__ " ==============================\n\n");

  if( argc>1 && argv[1][0]=='-' && argv[1][1]=='h')	{ Help(argv[0]);  return 0;  }

  OutFits.naxis= 0;	/** to mark it as not initialized **/

  for( arg=1;  arg<argc;  ++arg)
  {	args= argv[arg];
#ifdef DEBUG
	fprintf(stderr,"arg[%d] = %s\n",arg,args);
#endif
   if( !strncmp( args,"-ob",3))
   {	printf(" Object files...\n");
	arg= ReadFiles(argv,argc,++arg,&objsum ,&objcnt ,&objsm2, &objcn2 );
   }
   else	if( !strncmp( args,"-os",3))
   {	printf(" Object sky files...\n");
	arg= ReadFiles(argv,argc,++arg,&oskysum,&oskycnt,&oskysm2,&oskycn2);
   }
   else	if( !strncmp( args,"-re",3))
   {	printf(" Reference files...\n");
	arg= ReadFiles(argv,argc,++arg,&refsum ,&refcnt, &refsm2, &refcn2 );
   }
   else	if( !strncmp( args,"-rs",3))
   {	printf(" Reference sky files...\n");
	arg= ReadFiles(argv,argc,++arg,&rskysum,&rskycnt,&rskysm2,&rskycn2);
   }
   else if( !strncmp( args,"-o",2) && ++arg < argc)   OutName= argv[arg];
   else if( !strncmp( args,"-s",2) && ++arg < argc)   SigmName= argv[arg];
   else {  fprintf(stderr," unknown option: '%s'\n",args);	return 20;	}
  }

  objflg = (objcnt  != 0.0);
  oskyflg= (oskycnt != 0.0);
  refflg = (refcnt  != 0.0);
  rskyflg= (rskycnt != 0.0);

  printf(" object    : %g frames\n",objcnt);
  if( objcnt!=objcn2)
	fprintf(stderr," Warning: %g frames added for sum2, delta may be incorrect!\a\n",objcn2);
  printf(" object-sky: %g frames\n",oskycnt);
  if( oskycnt!=oskycn2)
	fprintf(stderr," Warning: %g frames added for sum2, delta may be incorrect!\a\n",oskycn2);
  printf(" reference : %g frames\n",refcnt );
  if( refcnt!=refcn2)
	fprintf(stderr," Warning: %g frames added for sum2, delta may be incorrect!\a\n",refcn2);
  printf(" refer.-sky: %g frames\n",rskycnt);
  if( rskycnt!=rskycn2)
	fprintf(stderr," Warning: %g frames added for sum2, delta may be incorrect!\a\n",rskycn2);

  for( i=0;  i < OutFits.n1 * OutFits.n2;  ++i)
  {	if( objflg)
	{ objsum[i] /= objcnt;					/** objsum = mean **/
	  objsm2[i]= CalcDeltaSq(objsum[i],objsm2[i],objcnt);	/** objsm2 = sigma **/
	}
	else
	{ objsum[i]= 1.0;  objsm2[i]= 0.0; }

	if( oskyflg)
	{ oskysum[i] /= oskycnt;
	  oskysm2[i]= CalcDeltaSq(oskysum[i],oskysm2[i],oskycnt);
	  objsum[i] -= oskysum[i];
	}
	else	oskysm2[i]= 0.0;

	if( refflg)
	{ refsum[i] /= refcnt;
	  refsm2[i]= CalcDeltaSq(refsum[i],refsm2[i],refcnt);
	}
	else
	{ refsum[i]= 1.0;  refsm2[i]= 0.0; }

	if( rskyflg)
	{ rskysum[i] /= rskycnt;
	  rskysm2[i]= CalcDeltaSq(rskysum[i],rskysm2[i],rskycnt);
	  refsum[i] -= rskysum[i];
	}
	else	rskysm2[i]= 0.0;

	objsum[i] /= refsum[i];
  }
  i= OutFits.n1/2 + (OutFits.n2/2 * OutFits.n1);
  norm= objsum[i];
  printf(" visibility^2 @ center: %g\n", norm);
  if( norm<=0.0)
  {	norm= -norm;
	fprintf(stderr," visib^2 @ center TOO strange, I'll use %g!\a\n",norm);
  }
  norm= sqrt(norm);

  /** objsum==visibility^2, refsum==<r^2> - <rs^2>, xxxsm2==sigma^2 von xxx **/

  for( i=0;  i < OutFits.n1 * OutFits.n2;  ++i)
  { if( objsum[i] > 0.0)
    {	objsm2[i]= sqrt( (objsm2[i]+oskysm2[i])/objsum[i] + (refsm2[i]+rskysm2[i])*objsum[i] )
								/ ( 2.0 * refsum[i] * norm) ;
	objsum[i]= sqrt( objsum[i] ) / norm;
    }
    else
    {	objsum[i]= 0.0;	 objsm2[i]= 1.0;  }
  }
  FitsAddHistKeyw( &OutFits,"quickvis");
  FitsWrite( OutName, &OutFits, objsum);
  FitsWrite(SigmName, &OutFits, objsm2);
  return 0;
}
