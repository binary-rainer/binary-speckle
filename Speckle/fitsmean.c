/* $Id: fitsmean.c 1.8 1996/07/13 19:39:44 koehler Exp koehler $
 *
 * Last Change: Tue Mar 19 17:00:44 2002
 *
 * $Log: fitsmean.c $
 * Revision 1.8  1996/07/13  19:39:44  koehler
 * vor Umstellung auf qsort() fuer -smooth_phase
 *
 * Revision 1.7  1995/08/19  20:11:29  koehler
 * median switch eingebaut
 *
 * Revision 1.6  1995/06/24  11:18:59  koehler
 * Vor Umstellung von SmoothPhase auf Median
 *
 * Revision 1.5  1995/05/08  09:10:20  koehler
 * vor Umstellung auf quadrantenweises Phase-smoothing
 *
 * Revision 1.4  1995/04/11  12:56:44  koehler
 * ganz kurz vor Einfuehrung von "-smooth_phase"
 *
 * Revision 1.3  1995/02/17  15:01:31  koehler
 * Revision 1.2  1995/01/17  11:04:47  koehler
 * Revision 1.1  1995/01/02  21:27:43  koehler
 * Initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fitsio.h"

#define PI	3.141592653
#define ZWEIPI	6.283185307

static long sqrarg;
#define SQR(a) ((sqrarg=(a))==0 ? 0L : sqrarg*sqrarg)


/********************************************************************************/
/* PrepSmoothTable(size_x,size_y)						*/
/* creates array contaning coordinates of all points, sorted by radius		*/
/********************************************************************************/

struct Point
{	int r;
	int x;
	int y;
} *smtbl;

long tablesz;

static int cmp_point( const struct Point *p1, const struct Point *p2)
{
	if( p1->r > p2->r )	return 1;
	if( p1->r < p2->r )	return -1;
	return 0;
}

short PrepSmoothTables(long sizex, long sizey)
{
	long sizexh,sizeyh;
	long x,y,i;

  printf(" creating tables for phases...\n");

  sizexh= sizex/2 + 1;	sizeyh= sizey/2 + 1;	tablesz= sizexh*sizeyh;

  if( !(smtbl= AllocBuffer(tablesz*sizeof(struct Point),"table of indices")))	return 1;

/** erst mal alle radien ausrechnen **/
  i=0;
  for( x=0;  x<sizexh;  ++x)
    for( y=0;  y<sizeyh;  ++y)
    {	smtbl[i].x= x;
	smtbl[i].y= y;
	smtbl[i].r= x*x+y*y;	++i;
    }

/** nach aufsteigendem radius sortieren **/

  qsort( smtbl, tablesz, sizeof(struct Point), (void *)cmp_point);

  return 0;	/** all right **/
}


/********************************************************************************/
/* PhaseDiffs(buffer,size_x,size_y)						*/
/* geht von aussen nach innen und ersetzt value durch diff zum inneren Nachbarn */
/* PhaseAdds(buffer,sizex,sizey)	macht genau das wieder rueckgaengig	*/
/********************************************************************************/

void PhaseDiffs( double *buffer, long sizex, long sizey)
{
	double *midptr;
	long xmid,ymid, k, x,y, x1,y1;

  xmid= sizex/2;	ymid= sizey/2;		midptr= buffer+(xmid+sizex*ymid);

  printf(" computing differences ... middle = (%ld,%ld)\n",xmid,ymid);

  for( k=tablesz;  k>1;  )	/** von aussen nach innen, ohne (0,0) **/
  {	--k;
	x1= x= smtbl[k].x;	if(x>0)  --x1;
	y1= y= smtbl[k].y;	if(y>0)	 --y1;

	if( x<xmid )
	{ if( y< ymid)	    midptr[ x + sizex*y] -= midptr[ x1 + sizex*y1];
	  if( y && y<=ymid) midptr[ x - sizex*y] -= midptr[ x1 - sizex*y1];
	}
	if( x && x<=xmid)		/** nur wenn x!=-x, -xmid <= -x **/
	{ if( y< ymid)	    midptr[-x + sizex*y] -= midptr[-x1 + sizex*y1];
	  if( y && y<=ymid) midptr[-x - sizex*y] -= midptr[-x1 - sizex*y1];
	}
  }
}

