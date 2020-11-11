/* $Id: despeckle.c,v 1.3 1996/12/05 16:39:10 koehler Exp koehler $
 * $Log: despeckle.c,v $
 * Revision 1.3  1996/12/05  16:39:10  koehler
 * Letzte Version vor Apgraeid auf Knox-Thompson
 *
 * Revision 1.2  1996/08/09  12:49:45  koehler
 * lauffaehige Version.
 * Die vorherige Version versucht, das gesamte Bispektrum im Speicher
 * zu halten => braucht typischerweise 200 MB Ram.
 * Ansonsten sind bestimmt noch jede Menge Bugs drin, da ich's nicht
 * testen konnte.
 *
 * Revision 1.1  1996/08/02  09:18:27  koehler
 * Initial revision
 *
 * Created:     Mon Jul 29 18:37:31 1996 by Koehler@Lisa
 * Last change: Mon Nov 20 16:13:31 2000
 *
 * This file is Hellware!
 * It will send your most important data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "fitsio.h"

/*#define PARANOID 1*/
/*#define DEBUG 1*/

#define PI	3.141592653
#define ZWEIPI	6.283185307

#define OFF(x,y)	(frmid+(x)+sizex*(y))	/** offset into PwrBuf/PhaBuf **/

#define SMOOTH_BOXR	2


float ***ObjPwr=NULL, ***ObjPha=NULL, ***RefPwr=NULL, ***RefPha=NULL, *Phase, *dPhase;

long  ObjFrames=0, RefFrames=0;
long  sizex,sizey, midy,frmid;	/** size of frame, offset of middle **/

short fill_corners=0, subdist=15, verbose=0;

struct FitsFile OutFits;

/*************************************************************************/
/**	Create an Array of Points, sorted by distance to origin		**/
/*************************************************************************/

struct Point
{	int rq;
	int x;
	int y;
} *Pnttbl;

long Pnttblsz;

static int cmp_point( const struct Point *p1, const struct Point *p2)
{
	if( p1->rq > p2->rq )	return 1;
	if( p1->rq < p2->rq )	return -1;
	return 0;
}

static short PrepPointTable(void)
{
	struct Point *Pptr;
	long sizexh,sizeyh;
	long x,y;

  printf(" creating point table\n");

  sizexh= sizex/2 + 1;	sizeyh= sizey/2 + 1;	Pnttblsz= sizexh*sizeyh;

  if( !(Pnttbl= AllocBuffer(Pnttblsz*sizeof(struct Point),"point table")))	return 1;

/** erst mal alle radien ausrechnen **/

  for( x=0;  x<sizexh;  ++x)
    for( y=0;  y<sizeyh;  ++y)
    {	Pptr= &Pnttbl[x + y*sizeyh];
	Pptr->rq= x*x + y*y;	Pptr->x= x;	Pptr->y= y;
    }

/** nach aufsteigendem radius sortieren **/

  qsort( Pnttbl, Pnttblsz, sizeof(struct Point), (void *)cmp_point);

  return 0;	/** all right **/
}


/*************************************************************************/
/**		Read Data and Create Pointed Arrays			**/
/*************************************************************************/

/** As one can see from the size calculation, the number of pointers **/
/** will be minimal for:	d1 <= d2 <= d3			     **/

void ***Build3DPntdArray( int d1, int d2, int d3, size_t elemsize)
{
	void ***CubePtr;
	void ***FramePtAr, **RowPtAr;
	char *PixArr;	/** don't use void * here, nobody knows how big **/
			/** it is and what results if you increment it! **/
	long PtrPhalanxSz, BufSz;
	int i1, i2;

  PtrPhalanxSz= (d1 * (1 + d2));	/** this is not in bytes! **/
	/**  frame^ptrs, row^ptrs **/

  /** doubles MUST be aligned on 8byte boundaries.  Pointers are only **/
  /** 4 bytes long, so we have to make sure we have an even number!!! **/
  if(PtrPhalanxSz % 2)	++PtrPhalanxSz;

  /** By the way, we rely on the fact that all pointers have the same size **/

  BufSz= PtrPhalanxSz*sizeof(void *) + d1*d2*d3*elemsize;
#ifdef PARANOID
  fprintf(stderr," allocating %d * %d * %d elements = %ld bytes\n",d1,d2,d3,BufSz);
#endif

  if( !(CubePtr= AllocBuffer(BufSz,"cube buffer" )))	return NULL;

  /** the Array of FramePtrs is simply at the start of the buf: **/
	FramePtAr= CubePtr;

  /** the Array of RowPtrs is behind the Array of FramePtrs: **/
	RowPtAr	=  (void **)(FramePtAr + d1);

  /** the Array of Pixels is - no, not behind the Array of RowPtrs, **/
  /** it's behind the "Pointer Phalanx" plus the alignment space!   **/
	PixArr	=  (char *)(CubePtr + PtrPhalanxSz);

  for( i1=0;  i1<d1;  ++i1)
  {
#ifdef PARANOID
	fprintf(stderr," filling ptr array, i1=%d\n",i1);
#endif

     *FramePtAr++ = RowPtAr;
     for( i2=0;  i2<d2;  ++i2)
     {
	*RowPtAr++ = PixArr;
	memset(PixArr,0,d3*elemsize);	/** clear this row **/
	PixArr += d3 * elemsize;	/** goto next row **/
     }
  }
  return CubePtr;
}

