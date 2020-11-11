/* $Id: badpixmask.c,v 1.4 1997/07/26 15:06:38 koehler Exp koehler $
 * $Log: badpixmask.c,v $
 * Revision 1.4  1997/07/26  15:06:38  koehler
 * changed into subroutine of prespeckle
 *
 * Revision 1.3  1995/08/19  20:13:10  koehler
 * vor Umstellung auf Behandlung von cubes
 *
 * Revision 1.2  1995/03/05  17:09:36  koehler
 * Badpixels erst in sep. Buffer, falls sigma erhoeht werden muss
 *
 * Revision 1.1  1995/01/17  11:04:47  koehler
 * Initial revision
 */

#include "prespeckle.h"

/****************************** Default values *******************************/

static double	minnsigm= 3.0;
static long	minboxr	= 3, maxboxr= 12;
static short	maxsteps= 5;
static long	maxfrac = 50;


/****************************** Find Bad Pixels ******************************/

static void MakeBadpixMask( struct FitsFile *InFits, float *InBuf, short *MaskBuf)
{
	double help,sum,sum2, mean,sigm, nsigm=minnsigm;
	short *AddMask;
	short step, dcnt, hcnt, dtot, htot;
	long  x,y,offs,xx,yy, n, framesz= InFits->n1*InFits->n2, boxr;

  dtot= htot= 0;	/** bisher keine badpixels **/
  if( !(AddMask= AllocBuffer(framesz*sizeof(short),"temp. mask")))	return;

  for( step=0;  step<maxsteps;  step++)
  { printf(" Step %d\r",step);	fflush(stdout);
    do
    { if( (dtot+htot) > framesz/maxfrac)  break;	/** already enough **/
      dcnt= hcnt= 0;
      for( x=0;  x<InFits->n1;  ++x)
      {	if( Verbose==3)	{ printf("x=%ld\r",x);	fflush(stderr);	}
	for( y=0;  y<InFits->n2;  ++y)
	{ offs= x + y*InFits->n1;
	  AddMask[offs]= 0;
	  if( !MaskBuf[offs] && !AddMask[offs])	/** (noch) nicht als bad markiert **/
	  {	sum= sigm= 0.0;	n=0;
		for( boxr= minboxr;  boxr<maxboxr;  ++boxr)
		{ sum= sum2= 0.0;	n=0;
		  for( xx= x-boxr;  xx<=x+boxr && xx<InFits->n1;  ++xx)
		  { if( xx<0)  xx=0;
		    for( yy= y-boxr;  yy<=y+boxr && yy<InFits->n2;  ++yy)
		    { if( yy<0)  yy=0;
		      if( !(x==xx && y==yy) && !MaskBuf[xx + yy*InFits->n1])
		      {	help= InBuf[xx + yy*InFits->n1];
			++n;	sum += help;	sum2 += help*help;
		      }
		    }
		  }	/** for( xx... **/
		  if( n>1)
		  {	sigm= sum2 - sum*sum/(double)n;
			if( sigm > 0.0)  break;
		  }
		}	/** for( boxr... **/
		if( Verbose>3)
		{ printf("x=%ld, y=%3ld, boxr=%ld, n=%ld, s^2=%g\r",
					x,y,boxr,n,sigm);
		  fflush(stderr);
		}
		help= (double)n;
		mean= sum/help;
		sigm= sqrt(sigm / (help-1.0));
		if( n<2)
		{ printf(" too many badpixels around (%ld,%ld), radius %ld\n",x,y,boxr);
		  AddMask[offs]= 150-step;
		  continue;	/** naechstes pixel **/
		}
		if( sigm==0.0)  sigm= mean/10.0;

		help= InBuf[offs];	sigm *= nsigm;
		if( help < mean - sigm)
		{ AddMask[offs]= 100-step;
		  if( Verbose>2)
			printf(" Dark: (%3ld,%3ld), %g not in %g+-%g\n",x,y,help,mean,sigm);
		  ++dcnt;
		}
		else if( help > mean + sigm)
		{ AddMask[offs]= 200-step;
		  if( Verbose>2)
			printf("  Hot: (%3ld,%3ld), %g not in %g+-%g\n",x,y,help,mean,sigm);
		  ++hcnt;
		}
	  }	/** if( !MaskBuf...)**/
	}	/** for( y...) **/
      }		/** for( x...) **/
      if( Verbose)
	printf(" Step %d: %.2f sigma, detected %d more dark and %d more hot pixels\n",
			step,nsigm,dcnt,hcnt);
      if( (dtot+htot+dcnt+hcnt) > framesz/maxfrac)	nsigm *= 1.1;
    }while( (dtot+htot+dcnt+hcnt) > framesz/maxfrac);

    if( dcnt==0 && hcnt==0)	break;
    for( x=0; x<framesz; ++x)	MaskBuf[x] += AddMask[x];
    dtot += dcnt;	htot += hcnt;
  }
  if( Verbose)
	printf(" Detected %d dark and %d hot pixels (%.2f sigma)\n",dtot,htot,nsigm);
  free(AddMask);
}