void PhaseAdds( double *buffer, long sizex, long sizey)
{
	double *midptr;
	long xmid,ymid, k, x,y, x1,y1;

  xmid= sizex/2;	ymid= sizey/2;		midptr= buffer+(xmid+sizex*ymid);

  printf(" adding differences ... middle = (%ld,%ld)\n",xmid,ymid);

  for( k=1;  k<tablesz;  ++k)	/** von innen nach aussen, ohne (0,0) **/
  {
	x1= x= smtbl[k].x;	if(x>0)  --x1;
	y1= y= smtbl[k].y;	if(y>0)	 --y1;

	if( x<xmid )
	{ if( y< ymid)	    midptr[ x + sizex*y] += midptr[ x1 + sizex*y1];
	  if( y && y<=ymid) midptr[ x - sizex*y] += midptr[ x1 - sizex*y1];
	}
	if( x && x<=xmid)		/** nur wenn x!=-x, -xmid <= -x **/
	{ if( y< ymid)	    midptr[-x + sizex*y] += midptr[-x1 + sizex*y1];
	  if( y && y<=ymid) midptr[-x - sizex*y] += midptr[-x1 - sizex*y1];
	}
  }
}


/********************************************************************************/
/* SmoothPhase(buffer,size_x,size_y)						*/
/* geht von innen nach aussen durch das Bild und buegelt Spruenge um 2pi aus	*/
/********************************************************************************/

#define BUFF(x,y)	(buffer[y*sizex + x])
#define BOXR		2

void SmoothPhasePoint( double *buffer, long sizex, long sizey, long x, long y, long r,
			short allpnts, short median )
{
	double medbuf[25];	/** bei 5x5-Umgebung braucht man nicht mehr **/
	double val;
	long cnt, xmid,ymid, xx,yy, mini;

  if( x<0 || x>=sizex || y<0 || y>=sizey)  return;	/** out of area **/

  cnt= 0;  val= 0.0;	xmid= sizex/2;	ymid= sizey/2;

  if( (xx=x-BOXR) < 0)	xx=0;
  for( ;  xx<=x+BOXR && xx<sizex;  ++xx)	/** je BOXR Punkte links und rechts **/
  { if( (yy=y-BOXR) < 0)  yy=0;
    for( ;  yy<=y+BOXR && yy<sizey;  ++yy)	/** je BOXR Punkte oben und unten **/
    {
	if( allpnts || (SQR(xx-xmid) + SQR(yy-ymid)) < r)
	{	val += (medbuf[cnt]= BUFF(xx,yy));	cnt++;	}
    }
  }
  if(median)
  {	/** jetzt den Kram im medbuf sortieren (soweit noetig) **/
    for( xx=0;  xx<cnt;  xx++)
    { val= medbuf[xx];	mini=xx;
      for( yy= xx+1;  yy<cnt;  yy++)	/** suche Minimum im Bereich xx...ende **/
	if( val > medbuf[yy])	{ val= medbuf[yy];  mini=yy;	}

      if( mini!=xx)	/** min. nicht dort, wo's hinsoll **/
      {	val= medbuf[xx];  medbuf[xx]= medbuf[mini];  medbuf[mini]= val;	}
    }
	/** bei gerader Anzahl Frames nehmen wir Mittelwert der beiden mittleren **/
    if( cnt%2==0)	/** gerade **/
	 val= (medbuf[cnt/2-1] + medbuf[cnt/2]) / 2.0;	/** z.B. cnt=4 => medbuf[1]+medbuf[2] **/
    else val= medbuf[cnt/2];				/** z.B. cnt=5 => medbuf[2]	**/
  }
  else	val /= cnt;	/** so einfach ist der simple Mittelwert... **/

    while( BUFF(x,y) < val-PI)
    {	/*printf(" increasing (%3ld,%3ld), since %5g < %5g-pi\n",x,y,BUFF(x,y),val);*/
	BUFF(x,y) += ZWEIPI;
    }
    while( BUFF(x,y) > val+PI)
    {	/*printf(" decreasing (%3ld,%3ld), since %5g > %5g-pi\n",x,y,BUFF(x,y),val);*/
	BUFF(x,y) -= ZWEIPI;
    }

}