/***** returns number of next option-arg *****/

static int ReadCubes( float ****PwrPtr, float ****PhaPtr, long *NFrmPtr,
			char **argv, int argc, int arg)
{
	struct FitsFile *FitsPwr, *FitsPha;
	float **PwrBuf, **PhaBuf;
	long framesz, PwrWordsz, PhaWordsz;
	int  cb, tfr, fr, x,y;
	char fname[256];

  /****** 1. count the frames *****/

  for( cb= arg; cb<argc && argv[cb][0]!='-';  ++cb)
  {
#ifdef DEBUG
    fprintf(stderr,"arg[%d]: %s\n",arg,argv[arg]);
#endif

    strcpy(fname,argv[arg]);	strcat(fname,"_pwr");
    if( !(FitsPwr= FitsOpenR(fname)))	continue;

    strcpy(fname,argv[arg]);	strcat(fname,"_pha");
    if( !(FitsPha= FitsOpenR(fname))) {	FitsClose(FitsPwr);  continue;	}
#ifdef DEBUG
    fprintf(stderr," %s_pwr & pha opened...\n",argv[arg]);
#endif

    if(	(FitsPwr->n1 != FitsPha->n1)
	|| (FitsPwr->n2 != FitsPha->n2) || (FitsPwr->n3 != FitsPha->n3) )
    {	fprintf(stderr," Sizes of power and phase not equal, skipping file.\a\n");
	FitsClose(FitsPwr);	FitsClose(FitsPha);	continue;
    }
    if( sizex==0)	/***** very first cube *****/
    {	sizex= FitsPwr->n1;	sizey= FitsPwr->n2;
	framesz= sizex*sizey;	midy= sizey/2;	frmid= sizex/2 + sizex*midy;

	if( (OutFits.Header == NULL) && (PwrPtr== &ObjPwr))
	{ OutFits.n1= sizex;	OutFits.n2= sizey;
	  if( !(OutFits.Header= AllocBuffer(FitsPwr->HeaderSize,"output header")))
		return 0;
	  memcpy( OutFits.Header,FitsPwr->Header,FitsPwr->HeaderSize);
	  OutFits.HeaderSize= FitsPwr->HeaderSize;
	}
    }
    else
    {	if( (sizex != FitsPwr->n1) || (sizey != FitsPwr->n2))
	{	fprintf(stderr," Frames sizes do not match, skipping file.\a\n");
		FitsClose(FitsPwr);	FitsClose(FitsPha);	continue;
	}
    }
    *NFrmPtr += FitsPwr->n3;	FitsClose(FitsPwr);	FitsClose(FitsPha);
  }
  if( *NFrmPtr == 0)
  {	fprintf(stderr,"No data found! (last file was%s)\a\n",fname);  return 0; }

  /***** 2. Allocate the buffers *****/

  printf(" Allocating two cubes with %ld entries each\n",framesz * *NFrmPtr);

  if( !( *PwrPtr= (void*)Build3DPntdArray( *NFrmPtr, sizex/2,sizey, sizeof(float))))
  {	printf("Couldn't build PwrPtr\a\n");	return 0;	}

  if( !( *PhaPtr= (void*)Build3DPntdArray( *NFrmPtr, sizex/2,sizey, sizeof(float))))
  {	free(*PwrPtr);	*PwrPtr=NULL;
	printf("Couldn't build PhaPtr\a\n");	return 0;
  }
  /***** 3. Fill the buffers *****/

  tfr= 0;	/** "total frame #" - counts across files **/

  for( cb= arg; cb<argc && argv[cb][0]!='-';  ++cb)
  {
#ifdef DEBUG
    fprintf(stderr,"arg[%d]: %s\n",arg,argv[arg]);
#endif

    strcpy(fname,argv[arg]);	strcat(fname,"_pwr");
    if( !(FitsPwr= FitsOpenR(fname)))	continue;

    strcpy(fname,argv[arg]);	strcat(fname,"_pha");
    if( !(FitsPha= FitsOpenR(fname))) {	FitsClose(FitsPwr);  continue;	}

    if(	(FitsPwr->n1 != sizex) || (FitsPha->n1 != sizex) ||
	(FitsPwr->n2 != sizey) || (FitsPha->n2 != sizey) || (FitsPwr->n3 != FitsPha->n3))
    {
	FitsClose(FitsPwr);	FitsClose(FitsPha);	continue;
    }
    printf(" Reading %s_pwr & pha...\n",argv[arg]);

    PwrWordsz= FitsWordSize(FitsPwr);
    PhaWordsz= FitsWordSize(FitsPha);

    FitsPwr->flags &= ~(FITS_FLG_EOF|FITS_FLG_ERROR);
    FitsPha->flags &= ~(FITS_FLG_EOF|FITS_FLG_ERROR);

    for( fr=0;  fr<FitsPwr->n3;  ++fr, ++tfr)
    {	if(verbose>1)	{ printf(" Frame %d\r",fr);	fflush(stdout); }
	PwrBuf= (*PwrPtr)[tfr];
	PhaBuf= (*PhaPtr)[tfr];

	for( y=0;  y<sizey;  ++y)
	{ fseek( FitsPwr->fp, sizex/2*PwrWordsz, SEEK_CUR);	/** skip redun- **/
	  fseek( FitsPha->fp, sizex/2*PhaWordsz, SEEK_CUR);	/** dant halves **/

	  for( x=0;  x<sizex/2;  ++x)
	  {	/** the output of fitsFFT is Re*Re+Im*Im **/
		PwrBuf[x][y] = (float) sqrt(FitsReadDouble(FitsPwr));
		PhaBuf[x][y] = (float) FitsReadDouble(FitsPha);

		if( (FitsPwr->flags|FitsPha->flags) & (FITS_FLG_EOF|FITS_FLG_ERROR))
		{	y= fr= 32000;	break;	}
	  }
	}
    }
    FitsClose(FitsPwr);		FitsClose(FitsPha);
  }	/** for cb **/

  return cb;
}