/****************************** Merge input Mask *****************************/

static void MergeMasks( struct FitsFile *InFits, short *MaskBuf, char *fname)
{
	struct FitsFile *IMFits;
	long x,y, xn,yn, cnt=0, cntda=0;

  if( !(IMFits= FitsOpenR(fname)))	return;

  if( Verbose)	printf(" Merging mask %s\n",fname);

  if( IMFits->naxis > 2)
	fprintf(stderr,"Warning: Using only 1.frame out of mask file %s\n",fname);

  for( y=0;  y<IMFits->n2;  ++y)
  {  yn= y + IMFits->SA_ymin - InFits->SA_ymin;

     for( x=0;  x<IMFits->n1;  ++x)
     {	/** fuer BSCALE muessen wir sowieso in float konvertieren **/
	if( FitsReadDouble( IMFits) > 0.1)	/** falls durch Rundungsfehler != 0.0 **/
	{ xn= x + IMFits->SA_xmin - InFits->SA_xmin;
	  cnt++;

	  if( Verbose>2)
		printf(" marked pixel at input coord (%ld,%ld) = data coord (%ld,%ld)\n",
			x,y, xn,yn );

	  if( xn>=0 && xn<InFits->n1 && yn>=0 && yn<InFits->n2)
	  {	MaskBuf[xn + yn*InFits->n1]= 50;	cntda++;	}
	}
     }
  }
  FitsClose(IMFits);
  printf(" found %ld marked bad pixels in %s\n"
	 "      (%ld pix in data area)\n",cnt,fname,cntda);
  if(cntda==0)  printf("\a");
}


/****************************** Merge Coord-file *****************************/

static void MergeCoord( struct FitsFile *InFits, short *MaskBuf, char *fname)
{
	char line[99], *strp;
	FILE *fp;
	long x,y;
	short val;

  if( !(fp= fopen(fname,"r")))
  {	fprintf(stderr," Can't open %s!\a\n",fname);	return;	}

  while( !feof(fp))
  {	fgets(line,99,fp);
	if( line[0]=='#')	continue;	/** Kommentar **/

	x= atoi(line) - 1;
	if( !(strp= strchr(line,' ')))
	{	printf(" bad coordinates: %s\n",line);	continue;	}

	while( *strp==' ')  strp++;
	y= atoi(strp) - 1;
	if( !(strp= strchr(strp,' ')))	val= 52;
			else		val= atoi(++strp);

	if( x>=0 && x<InFits->n1 && y>=0 && y<InFits->n2)
	{ if( Verbose)	printf(" (%3ld,%3ld) set to %d\n",x,y,val);
	  MaskBuf[ x + y*InFits->n1] = val;
	}
	else	printf(" %s => %ld,%ld out of range!\n",line,x,y);
  }
  fclose(fp);
}


/****************************** Merge Region *********************************/