void SmoothPhase( double *buffer, long sizex, long sizey)
{
	long xmid,ymid, x,y, k;

/** wir gehen alle Punkte durch und sehen nach, ob's einen Sprung gibt **/
	/** ohne Punkte (0,0)=0, (1,0)=1, (0,1)=2, (-1,0)=3, (0,-1)=4 **/

  xmid= sizex/2;	ymid= sizey/2;
  printf(" smoothing frame ... middle = (%ld,%ld)\n",xmid,ymid);

  for( k=5;  k<tablesz;  ++k)	/** alle pkte mit inneren Nachbarn vergleichen (median) **/
  {	x= smtbl[k].x;	y= smtbl[k].y;
	SmoothPhasePoint( buffer,sizex,sizey, xmid+x, ymid+y, smtbl[k].r, 0,1);
	SmoothPhasePoint( buffer,sizex,sizey, xmid+x, ymid-y, smtbl[k].r, 0,1);
	SmoothPhasePoint( buffer,sizex,sizey, xmid-x, ymid+y, smtbl[k].r, 0,1);
	SmoothPhasePoint( buffer,sizex,sizey, xmid-x, ymid-y, smtbl[k].r, 0,1);
  }
  for( k=5;  k<tablesz;  ++k)	/** alle pkte mit inneren Nachbarn vergleichen (average) **/
  {	x= smtbl[k].x;	y= smtbl[k].y;
	SmoothPhasePoint( buffer,sizex,sizey, xmid+x, ymid+y, smtbl[k].r, 0,0);
	SmoothPhasePoint( buffer,sizex,sizey, xmid+x, ymid-y, smtbl[k].r, 0,0);
	SmoothPhasePoint( buffer,sizex,sizey, xmid-x, ymid+y, smtbl[k].r, 0,0);
	SmoothPhasePoint( buffer,sizex,sizey, xmid-x, ymid-y, smtbl[k].r, 0,0);
  }
  for( k=5;  k<tablesz;  ++k)	/** alle pkte mit allen Nachbarn vergleichen (average) **/
  {	x= smtbl[k].x;	y= smtbl[k].y;
	SmoothPhasePoint( buffer,sizex,sizey, xmid+x, ymid+y, smtbl[k].r, 1,1);
	SmoothPhasePoint( buffer,sizex,sizey, xmid+x, ymid-y, smtbl[k].r, 1,1);
	SmoothPhasePoint( buffer,sizex,sizey, xmid-x, ymid+y, smtbl[k].r, 1,1);
	SmoothPhasePoint( buffer,sizex,sizey, xmid-x, ymid-y, smtbl[k].r, 1,1);
  }
}


/*****************************************************************************/

void CalcMedian( double *medbuf, double *sgmbuf, double *inbuff, long frcnt, long framesz)
{
	double val,sum2;
	long i,fr,fr2,frh,minfr;

  frh= (frcnt-1)/2;	/** index of median (or 0.5 more), we ignore the highest value **/

  for( i=0;  i<framesz;  ++i)		/** foreach pixel **/
  { for( fr=0;  fr<frcnt;  ++fr)	/** foreach frame **/
    {	val = inbuff[fr*framesz + i];
	minfr= fr;
	for( fr2=fr+1;  fr2<frcnt;  ++fr2)	/** search min value in frames >= fr **/
	{	if( val > inbuff[fr2*framesz + i])
		{	val= inbuff[fr2*framesz + i];	minfr= fr2;	}
	}
	if( minfr != fr)	/** min not where it belongs **/
	{	/* val= inbuff[minfr*framesz + i];	-- already in fr2-loop */
		inbuff[minfr*framesz + i]= inbuff[fr*framesz + i];
		inbuff[fr*framesz + i]   = val;
	}
    }	/** now we have sorted the values of pixel i **/
    medbuf[i]= frcnt%2 ? inbuff[frh*framesz + i] :	/** <- frcnt odd, v- frcnt even **/
			(inbuff[(frh-1)*framesz + i] + inbuff[frh*framesz + i]) / 2.0;

    if(sgmbuf)		/** sigma will er auch noch **/
    {
	for( sum2=0.0, fr=0;  fr<frcnt;  ++fr)		/** sum over each frame **/
	{	val= inbuff[fr*framesz + i] - medbuf[i];	/** delta to median **/
		sum2 += val*val;
	}
	sgmbuf[i]= sqrt( sum2 / (double)(frcnt * (frcnt-1)));
    }
  }
}