/*************************************************************************/
/**			Smooth Phase at one Point			**/
/*************************************************************************/

/** x,y sind relativ zur Mitte (kommen aus Pnttbl) **/

void SmoothPhasePoint( int x, int y, int rq, short allpnts, short median )
{
	double medbuf[25];	/** bei 5x5-Umgebung braucht man nicht mehr **/
	double val;
	long cnt, halfx,halfy, xx,yy, mini, offs;

  cnt= 0;  val= 0.0;	halfx= sizex/2;	 halfy= sizey/2;

  if( (xx= x-SMOOTH_BOXR) < -halfx)	 xx= -halfx;
  for( ;  xx<= x+SMOOTH_BOXR && xx<halfx;  ++xx)
  {
    if( (yy= y-SMOOTH_BOXR) < -halfy)  yy= -halfy;
    for( ;  yy<= y+SMOOTH_BOXR && yy<halfy;  ++yy)
    {
	if( allpnts || ((xx*xx + yy*yy) < rq))
	{	val += (medbuf[cnt]= Phase[ OFF(xx,yy)]);	cnt++;	}
    }
  }
  if(median)		/** jetzt den Kram im medbuf sortieren (soweit noetig) **/
  {
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

  offs= OFF(x,y);
  while( Phase[offs] < val-PI)
  {	/*fprintf(stderr," increasing (%3ld,%3ld), since %5g < %5g-pi\n",x,y,BUFF(x,y),val);*/
	Phase[offs] += ZWEIPI;
  }
  while( Phase[offs] > val+PI)
  {	/*fprintf(stderr," decreasing (%3ld,%3ld), since %5g > %5g-pi\n",x,y,BUFF(x,y),val);*/
	Phase[offs] -= ZWEIPI;
  }
}


/*************************************************************************/
/**		Compute Bispectrums-modulus of one point		**/
/*************************************************************************/

static
double BiModul( float ***PwrCube, float ***PhaCube, long nframes,
		int x1, int y1, int x2, int y2)
{
	float **PwrPtr, **PhaPtr;
	double Pwr, Pha,Ph, BiReal, BiImag;
	short phs1,phs2,phs3;	/** Phase Signs **/
	long fr;
	int x3,y3;

  /***** We read only the right half of the frames, i.e. x>=0 *****/

  phs1= phs2= phs3= 1;	x3= x1+x2;  y3= y1+y2;

  if( x1<0) {	x1= -x1;  y1= -y1;  phs1= -1;	}
  if( x2<0) {	x2= -x2;  y2= -y2;  phs2= -1;	}
  if( x3<0) {	x3= -x3;  y3= -y3;  phs3= -1;	}

  BiReal= BiImag= 0.0;
  for( fr=0;  fr < nframes;  ++fr)
  {
	PwrPtr= PwrCube[fr];	PhaPtr= PhaCube[fr];

	Pwr= sqrt( PwrPtr[x1][y1] * PwrPtr[x2][y2] * PwrPtr[x3][y3]);

	Ph = PhaPtr[x1][midy+y1];	Pha = phs1>0 ? Ph:-Ph;
	Ph = PhaPtr[x2][midy+y2];	Pha+= phs2>0 ? Ph:-Ph;
	Ph = PhaPtr[x3][midy+y3];	Pha-= phs3>0 ? Ph:-Ph;

	BiReal += Pwr * cos(Pha);
	BiImag += Pwr * sin(Pha);
  }
  return sqrt(BiReal*BiReal + BiImag*BiImag);
}

/*************************************************************************/
/**		Compute Visibility from Bispectrum			**/
/*************************************************************************/

static void BispVisOfPnt( struct Point *Pt)
{
  /** check if pnt is on frame, i.e.  0 <= sizexy/2 + xy < sizexy  **/

  if(	(Pt->x < -(sizex/2)) || (Pt->x >= sizex/2) ||
	(Pt->y < -(sizey/2)) || (Pt->y >= sizey/2))	return;

  Phase[OFF( Pt->x, Pt->y)]=
  Phase[OFF(-Pt->x,-Pt->y)]=
	sqrt(	BiModul( ObjPwr,ObjPha,ObjFrames, 0,0, Pt->x,Pt->y) /
		BiModul( RefPwr,RefPha,ObjFrames, 0,0, Pt->x,Pt->y) );

  Phase[OFF( Pt->x,-Pt->y)]=
  Phase[OFF(-Pt->x, Pt->y)]=
	sqrt(	BiModul( ObjPwr,ObjPha,ObjFrames, 0,0,-Pt->x,Pt->y) /
		BiModul( RefPwr,RefPha,ObjFrames, 0,0,-Pt->x,Pt->y) );

  if( verbose>2)	printf(" %g\n",Phase[OFF( Pt->x, Pt->y)]);
}


/*************************************************************************/
/**		Compute Bispectrums-phase of one point			**/
/*************************************************************************/

static
double BiPhase1( float ***PwrCube, float ***PhaCube, long nframes,
		 int x1,int y1,short phs1, int x2,int y2,short phs2, int x3,int y3,short phs3)
{
	float **PwrPtr, **PhaPtr;
	double Pwr, Pha,Ph, BiReal, BiImag;
	long fr;

  BiReal= BiImag= 0.0;
  for( fr=0;  fr < nframes;  ++fr)
  {
	PwrPtr= PwrCube[fr];	PhaPtr= PhaCube[fr];

	Pwr= sqrt( PwrPtr[x1][y1] * PwrPtr[x2][y2] * PwrPtr[x3][y3]);

	Ph = PhaPtr[x1][midy+y1];	Pha = phs1>0 ? Ph:-Ph;
	Ph = PhaPtr[x2][midy+y2];	Pha+= phs2>0 ? Ph:-Ph;
	Ph = PhaPtr[x3][midy+y3];	Pha-= phs3>0 ? Ph:-Ph;

	BiReal += Pwr * cos(Pha);
	BiImag += Pwr * sin(Pha);
  }
  return atan2(BiImag,BiReal);	/** atan2 on solaris handles the singularity itself **/
}


double BiPhase( int x1, int y1, int x2, int y2)
{
	short phs1,phs2,phs3;	/** Phase Signs **/
	int x3,y3;

  /***** We read only the right half of the frames, i.e. x>=0 *****/

  phs1= phs2= phs3= 1;	x3= x1+x2;  y3= y1+y2;

  if( x1<0) {	x1= -x1;  y1= -y1;  phs1= -1;	}
  if( x2<0) {	x2= -x2;  y2= -y2;  phs2= -1;	}
  if( x3<0) {	x3= -x3;  y3= -y3;  phs3= -1;	}

  return( BiPhase1( ObjPwr,ObjPha,ObjFrames, x1,y1,phs1, x2,y2,phs2, x3,y3,phs3 )
	 -BiPhase1( RefPwr,RefPha,RefFrames, x1,y1,phs1, x2,y2,phs2, x3,y3,phs3 ));
}


/*************************************************************************/
/**		Compute Phase of one Point from Bispectrum		**/
/*************************************************************************/

/* zur Fehlerrechnung:
 * Real = Sum(cos)	Imag = Sum(sin)
 * R := 1/N * Real	I := 1/N * Imag
 * dR^2	   = 1/(N(N-1)) * ( Sum(cos^2) - (Sum(cos))^2/N )
 * dReal^2 = N^2*(dR^2) = N/(N-1) * ( Sum(cos^2) - (Sum(cos))^2/N )
 *			= 1/(N-1) * ( N*Sum(cos^2) - (Sum(cos))^2 )
 *			= 1/(N-1) * ( N * sRea - Real^2 )	- s.u.
 *
 * sIma = Sum(sin^2) = Sum(1-cos^2) = N - sRea,
 * also Eigentlich koennte man sich's sparen, sIma mitzuschleppen.
 * Das sollte mal jemand nachrechnen...
 *
 * Ph	= atan(I/R) = atan(Imag/Real)
 * dPh	= 1/(R^2+I^2) * sqrt( (R*dR)^2 + (I*dI)^2 )
 *	= 1/(Real^2+Imag^2) * sqrt( (Real*dReal)^2 + (Imag*dImag)^2 )
 */

static double DeltaPhase( double Real, double Imag, double sRea, double sIma, double Cnt)
{
	double dRQ, dIQ, ReQ, ImQ;

  if( Cnt > 1.0)
  {
	ReQ= Real * Real;
	ImQ= Imag * Imag;
	dRQ= (Cnt * sRea - ReQ) / (Cnt-1.0);
	dIQ= (Cnt * sIma - ImQ) / (Cnt-1.0);
	return sqrt( ReQ*dRQ + ImQ*dIQ ) / (ReQ + ImQ);
  }
  else	return 3e-3;	/** ca. PI/1000 **/
}

static void BispPhaseOfPnt( struct Point *Pt)
{
	double Real1,Imag1, Real2,Imag2, hlp;
	double sRea1,sIma1, sRea2,sIma2, Phas;
	long off1, off2, cnt;
	int x1,y1, x2,y2, rq1;

  /** check if pnt is on frame, i.e.  0 <= sizexy/2 + xy < sizexy  **/

  if(	(Pt->x < -(sizex/2)) || (Pt->x >= sizex/2) ||
	(Pt->y < -(sizey/2)) || (Pt->y >= sizey/2))	return;

  /***** sum over subpoints *****/

  Real1= Imag1= Real2= Imag2= sRea1= sIma1= sRea2= sIma2= 0.0;	cnt= 0;

  for( x1= Pt->x-subdist+1;  x1<Pt->x+subdist && x1<sizex/2;  ++x1)
  {  x2= Pt->x - x1;

     for( y1= Pt->y-subdist+1;  y1<Pt->y+subdist && y1<sizey/2;  ++y1)
     {	y2= Pt->y - y1;

	rq1= x1*x1 + y1*y1;
	if( (Pt->rq <= rq1) || ((x2*x2 + y2*y2) >= rq1))  continue;

	Phas= Phase[ OFF(x1,y1)] + Phase[ OFF(x2,y2)] - BiPhase(x1,y1,x2,y2);
	hlp= cos(Phas);		Real1 += hlp;	sRea1 += hlp*hlp;
	hlp= sin(Phas);		Imag1 += hlp;	sIma1 += hlp*hlp;

	Phas= Phase[ OFF(x1,-y1)] + Phase[ OFF(x2,-y2)] - BiPhase(x1,-y1,x2,-y2);
	hlp= cos(Phas);		Real2 += hlp;	sRea2 += hlp*hlp;
	hlp= sin(Phas);		Imag2 += hlp;	sIma2 += hlp*hlp;

	cnt++;
     }
  }
  off1= OFF( Pt->x,Pt->y);	off2= OFF(-Pt->x,-Pt->y);
  Phas= atan2(Imag1,Real1);
  Phase[ off1] = Phas;
  SmoothPhasePoint( Pt->x,Pt->y,Pt->rq, 0,0);	/** only inner points, average **/
  Phase[ off2] = -Phase[off1];		/** Smooth changes it! **/
  dPhase[off1] = dPhase[off2]= DeltaPhase( Real1, Imag1, sRea1, sIma1, (double)cnt);

  off1= OFF( Pt->x,-Pt->y);	off2= OFF(-Pt->x, Pt->y);
  Phas= atan2(Imag2,Real2);
  Phase[ off1] = Phas;
  SmoothPhasePoint( Pt->x,-Pt->y,Pt->rq, 0,0);	/** only inner points, average **/
  Phase[ off2] = -Phase[off1];
  dPhase[off1] = dPhase[off2]= DeltaPhase( Real2, Imag2, sRea2, sIma2, (double)cnt);
}

/*************************************************************************/
/**		Compute Phase of one Point with Knox-Thompson		**/
/*************************************************************************/

static
double KT_PhasDiff( float ***PwrCube, float ***PhaCube,
			long nframes, int x, int y, int x1, int y1)
{
	double Real, Imag, Pwr, Pha, PhS=1.0, PhS1=1.0;
	long fr;

  Real= Imag= 0.0;
  if( x <0) {	x = -x;	 y = -y;  PhS = -1.0;	}
  if( x1<0) {	x1= -x1; y1= -y1; PhS1= -1.0;	}

  for( fr=0;  fr < nframes;  ++fr)
  {
	Pwr= sqrt( PwrCube[fr][x][midy+y] * PwrCube[fr][x1][midy+y1]);
	Pha=   PhS*PhaCube[fr][x][midy+y] - PhaCube[fr][x1][midy+y1]*PhS1;

	Real += Pwr * cos(Pha);
	Imag += Pwr * sin(Pha);
  }
  return atan2(Imag,Real);
}

static void KT_PhaseOfPnt( float ***Pwr, float ***Pha, long Frames, struct Point *Pt)
{
	double Real1, Imag1, sRea1, sIma1, Phas, hlp;
	double Real2, Imag2, sRea2, sIma2;
	long off1, off2, cnt;
	int x1,y1,rq1;

  /** check if pnt is on frame, i.e.  0 <= sizexy/2 + xy < sizexy  **/

  if(	(Pt->x < -(sizex/2)) || (Pt->x >= sizex/2) ||
	(Pt->y < -(sizey/2)) || (Pt->y >= sizey/2))	return;

  /***** sum over subpoints *****/

  Real1= Imag1= sRea1= sIma1= Real2= Imag2= sRea2= sIma2= 0.0;
  cnt= 0;

  for( x1= Pt->x - 2;  x1 <= Pt->x + 2 && x1<sizex/2;  ++x1)
  {
     for( y1= Pt->y - 2;  y1 <= Pt->y + 2 && y1<sizey/2;  ++y1)
     {
	rq1= x1*x1 + y1*y1;
	if( Pt->rq <= rq1)	continue;

	off1= OFF(x1,y1);
	Phas= Phase[off1] + KT_PhasDiff( Pwr,Pha,Frames, Pt->x,Pt->y, x1,y1);
	hlp= cos(Phas);		Real1 += hlp;	sRea1 += hlp*hlp;
	hlp= sin(Phas);		Imag1 += hlp;	sIma1 += hlp*hlp;

	off1= OFF(-x1,y1);
	Phas= Phase[off1] + KT_PhasDiff( Pwr,Pha,Frames,-Pt->x,Pt->y,-x1,y1);
        hlp= cos(Phas); 	Real2 += hlp;	sRea2 += hlp*hlp;
        hlp= sin(Phas); 	Imag2 += hlp;   sIma2 += hlp*hlp;

	cnt++;
     }
  }
  hlp= (double)cnt;

  off1= OFF( Pt->x,Pt->y);	off2= OFF(-Pt->x,-Pt->y);
  Phase[off1] = atan2( Imag1,Real1);
  SmoothPhasePoint( Pt->x,Pt->y,Pt->rq, 0,0);	/** only inner points, average **/
  Phase[off2] = -Phase[off1];		/** Smooth changes it! **/
  dPhase[off1] = dPhase[off2]= DeltaPhase( Real1, Imag1, sRea1, sIma1, hlp);

  off1= OFF(-Pt->x,Pt->y);	off2= OFF( Pt->x,-Pt->y);
  Phase[off1] = atan2( Imag2,Real2);
  SmoothPhasePoint(-Pt->x,Pt->y,Pt->rq, 0,0);	/** only inner points, average **/
  Phase[off2] = -Phase[off1];		/** Smooth changes it! **/
  dPhase[off1] = dPhase[off2]= DeltaPhase( Real2, Imag2, sRea2, sIma2, hlp);
}


/*************************************************************************/
/**				Center Phase				**/
/*************************************************************************/

static void CenterPhase( struct FitsFile *ff, float *Buffer)
{
	long x,y, xm,ym, xsize;
	float shift;

  xm= ff->n1 / 2;	ym= ff->n2 / 2;		xsize= ff->n1;

  shift= Buffer[ xm + ym * xsize];	/** offset at (0;0)? **/
  if( shift != 0.0)
  {	printf(" Subtracting %g from phase frame\n",shift);
	for( x=0;  x < ff->n1;  ++x)
	  for( y=0;  y < ff->n2;  ++y)
		Buffer[x + xsize * y] -= shift;
  }

  shift= (Buffer[ xm+1 + ym * xsize] - Buffer[ xm-1 + ym * xsize]) / 2.0;
  if( shift != 0.0)				/** offset at (-1:0) or (1;0) ? **/
  {	printf(" Subtracting u * %g from phase frame\n",shift);
	for( x=0;  x < ff->n1;  ++x)
	  for( y=0;  y < ff->n2;  ++y)
		Buffer[x + xsize * y] -= (float)(x-xm) * shift;
  }

  shift= (Buffer[ xm + (ym+1) * xsize] - Buffer[ xm + (ym-1) * xsize]) / 2.0;
  if( shift != 0.0)				/** offset at (-1:0) or (1;0) ? **/
  {	printf(" Subtracting v * %g from phase frame\n",shift);
	for( x=0;  x < ff->n1;  ++x)
	  for( y=0;  y < ff->n2;  ++y)
		Buffer[x + xsize * y] -= (float)(y-ym) * shift;
  }
}

/*************************************************************************/
/**				MAIN					**/
/*************************************************************************/

static void Help(char *prg)
{
 printf(" Usage: %s [-h]\n"
	"    or: %s [-v] [-r r] [-bid d] -obj file [file...] [-ref file [file...]]\n"
	"		[-bip phase-file] [-bis sigma-file]\n\n"
	" Where: -v		increases verbosity\n"
	"	-r r 		maximum radius where phase will be computed\n"
	"	-bid d		maximum distance of used subpoints [default 15]\n"
	"	-obj file...	gives the files containing the object\n"
	"	-ref file...	gives the files containing the reference\n"
	"	-bip file	result of the bispectrum-algorithm\n"
	"	-bis file	sigma of the result of the bisp.-alg.\n"
	"	-ktp file	result of the Knox-Thompson-algorithm\n"
	"	-kts file	sigma of the result of the KT-alg.\n"
	"\n", prg,prg);
}

int main( int argc, char **argv)
{
	char *biph_fname= NULL, *dbip_fname= NULL, *bivi_fname= NULL;
	char *KTph_fname= NULL, *dKTp_fname= NULL;
	char line[100];
	long rqmax=0, arg;
	int  res=20;
	FILE *hfp;

  printf("\ndespeckle, compiled on " __DATE__
				" ========================================\n\n");

  while( (hfp= fopen("/home/koehler/.hold", "r")))
  {	time_t time_now;
	struct tm *now;

	fclose(hfp);
	time(&time_now);	now= localtime(&time_now);
	if( now->tm_wday==0 || now->tm_wday == 6 	/** Sun/Saturday **/
	 || now->tm_hour< 9 || now->tm_hour >= 20 )	break;
	fprintf(stderr,
		" The time is %02d:%02d, that's not the right time for big background jobs!\n"
		" I'm going to sleep for an hour.\n\n",	now->tm_hour, now->tm_min);
	sleep(3600);
	nice(20);	/** maximize nice **/
  }
  OutFits.flags= 0L;	OutFits.filename= NULL;
  OutFits.fp= NULL;	OutFits.bitpix= -32;
  OutFits.naxis= 2;	OutFits.n3= 0;		/** n1, n2 will be set later **/
  OutFits.Header= NULL;	OutFits.HeaderSize= 0;

  for( arg=1;  arg<argc;  ++arg)
  {	char *str= argv[arg];

	 if( !strncmp(str,"-obj",4) && ++arg<argc)
	 {  if( !(arg= ReadCubes( &ObjPwr,&ObjPha,&ObjFrames, argv,argc,arg)))  return 20;
	    --arg;
	 }
    else if( !strncmp(str,"-ref",4) && ++arg<argc)
	 {  if( !(arg= ReadCubes( &RefPwr,&RefPha,&RefFrames, argv,argc,arg)))  return 20;
	    --arg;
	 }
    else if( !strncmp(str,"-bid",4) && ++arg<argc)  subdist= atoi(argv[arg]);
    else if( !strncmp(str,"-bip",4) && ++arg<argc)  biph_fname= argv[arg];
    else if( !strncmp(str,"-biv",4) && ++arg<argc)  bivi_fname= argv[arg];
    else if( !strncmp(str,"-bis",4) && ++arg<argc)  dbip_fname= argv[arg];
    else if( !strncmp(str,"-ktp",4) && ++arg<argc)  KTph_fname= argv[arg];
    else if( !strncmp(str,"-kts",4) && ++arg<argc)  dKTp_fname= argv[arg];
    else if( !strncmp(str,"-r",2) && ++arg<argc)    rqmax= atol(argv[arg]);
    else if( !strncmp(str,"-v",2))	++verbose;
    else if( !strncmp(str,"-h",2))  {	Help(argv[0]);	return 0; }
    else {	fprintf(stderr," Unknown option: %s\a\n\n",str);
		Help(argv[0]);		return 10;
	 }
  }
  if( PrepPointTable())  {  res= 18;	goto Exit;	}

  if( !(Phase= AllocBuffer(2*sizex*sizey*sizeof(float),"phase buffer")))
  {	res= 16;	goto Exit;	}
  dPhase= Phase + sizex*sizey;

  /******************** Prepare Header of output ********************/

  sprintf(line,"BITPIX  = %20d   / %-45s",OutFits.bitpix,"Data Type");
  FitsSetHeaderLine( &OutFits, "BITPIX  = ", line);

  sprintf(line,"NAXIS   = %20d   / %-45s",2,"number of axes");
  FitsSetHeaderLine( &OutFits, "NAXIS   = ", line);

  FitsSetHeaderLine( &OutFits, "NAXIS3  = ", NULL);	/** clear it **/

  if(rqmax==0)	rqmax= ((sizex > sizey) ? sizex : sizey)/2;	/** max radius **/
  printf(" maximum radius: %ld\n",rqmax);
  rqmax *= rqmax;	/** we need the square **/

  /******************** Knox-Thompson ******************************/

  if( KTph_fname)
  {	float *ObjPhase, *dObjPhase;
	char *histkeyw;

    printf(" computing Knox-Thompson phase of object...\n");
    histkeyw= FitsAddHistKeyw( &OutFits, "complex phase from Knox-Thompson");

    memset( Phase, 0, 2*sizex*sizey*sizeof(float));	/** clear entire Phase **/
    for( arg=0;  arg<Pnttblsz && Pnttbl[arg].rq<rqmax;  ++arg)
    {	if( verbose>2)	printf(" point #%ld: (%3d,%3d) = %d\n",arg,
					Pnttbl[arg].x,Pnttbl[arg].y,Pnttbl[arg].rq);
	if( verbose && (arg%15) == 0)
	{	if(verbose>1)	FitsWriteAny( KTph_fname, &OutFits, Phase,-32);
		printf(" point #%ld\r",arg);	 fflush(stdout);
	}
	if( Pnttbl[arg].rq > 0.0)  KT_PhaseOfPnt( ObjPwr,ObjPha,ObjFrames,&Pnttbl[arg]);
    }
    CenterPhase( &OutFits,Phase);
    FitsWriteAny( KTph_fname, &OutFits, Phase,-32);
    if( dKTp_fname)  FitsWriteAny( dKTp_fname, &OutFits, dPhase,-32);

    ObjPhase= Phase;	dObjPhase= dPhase;

    if( !(Phase= AllocBuffer(2*sizex*sizey*sizeof(float),"ref phase buffer")))
    {	res= 16;	goto Exit;	}
    dPhase= Phase + sizex*sizey;

    printf(" computing Knox-Thompson phase of reference...\n");
    memset( Phase, 0, 2*sizex*sizey*sizeof(float));	/** clear entire Phase **/
    for( arg=0;  arg<Pnttblsz && Pnttbl[arg].rq<rqmax;  ++arg)
    {	if( verbose>2)	printf(" point #%ld: (%3d,%3d) = %d\n",arg,
					Pnttbl[arg].x,Pnttbl[arg].y,Pnttbl[arg].rq);
	if( Pnttbl[arg].rq > 0.0)  KT_PhaseOfPnt( RefPwr,RefPha,RefFrames,&Pnttbl[arg]);
    }
    for( arg=0;  arg<sizex*sizey;  ++arg)
    {	 ObjPhase[arg] -=  Phase[arg];	dObjPhase[arg] += dPhase[arg];	}

    FitsWriteAny( KTph_fname, &OutFits, ObjPhase,-32);
    if( dKTp_fname)  FitsWriteAny( dKTp_fname, &OutFits, dObjPhase,-32);

    if(histkeyw)
    {	strcpy(histkeyw,"COMMENT");		/** clear history in header **/
	for( arg=7;  arg<80;  histkeyw[arg++]= ' ')	;
    }
  }
  fflush(stdout);

  /******************** Bispectrum *********************************/

  if( biph_fname)
  {
    printf(" computing Bispectrum phase...\n");
    FitsAddHistKeyw( &OutFits, "complex phase from bispectrum");

    memset( Phase, 0, 2*sizex*sizey*sizeof(float));	/** clear entire Phase **/
    for( arg=0;  arg<Pnttblsz && Pnttbl[arg].rq<rqmax;  ++arg)
    {	if( verbose>2)	printf(" point #%ld: (%3d,%3d) = %d\n",arg,
					Pnttbl[arg].x,Pnttbl[arg].y,Pnttbl[arg].rq);
	if( verbose && (arg%15) == 0)
	{	if(verbose>1)	FitsWriteAny( biph_fname, &OutFits, Phase,-32);
		printf(" point #%ld\r",arg);	 fflush(stdout);
	}
	if( Pnttbl[arg].rq > 1.0)  BispPhaseOfPnt( &Pnttbl[arg]);
    }
    FitsWriteAny( biph_fname, &OutFits, Phase,-32);
    if( dbip_fname)  FitsWriteAny( dbip_fname, &OutFits, dPhase,-32);
  }

  /******************** Visibility from Bispectrum ***********************/

  if( bivi_fname)
  {
    printf(" computing Bispectrum visibility...\n");
    FitsAddHistKeyw( &OutFits, "visibility from bispectrum");

    memset( Phase, 0, 2*sizex*sizey*sizeof(float));	/** clear entire Frame **/
    for( arg=0;  arg<Pnttblsz && Pnttbl[arg].rq<rqmax;  ++arg)
    {	if( verbose && (arg%15) == 0)
	{	if(verbose>1)	FitsWriteAny( bivi_fname, &OutFits, Phase,-32);
		printf(" point #%ld\r",arg);	 fflush(stdout);
	}
	if( verbose>2)	printf(" point #%ld: (%3d,%3d) => ",arg,
					Pnttbl[arg].x,Pnttbl[arg].y);
	BispVisOfPnt( &Pnttbl[arg]);
    }
    FitsWriteAny( bivi_fname, &OutFits, Phase,-32);
  }


  /********************** All done *********************************/

	res = 0;
Exit:
#ifdef PARANOID
	printf(" cleaning up...\n");
#endif
 	if( ObjPwr)	free(ObjPwr);
	if( ObjPha)	free(ObjPha);
	if( RefPwr)	free(RefPwr);
	if( RefPha)	free(RefPha);
	if( Pnttbl)	free(Pnttbl);
	if( Phase)	free(Phase);
	if( OutFits.Header)  free( OutFits.Header);

  return res;
}