static void MergeRegion( struct FitsFile *InFits, short *MaskBuf, char *fname)
{
	char line[99], *strp;
	FILE *fp;
	long x,y;

  if( !(fp= fopen(fname,"r")))
  {	fprintf(stderr," Can't open %s!\a\n",fname);	return;	}

  while( !feof(fp))
  {	fgets(line,99,fp);
	if( line[0]=='#')	continue;	/** Kommentar **/

	if( strncmp(line+1,"POINT(",6))
	{	fprintf(stderr,"Unknown region: %s\a\n",line);	continue;	}

	x= (short) (atof(line+7) + 0.5) - 1;	/** Runden! **/
	strp= strchr(line,',')+1;
	y= (short) (atof(strp) + 0.5) - 1;
	if( (strp= strchr(line,'\n')))	*strp= 0;

	if( x>=0 && y>=0 && x<InFits->n1 && y<InFits->n2 )
	{ if( Verbose)	printf(" %s => %sing %ld,%ld\n",
				line,line[0]=='-' ? "add":"remov", x,y);
	  MaskBuf[ x + InFits->n1*y]= line[0]=='-'? 42:0;
	}
	else	printf(" %s => %ld,%ld out of range!\n",line,x,y);
  }
  fclose(fp);
}


/*********************************** Badpixmask ************************************/

static void Help_bpm(void)
{
  printf(" USAGE: badpixmask [-help]\n"
	"  or:	badpixmask [-nsigm n] [-boxr r] [-maxbox r] [-steps s] [-badfrac f]\n"
	"		[-im mask] [-c coord-list] [-r region] Output-maskname\n"
	" where:\t-nsigm n	minimal no. of sigmas a bad pix must deviate [%.1f]\n"
	"\t-boxr r\t	min. radius of the box used to compute the mean [%ld]\n"
	"\t-maxbox r	max. radius of the box used to compute the mean [%ld]\n"
	"\t-steps s	maximum number of steps [%d]\n"
	"\t-badfrac f	maximal fraction of bad pixels in the frame [%ld]\n"
	"\t-im mask	name of predefined mask\n"
	"\t-c coord-list	ascii-file containing known bad pixels\n"
	"\t-r region	saoimage-region containing known bad pixels\n"
	"\tmaskname\t	name of fitsfile for the new mask\n\n",
	minnsigm, minboxr, maxboxr, maxsteps, maxfrac	);
}