/*****************************************************************************/

void Usage(char *prg)
{
  printf(" Usage: %s [-b bitpix] [-o Outputfile] [-s Sigmafile] [-dbg debugfile]\n"
	 "	[-nosigma] [-median] [-skipmax] [-delta] [-phase] [-smooth_phase]\n"
	 "	[-weights] input [weight]...\n",
	prg);
}

void Help(char *prg)
{
 Usage(prg);
 printf(" where:\t-b bitpix	gives data format of output\n"
	"	-o filename	gives name of mean\n"
	"	-s filename	gives name of sigma\n"
	"	-nosigma	suppresses writing of sigma file\n"
	"	-median		give median instead of mean values\n"
	"	-skipmax	ignores maximal value of each pixel\n"
	"	-delta		treats values as errors and adds them\n"
	"	-phase		use phase difference method to compute mean\n"
	"	-smooth_phase	removes jumps in phase before averaging\n"
	"	-weights	every input-file is followed by its weight\n"
	"	input		the input in FITS-format\n"
	"	weight		the weight of the preceding file\n\n"	);
}

int main( int argc, char **argv)
{
	struct FitsFile *InFits, OutFits;	/** OutFits is no ptr! **/
	double *inbuf, *mnbuf, *sgbuf, *mxbuf, sumwgt, curwgt, val;
	char line[100], sharp[10], *Outname=NULL, *Sgmname=NULL, *Dbgname=NULL;
	short wrt_sgm=1, median=0, skip_max=0, delta=0, phase=0, smooth_phase=0, weights=0;
	long framesz, wordsz, frcnt, arg,fr,i;

  printf("\nfitsmean, compiled on "__DATE__" ========================================\n");

  OutFits.bitpix= 0;	/** Default: same as input **/

/***** Let's see what the user wants... *****/

  for( arg=1;  arg<argc;  ++arg)
  {
	 if( !strncmp(argv[arg],"-nosi",5))	wrt_sgm= 0;
    else if( !strncmp(argv[arg],"-med" ,4))	median = 1;
    else if( !strncmp(argv[arg],"-skip",5))	skip_max=1;
    else if( !strncmp(argv[arg],"-del" ,4))  {	delta =	 1;	wrt_sgm=0;	}
    else if( !strncmp(argv[arg],"-pha" ,4))	phase =  1;
    else if( !strncmp(argv[arg],"-smoo",5))	smooth_phase=1;
    else if( !strncmp(argv[arg],"-wei",4))	weights= 1;
    else if( !strncmp(argv[arg],"-dbg",3) && ++arg<argc)  Dbgname= argv[arg];
    else if( !strncmp(argv[arg],"-b",2) && ++arg<argc)
					  OutFits.bitpix= atoi(argv[arg]);
    else if( !strncmp(argv[arg],"-o",2) && ++arg<argc)  Outname= argv[arg];
    else if( !strncmp(argv[arg],"-s",2) && ++arg<argc)  Sgmname= argv[arg];
    else if( !strncmp(argv[arg],"-h",2))
	 {	Help(argv[0]);		return 0;	}
    else if( argv[arg][0]=='-')
	 {	fprintf(stderr," Unknown option: %s\a\n\n",argv[arg]);
		Help(argv[0]);		return 10;
	 }
    else break;
  }
  if( arg>=argc)
  {	fprintf(stderr," No input file specified!\n");
	Usage(argv[0]);		return 10;
  }
  if( median && weights)
  {	fprintf(stderr," Don't know how to use weights for median, ignoring them\a\n\n");  }

  if( skip_max && weights)
  {	fprintf(stderr,	" I'm not sure if I handle -skipmax and -weights correctly.\n"
			" I'll do my best, but don't complain if it's wrong!\a\n\n");
  }

/***** Look at the 1.file to determine the frame size etc.*****/

  if( !(InFits= FitsOpenR( argv[arg])))	return 10;

  framesz= InFits->n1 * InFits->n2;
  if( !(wordsz= FitsWordSize(InFits)))	return 20;	/** unknown bitpix **/

  OutFits.fp= NULL;
  OutFits.naxis= 2;
  OutFits.n1= InFits->n1;  OutFits.n2= InFits->n2;  OutFits.n3= 0;

  if( OutFits.bitpix)	/** user-given? **/
  {
    if( (OutFits.bitpix !=-32) && (OutFits.bitpix !=-64) &&
	(OutFits.bitpix != 8) && (OutFits.bitpix != 16) && (OutFits.bitpix != 32))
    {	fprintf(stderr," Bad bitpix value %d, using %d\a\n",OutFits.bitpix,InFits->bitpix);
	OutFits.bitpix= InFits->bitpix;
    }
  }
  else	OutFits.bitpix= InFits->bitpix;

  if( !(OutFits.Header= AllocBuffer(InFits->HeaderSize,"output header")))
	return 21;
  memcpy(OutFits.Header,InFits->Header,InFits->HeaderSize);
  OutFits.HeaderSize= InFits->HeaderSize;

  if(phase || smooth_phase)	PrepSmoothTables(InFits->n1,InFits->n2);
  fits_close(InFits);

/***** Allocate & clear the buffers *****/

  if( !Outname)
  {	char *gzptr;

	Outname= malloc(strlen(argv[arg]) + 6);
	strcpy(Outname,argv[arg]);
	gzptr = Outname + strlen(Outname) - 3;	/** ".gz" at end? **/
	if (strcmp(gzptr,".gz")==0)  *gzptr= 0; /** yes, dump it  **/
	strcat(Outname,"_mean");
  }
  if( !Sgmname)
  {	if( Outname)
	{  Sgmname= malloc(strlen(Outname) + 7);    strcpy(Sgmname,Outname);	}
	else
	{  Sgmname= malloc(strlen(argv[arg]) + 7);  strcpy(Sgmname,argv[arg]);	}
	strcat(Sgmname,"_sigma");
  }
  if( !(inbuf= AllocBuffer( framesz*sizeof(double),"input buffer")))  return 22;
  if( !(mnbuf= AllocBuffer( framesz*sizeof(double), "mean buffer")))  return 23;
  if( !(sgbuf= AllocBuffer( framesz*sizeof(double),"sigma buffer")))  return 24;
  if( skip_max &&
	!(mxbuf= AllocBuffer( framesz*sizeof(double),"maximum buffer")))  return 25;

  for( i=0;  i<framesz;  ++i)	mnbuf[i]= sgbuf[i]= 0.0;

/***** And now, add every frame of every file *****/

  frcnt=0;	sumwgt=0.0;	curwgt=1.0;

  for( ;  arg<argc;  arg += weights? 2:1)	/** for each input file **/
  { if( !(InFits= FitsOpenR(argv[arg])))
    {	fprintf(stderr," Can't open %s - skipping file\n",argv[arg]);	continue;  }

    if(weights)	 curwgt= (arg+1<argc)? atof(argv[arg+1]) : 1.0;
    printf(" Reading %s (%d frames), weight %g\n",argv[arg],InFits->n3,curwgt);

    if( InFits->n1 != OutFits.n1 || InFits->n2 != OutFits.n2)
    {	fprintf(stderr," Frame sizes do not match - skipping file\n");	}
    else
    {	if( InFits->flags & FITS_FLG_SHARP)	/** skip statistics frame **/
	{	char *strp;

	  fread(inbuf,FitsWordSize(InFits),framesz,InFits->fp);
	  fread(sharp,1,8,InFits->fp);
	  if( (strp= FitsFindEmptyHdrLine(InFits)))
		sprintf(strp,"CAMERA  = %20s   / %-44s",
			/**   0123456789A...C12345....^79	**/
				"'SHARP'", "camera used for data aquisition");
	}

	if( median)
	{ fr= frcnt;	frcnt += InFits->n3;
	  if( !(inbuf= realloc(inbuf,frcnt*framesz*sizeof(double))))
	  { fprintf(stderr," Sorry - no memory for input buffer\a\n");
	    FitsClose(InFits);	exit(20);
	  }
	  for( ; fr<frcnt; fr++)
	  { if( FitsReadDoubleBuf(InFits, &inbuf[framesz*fr], framesz) < framesz)
	    {	fprintf(stderr," Not enough data in %s\n",argv[arg]);
		fprintf(stderr," I'm too confused to go on!\a\n");
		FitsClose(InFits);	exit(19);
	    }
	    if( InFits->flags & FITS_FLG_SHARP)  fread(sharp,1,8,InFits->fp);
	  }
	}
	else	/** not median, mean|delta ! **/
	{
	  for( fr=0;  fr<InFits->n3;  ++fr)
	  {	/* printf("%ld...",f);	 fflush(stdout); */

	    if( FitsReadDoubleBuf(InFits,inbuf,framesz) < framesz)
	    {	fprintf(stderr,"Not enough data in %s\n",argv[arg]);	break;	}

	    if(phase)		PhaseDiffs( inbuf,InFits->n1,InFits->n2);
	    if(smooth_phase)	SmoothPhase(inbuf,InFits->n1,InFits->n2);

	    for( i=0;  i<framesz;  ++i)
	    {	val= inbuf[i];
		if(delta)  val *= val;
		mnbuf[i] += curwgt * val;
		sgbuf[i] += curwgt * val*val;
		if( skip_max && (frcnt==0 || mxbuf[i] < curwgt * val))
							mxbuf[i]= curwgt * val;
	    }
	    sumwgt += curwgt;	frcnt++;
	    if( InFits->flags & FITS_FLG_SHARP)  fread(sharp,1,8,InFits->fp);
	  }
	}
    }
    FitsClose(InFits);
  }
/***** Calculate mean/median & sigma *****/

  if(wrt_sgm && (frcnt==1 || (skip_max && frcnt==2)))
	printf(" Only 1 frame - can't calculate sigma\n");

  if(median)	CalcMedian( mnbuf, wrt_sgm? sgbuf:NULL, inbuf,frcnt,framesz);
  else
  { if( skip_max)  sumwgt -= 1.0;	/** forgot the curval auf max **/

    for( i=0;  i<framesz;  ++i)
    {	if(frcnt>1)
	{ sgbuf[i] -= mnbuf[i] / sumwgt * mnbuf[i];	/** (Sx^2) - (Sx)^2/N	**/
	  sgbuf[i] /= sumwgt * (sumwgt-1);		/**      N (N-1)	**/
	  sgbuf[i] = sqrt(sgbuf[i]);
	}
	if( skip_max)	mnbuf[i] -= mxbuf[i];
	if( delta)	mnbuf[i]= sqrt(mnbuf[i]);
		/** MW=Sum(x_i)/N =>(Fehlerfortpfl.)=> DMW= sqrt(Sum(Dx_i^2))/N **/

	mnbuf[i] /= sumwgt;
    }
  }
  if(phase)	PhaseAdds( mnbuf,InFits->n1,InFits->n2);

/***** Prepare Header of output *****/

  sprintf(line,"BITPIX  = %20d   / %-45s",OutFits.bitpix,"Data Type");
  FitsSetHeaderLine( &OutFits, "BITPIX  = ", line);

  sprintf(line,"NAXIS   = %20d   / %-45s",2,"number of axes");
  FitsSetHeaderLine( &OutFits, "NAXIS   = ", line);

  FitsSetHeaderLine( &OutFits, "NAXIS3  = ", NULL);	/** clear it **/

/***** write mean/median frame *****/

  for( val=0.0, i=0;  i<framesz;  ++i)	val += mnbuf[i];
  sprintf(line,"INTEGRAL= %20g   / %-45s",val,"sum of pixel values");
  FitsSetHeaderLine( &OutFits, "INTEGRAL= ", line);

  FitsAddHistKeyw( &OutFits, median? "median frame created" :
			    (delta? "error sum frame created" : "mean frame created"));

  FitsWrite( Outname,&OutFits,mnbuf);

/***** write sigma frame *****/
  if(wrt_sgm && frcnt>1)
  {
	for( val=0.0, i=0;  i<framesz;  ++i)	val += sgbuf[i];
	sprintf(line,"INTEGRAL= %20g   / %-45s",val,"sum of pixel values");
	FitsSetHeaderLine( &OutFits, "INTEGRAL= ", line);

	FitsAddHistKeyw( &OutFits, "sigma frame created");
	FitsWrite( Sgmname,&OutFits,sgbuf);
  }

/***** debug something *****/
  if( Dbgname)
  {	OutFits.naxis=3;  OutFits.n3= frcnt;	FitsWrite( Dbgname,&OutFits,inbuf);	}

  if(smooth_phase)  free(smtbl);
  if(skip_max)	free(mxbuf);
  free(sgbuf);
  free(mnbuf);
  free(inbuf);
  return 0;
}