void CreateBadpixmask(char *opts)
{
	struct FitsFile *MaskFits;
	float *MeanBuf;
	char *OutName=NULL, *hdrline, *argp, *next;
	long i,fr,framesz;

  if( opts && !strncmp(opts,"-h",2))	{ Help_bpm(); return; }

  if( !Fits) {	printf(" Nothing loaded - nothing to mask!\a\n");  return; }
  if( !(MaskFits= CopyFits(Fits)))	return;

  if( DefBitpix)  MaskFits->bitpix= DefBitpix;
  MaskFits->naxis= 2;	MaskFits->n3= 1;
  FitsAddHistKeyw( MaskFits, "bad pixel mask created");
  if( (hdrline= FitsFindEmptyHdrLine( MaskFits)))
	memcpy( hdrline,"COMMENT = Bad-pixel mask",24);

  if( !(MaskFits->Buffer.s= AllocBuffer(Fits->n1*Fits->n2*sizeof(short),"mask")))
  {	FitsClose(MaskFits);	return;		}

  memset( MaskFits->Buffer.s, 0, Fits->n1*Fits->n2*sizeof(short));

  /********** Nun aber los **********/

  if(Verbose)  printf(" badpixmask-opts: %s\n",opts);

  for( argp= strtok(opts," \t");  argp;  argp= strtok(NULL," \t"))
  {	next= strtok(NULL," \t");	/** all opts have an argument **/

	if(Verbose)  printf(" badpixmask-arg: %s : %s\n",argp,next?next:"(nada)");

	if( !strncmp(argp,"-bad",4) && next)	maxfrac = atoi(next);
   else	if( !strncmp(argp,"-box",4) && next)	minboxr = atoi(next);
   else	if( !strncmp(argp,"-maxbox",7) && next) maxboxr = atoi(next);
   else	if( !strncmp(argp,"-ns",3) && next)	minnsigm= atof(next);
   else	if( !strncmp(argp,"-st",3) && next)	maxsteps= atoi(next);
   else	if( !strncmp(argp,"-o",2)  && next)	OutName = next;
   else	if( !strncmp(argp,"-im",3) && next)	MergeMasks( Fits,MaskFits->Buffer.s, next);
   else	if( !strncmp(argp,"-c",2)  && next)	MergeCoord( Fits,MaskFits->Buffer.s, next);
   else	if( !strncmp(argp,"-r",2)  && next)	MergeRegion(Fits,MaskFits->Buffer.s, next);
   else	if( *argp=='-')
   {	fprintf(stderr," Unknown option: %s\a\n\n",argp);  Help_bpm();  }
   else	break;
  }
  if( (OutName= argp) == NULL)
  {	OutName= "badpixmask.fits";
	fprintf(stderr,"prespeckle-warning: no badpixmask-name given, using %s\n",OutName);
  }
  if(Verbose)	printf("Maskname is \"%s\"\n",OutName);

  /********** And now...Do it! **********/

  if( Fits->naxis > 2 && Fits->n3 > 1)
  { framesz= Fits->n1 * Fits->n2;
    if( !(MeanBuf= AllocBuffer(framesz*sizeof(float),"mean frame")))  return;

    for( i=0;  i<framesz;  ++i)
    {	MeanBuf[i]= 0.0;
	for( fr=0;  fr<Fits->n3;  ++fr)
		MeanBuf[i] += Fits->Buffer.f[ fr * framesz + i ];
	MeanBuf[i] /= (double)Fits->n3;
    }
  }
  else MeanBuf= Fits->Buffer.f;

  if(Verbose)	printf("Mask parameters:\n"
			" minnsigm= %f, minboxr= %ld, maxboxr= %ld, maxsteps= %d, maxfrac= %ld\n",
			  minnsigm,	minboxr,	maxboxr,    maxsteps,	  maxfrac);

  MakeBadpixMask( Fits,MeanBuf,MaskFits->Buffer.s);

  if(MeanBuf != Fits->Buffer.f)	 free(MeanBuf);

  FitsWriteAny( OutName,MaskFits, MaskFits->Buffer.s,16);
  FitsClose(MaskFits);		/** to free struct & Header **/
  printf("\n");
}

/******************************************************************************/
/*******			Correct Badpix				*******/
/******************************************************************************/

static long bp_minboxr=2, bp_mingood=4;

static int cmp_floats( float *a, float *b)
{
  if( *a > *b)	return  1;
  if( *a < *b)	return -1;
  return 0;
}


static void Help_bp(void)
{
  printf(" USAGE: badpix [-help]\n"
	"  or:	badpix [-median|-mean] [-boxr r] [-mingood n] [-m Mask] [-c coord]\n"
	" where:"
	"	-median		use median to replace bad pixels\n"
	"	-mean		use mean to replace bad pixels\n"
	"	-boxr r		min. r of the box used to compute the mean|median [%ld]\n"
	"	-mingood n	min. no. of good pixels used to replace a bad one [%ld]\n"
	"	-m mask		mask file: good pixels = 0, bad pixels <> 0\n"
	"	-c coord-list	ascii-file containing coordinates of bad pixels\n\n",
	bp_minboxr, bp_mingood	);
}

void CorrectBadpix(char *opts)
{
	short *MaskBuf;
	char  *argp, *next;
	short Medianflag=1;
	long  x,y,fr,xx,yy, r, cnt, offs, framesz;
	float val, sum, *medbuf=NULL;
	int medbfsz=0;

  if( opts && !strncmp(opts,"-h",2))	{ Help_bp(); return; }

  if( !Fits) {	printf(" Nothing loaded - nothing to correct!\a\n");  return; }

  framesz= Fits->n1*Fits->n2;
  if( !(MaskBuf= AllocBuffer(framesz*sizeof(short),"mask")))	return;

  memset( MaskBuf, 0, framesz*sizeof(short));

  /********** Nun aber los **********/

  if(Verbose)  printf(" badpix-opts: %s\n",opts);

  for( argp= strtok(opts," \t");  argp;  argp= strtok(NULL," \t"))
  {
	if(Verbose)  printf(" badpix-arg: %s\n",argp);

	if( !strncmp(argp,"-mea",4))	Medianflag=0;
   else	if( !strncmp(argp,"-med",4))	Medianflag=1;
   else	if( !strncmp(argp,"-box",4) && (next= strtok(NULL," \t")))  bp_minboxr= atoi(next);
   else	if( !strncmp(argp,"-min",4) && (next= strtok(NULL," \t")))  bp_mingood= atoi(next);
   else if( !strncmp(argp,"-m",2) && (next= strtok(NULL," \t")))  MergeMasks(Fits,MaskBuf,next);
   else	if( !strncmp(argp,"-c",2) && (next= strtok(NULL," \t")))  MergeCoord(Fits,MaskBuf,next);
   else	if( *argp=='-')
   {	fprintf(stderr," Unknown option: %s\a\n\n",argp);  Help_bp();  }
   else	break;
  }

  /********** Jetzt koennen wir flicken **********/

  for( y=0;  y<Fits->n2;  ++y)
    for( x=0;  x<Fits->n1;  ++x)
      if( MaskBuf[ x + y*Fits->n1])	/** bad pixel! **/
      { r= bp_minboxr;
	for( fr=0;  fr<Fits->n3;  ++fr)
	{ do
	  { if( Medianflag)
	    {	cnt= 2*r+1;	cnt *= cnt;
		if( Verbose && fr==0)
			printf("Bad pix at (%ld,%ld), boxradius %ld = %ld points\n",x,y,r,cnt);
		if( cnt > medbfsz)
		{ if( medbuf)  free(medbuf);
		  if( !(medbuf= AllocBuffer(cnt*sizeof(double),"median buffer"))) return;
		  medbfsz= cnt;
		}
	    }
	    cnt=0;	sum=0.0;

	    if( (yy= y-r) < 0)  yy=0;
	    for( ;  yy<=y+r && yy<Fits->n2;  ++yy)
	    {
	      if( (xx= x-r) < 0)  xx=0;
	      for( ;  xx<=x+r && xx<Fits->n1;  ++xx)
	      {
		offs= xx + yy*Fits->n1;
		if( !MaskBuf[offs])
		{ val= Fits->Buffer.f[ fr * framesz + offs ];
		  if(Medianflag) medbuf[cnt]= val;
			else	 sum += val;
		  cnt++;
		}
	      }
	    }
	    if( cnt<bp_mingood)
	    {	++r;
		if(fr==0)		/** don't complain about 2... frame **/
		  printf(" Too many bad pixels around (%ld,%ld),"
			 " increasing radius to %ld\n",	x,y,r);
	    }
	  }while( cnt<bp_mingood);	/** we want mean of at least some good pix **/

	  if( Medianflag)		/** sort values in medbuf **/
	  {	qsort( medbuf, cnt, sizeof(float), (void *)cmp_floats);

		if( cnt%2) val= medbuf[cnt/2];	/** cnt odd => middle def'ed **/
		else	   val= (medbuf[cnt/2] + medbuf[cnt/2+1]) / 2.0;
	  }
	  else val= sum / (double)cnt;	/** simple mean **/

	  if(Verbose)
		printf(" Replacing pixel at (%ld,%ld) by %s value %g\n",
				x,y, Medianflag? "median":"mean", val);

	  Fits->Buffer.f[ fr * framesz + x + y*Fits->n1] = val;
	} /** for( fr... **/
      }	/** if( bad pixel... **/

  /********** All done **********/

  FitsAddHistKeyw( Fits, "prespeckle: badpixels corrected");
}
