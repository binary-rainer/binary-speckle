/* $Id$
 * $Log$
 * maxbrwish.c
 * based on example in XTK-doc
 *
 * Created:     Mon Aug 11 15:49:02 1997 by Koehler@sun18
 * Last change: Tue Jul 16 19:21:48 2002
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <tk.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <unistd.h>
#ifdef HAVE_TKPGPLOT
# include "tkpgplot.h" /* Needed solely for tkpgplot_Init() */
#endif
#include "cpgplot.h"  /* Needed for cpg*() pgplot functions */
#include "fitsio.h"
#ifdef __sun
# include <ieeefp.h>	/** for finite() **/
#endif

#define PI	3.14159265359
#define ZWEIPI	6.28318530718

#define RAD(d)	((d) * (PI/180.0))
#define DEG(d)	((d) * (180.0/PI))

extern int Macro_TclWrappers( Tcl_Interp *pmtInterp );
extern int PGPLOT_TclWrappers( Tcl_Interp *pmtInterp );

extern void Projektion( struct FitsFile *ValFits, struct FitsFile *DValFits,
			double *v, double *dv, short *npp, double *psoll,
			double pangle, short rmin, short rmax, short stripw );

struct FitsFile *VisFits= NULL, *DVisFits= NULL;
struct FitsFile *KTFits = NULL, *DKTFits = NULL;
struct FitsFile *BiFits = NULL, *DBiFits = NULL;
struct FitsFile *MaskFits=NULL;

double *ProjData, *ProjDDat;
short  *NProj;
float  *pg_xval, *pg_yval, *pg_eval;


short naxis1=0, naxis2;

/* Prototype local functions. */

static int MB_AppInit(Tcl_Interp *interp);
static int usage_error(Tcl_Interp *interp, char *usage);
static int tcl_pgopen(	 ClientData data, Tcl_Interp *i, int argc, char *argv[]);
static int tcl_pgclose(	 ClientData data, Tcl_Interp *i, int argc, char *argv[]);

/***************************** Read FitsFrame *********************************/

static short ReadFrame(char *FileName, struct FitsFile **ffp)
{
	struct FitsFile *ff;
	long size;

  if( *ffp)  {	FitsClose(*ffp);  *ffp= NULL;	}

  if( !FileName)	return 0;

  if( !(ff = FitsOpenR(FileName)))	return 1;

  if( naxis1==0)		/** first time we read a frame **/
  { size= (ff->n1 + ff->n2 + 2) * 2;
    if( !(ProjData= AllocBuffer(size * (2*sizeof(double)+sizeof(short)),"projected values")))
		return 3;
    ProjDDat= ProjData + size;
    NProj   = (short *)(ProjDDat + size);

    if( !(pg_xval= AllocBuffer(size * 3*sizeof(float),"plot arrays")))	return 3;
    pg_yval= pg_xval+size;
    pg_eval= pg_yval+size;
  }
  else if( (ff->n1 != naxis1) || (ff->n2 != naxis2))
  {	fprintf(stderr,
	  " I hate to bother you, but the frame sizes of %s and the other frames\n"
	  " do not match.  I hope it doesn't disturb you that I'm skipping it.\a\n",
	  FileName);
	FitsClose(ff);
	return 2;
  }
  if( !(ff->Buffer.d= AllocBuffer( ff->n1 * ff->n2 *
		    (ffp==&MaskFits? sizeof(short):sizeof(double)), "frame buffer")))
  {	FitsClose(ff);	return 3;	}

  if( ffp != &MaskFits)
  { if( FitsReadDoubleBuf( ff, ff->Buffer.d, ff->n1 * ff->n2) < ff->n1 * ff->n2)
    {	fprintf(stderr," Not enough data in %s!\a\n",FileName);
	FitsClose(ff);	return 4;
    }
  }
  else	/** read Mask into 16bit-int-buffer **/
  {	double pxl;
	short zeroflg=0;
	long i;
	char line[99];

    for( i=0;  i < ff->n1 * ff->n2;  ++i)
    {	pxl= FitsReadDouble(ff);
	if((ff->Buffer.s[i]= (pxl < -1.0 || pxl > 1.0) ? 1756 : 0)==0)	zeroflg=1;
    }
    if( !zeroflg)
    {	fprintf(stderr,
		"\n %s has no zero pixels, i.e. it masks the WHOLE frame\a\n"
		  " I don't think it's a good idea to use it as a mask\a\n"
		  " Hit return to continue...",	FileName);
	fgets(line,98,stdin);
    }
  }
  *ffp= ff;	naxis1= ff->n1;	 naxis2= ff->n2;
  return 0;
}

/****************************** TCL-Interface ******************************/

static int tcl_loadfits(ClientData data, Tcl_Interp *interp, int argc, char *argv[])
{
	struct FitsFile **ffp;
	/*char	result[42];*/
	short	res;

  if(argc != 3)	 return usage_error(interp, "loadfile what filename");

	if( !strncasecmp(argv[1],"Vi",2))  ffp= &VisFits;
  else	if( !strncasecmp(argv[1],"KT",2))  ffp= &KTFits;
  else	if( !strncasecmp(argv[1],"Bi",2))  ffp= &BiFits;
  else	if( !strncasecmp(argv[1],"DVi",3))  ffp= &DVisFits;
  else	if( !strncasecmp(argv[1],"DKT",3))  ffp= &DKTFits;
  else	if( !strncasecmp(argv[1],"DBi",3))  ffp= &DBiFits;
  else	if( !strncasecmp(argv[1],"Mask",4)) ffp= &MaskFits;
  else
  {	Tcl_AppendResult(interp, "Unknown data type: ", argv[1], NULL);
	return TCL_ERROR;
  }
  if( !argv[2][0])
  {	Tcl_AppendResult(interp,"No filename given",NULL);
	return TCL_OK;
  }
  if((res= ReadFrame(argv[2],ffp)))
  {	/*sprintf(result,"ReadFrame returned %d!\n",res);*/
	return TCL_ERROR;
  }
  return TCL_OK;
}

/*****************************************************************************
 * Subroutine to compute maxbright from phase
 *****************************************************************************/

static void Phase2Maxbr(struct FitsFile *ff, struct FitsFile *dff, float *maxp,
			double pa, int rmin, int rmax)
{
	double minP, v;
	long mbr,r,k;

  Projektion(ff,dff, ProjData,ProjDDat,NProj,NULL, RAD(pa),rmin,rmax,0);

  for( r=0;  r<=2*rmax+1;  ++r)		/** r ist der Radius im Fourierraum! **/
    if( (NProj[r] > 1) && (r != rmax+1))
    {	if((mbr= r - (rmax+1)) < 0)	mbr= -mbr;
	minP= FLT_MAX;
	/** wir suchen das Minimum bzgl der Stufen, d.h. jeweils die engste **/
	/** Grenze, aber das Maximum bzgl. der Winkel, d.h. den hellsten    **/
	/** moegl. Begleiter						   **/
	/** loop over phase-steps, end if not enough points for projection **/

	for( k=1;  ; ++k)
	{	long roffs= (rmax+1) + k * (r-(rmax+1));

	  if( roffs<0 || (roffs>2*rmax+1) || (NProj[roffs] <= 1))  break;
	  if((v = ProjData[roffs]) < 0)  v= -v;
	  v= (v + ProjDDat[roffs]) / (double)k;
	  if( minP > v)  minP= v;
	}
	if( maxp[mbr] < minP)  maxp[mbr]= minP;
    }
}

/*****************************************************************************
 * Subroutine to compute min(Vis) and max(Phases)
 *****************************************************************************/

static void FindMinVisMaxPha( Tcl_Interp *interp,
		float *minV, float *maxKT, float *maxBi,
		double pa_min, double pa_max, double pa_stp, int rmin, int rmax)
{
	char line[42];
	double pa;
	int i,j;

  /** gehe nacheinander alle Winkel durch, berechne  **/
  /** die Projektion und vergleiche Ergebnis mit minV */

  for( i=0;  i<=rmax;  ++i)  { minV[i]= FLT_MAX;  maxKT[i]= maxBi[i]= 0.0;  }

  for( pa= pa_min;  pa<=pa_max;  pa += pa_stp)
  { sprintf(line,"update_clock %3.0f",(pa-pa_max));
    Tcl_Eval(interp,line);

    if( VisFits)	/********** projiziere Visibility **********/
    {		double vis0, v;

	Projektion( VisFits,DVisFits, ProjData,ProjDDat,NProj,NULL, RAD(pa),rmin,rmax,0);
	vis0= ProjData[rmax+1];		/** val[Nullpunkt] wird gleich 1.0 **/
	/**fprintf(stderr,"%g deg: vis0 = %g\n",pa,vis0);**/
	for( i=0;  i<=2*rmax+1;  ++i)
	  if( NProj[i] > 1)		/** bei einem Punkt sind die Fehler zu gross **/
	  {	if((j= i - (rmax+1)) < 0)	j= -j;
		v= ProjData[i] / vis0;	/** normieren! **/
		if( v > 1.0)  v= 1.0;	/** kann ja wohl nicht sein! **/
		v -= ProjDDat[i] / vis0;
		if( v < 0.0)  v= 0.0;	/** val-dval >= 0.0 !	**/
		if( minV[j] > v)  minV[j]= v;
	  }
    }
    if( KTFits)	 Phase2Maxbr( KTFits, DKTFits, maxKT, pa,rmin,rmax);
    if( BiFits)	 Phase2Maxbr( BiFits, DBiFits, maxBi, pa,rmin,rmax);
  }
}


/*****************************************************************************
 * command to plot something.
 *****************************************************************************/

static void line_and_text( double xend, double yline, double ytext, char *text)
{
  pg_xval[0]= 0.7 * xend;	pg_xval[1]= 0.79 * xend;
  pg_yval[0]= pg_yval[1]= yline;
  cpgline(2,pg_xval,pg_yval);
  cpgtext(0.8 * xend, ytext, text);
}

static int tcl_plot(ClientData data, Tcl_Interp *interp, int argc, char *argv[])
{
	struct FitsFile *ff=NULL, *dff=NULL;
	float	*minV=NULL, *maxKT, *maxBi;
	double	ymin, ymax, pa_min, pa_max, pa_stp, rmind, rmaxd;
	double	pa;
	float	val, min,max;
	int	i,j, npnts, rmin,rmax;

  if(argc != 9)
	 return usage_error(interp, "plot what ymin ymax pa_min pa_max pa_step rmin rmax");

  if( !strncasecmp(argv[1],"vi",2)) { ff= VisFits;  dff= DVisFits; }
  else
  if( !strncasecmp(argv[1],"KT",2)) { ff= KTFits;  dff= DKTFits; }
  else
  if( !strncasecmp(argv[1],"Bi",2)) { ff= BiFits;  dff= DBiFits; }
  else
  if( strncasecmp(argv[1],"ma",2))	/** err if NOT maxbright **/
  {	Tcl_AppendResult(interp, "Don't know how to plot ",argv[1],NULL);
	return TCL_ERROR;
  }

  if( (argv[2][0] && Tcl_GetDouble(interp,argv[2], &ymin)==TCL_ERROR) ||
      (argv[3][0] && Tcl_GetDouble(interp,argv[3], &ymax)==TCL_ERROR))	return TCL_ERROR;

  if(	Tcl_GetDouble(interp, argv[4], &pa_min) == TCL_ERROR ||
	Tcl_GetDouble(interp, argv[5], &pa_max) == TCL_ERROR ||
	Tcl_GetDouble(interp, argv[6], &pa_stp) == TCL_ERROR ||
	Tcl_GetDouble(interp, argv[7], &rmind) == TCL_ERROR ||
	Tcl_GetDouble(interp, argv[8], &rmaxd) == TCL_ERROR)	return TCL_ERROR;

  rmin= (int)rmind;	rmax= (int) rmaxd;

  if( pa_min > pa_max)	return usage_error(interp,"pa_min larger than pa_max");
  if( pa_stp == 0.0)	return usage_error(interp,"pa_step is zero");

  min= FLT_MAX;	 max= -FLT_MAX;

  if( !ff)	/** plot MaxBright **/
  {
    if( !(minV= AllocBuffer( 3 * (rmax+2) * sizeof(float),"maximal brightnesses")))
    {	Tcl_AppendResult(interp, "Out of chips",NULL);
	return TCL_ERROR;
    }
    maxKT= minV + rmax+2;
    maxBi= maxKT+ rmax+2;
    FindMinVisMaxPha(interp,minV,maxKT,maxBi,pa_min,pa_max,pa_stp,rmin,rmax);

    for( i=0;  i<=rmax;  ++i)
    {	if( VisFits && (minV[i] < FLT_MAX))
	{ val= (1.0-minV[i])/(1.0+minV[i]);
	  if(max<val)  max= val;
	  if(min>val)  min= val;
	}
	if( KTFits && (maxKT[i] > 0.0))
	{ val= (maxKT[i] < ZWEIPI) ? (1.0 / (ZWEIPI/maxKT[i] - 1.0)) : 1.0;
	  if(max<val)  max= val;
	  if(min>val)  min= val;
	}
	if( BiFits && (maxBi[i] > 0.0))
	{ val= (maxBi[i] < ZWEIPI) ? (1.0 / (ZWEIPI/maxBi[i] - 1.0)) : 1.0;
	  if(max<val)  max= val;
	  if(min>val)  min= val;
	}
    }
    if( max>1.0)  max= 1.0;
  }
  else		/** not Maxbright - seach for min/max in frames **/
  if( argv[2][0]==0 || argv[3][0]==0)
  {	long x, y, midx= naxis1/2, midy= naxis2/2;

    if( (y=-rmax) < -midy)	y= -midy;
    for( ;  y<=rmax && y<midy;  ++y)
    { if( (x=-rmax) < -midx)	x= -midx;
      for( ;  x<=rmax && x<midx;  ++x)
      { i= x*x + y*y;
	if( i >= rmin*rmin && i <= rmax*rmax)
	{ val= ff->Buffer.d[ (x+midx) + naxis1*(y+midy)];
	  if(max<val)  max= val;
	  if(min>val)  min= val;
	}
      }
    }
    min -= 0.02;	max += 0.02;
  }
  if(argv[2][0])  min= ymin;
  if(argv[3][0])  max= ymax;

  npnts= 2*rmax+2;

  cpgpage();
  cpgsvp( 0.05, 0.98, 0.05, 0.98);	/** set viewport **/

  if(!ff)	/** plot MaxBright (real soon now!) **/
  { val= ceil(log10(naxis1/2.0));	/** Ende der X-achse **/
    cpgswin( 0.0, val, min, max);
    cpgbox("BCLNST", 0, 0, "BCNST", 0, 0);

    if( VisFits)
    {	for( i=1, j=0;  i<=rmax;  ++i)	/** 1/(r=0) gilt nicht **/
	  if( minV[i] < FLT_MAX)
	  {	pg_xval[j]= log10((float) naxis1/2.0 / (float)i);
		pg_yval[j]= (1.0-minV[i]) / (1.0+minV[i]);
		++j;
	  }	/** j ist jetzt die Anzahl der zu plotenden Punkte **/
	cpgline(j,pg_xval,pg_yval);
	line_and_text(val, 0.04*min + 0.96*max, 0.05*min+0.95*max,"from Visibility");
    }
    if( KTFits)
    {	for( i=1, j=0;  i<=rmax;  ++i)	/** 1/(r=0) gilt nicht **/
	  if( maxKT[i] > 0.0)
	  {	pg_xval[j]= log10((float) naxis1/(float)i);
		pg_yval[j]= (maxKT[i] < ZWEIPI) ? (1.0 / (ZWEIPI/maxKT[i] - 1.0)) : 1.0;
		++j;
	  }	/** j ist jetzt die Anzahl der zu plotenden Punkte **/
	cpgsls(2);
	cpgline(j,pg_xval,pg_yval);
	line_and_text(val, 0.09*min + 0.91*max, 0.10*min+0.90*max,"from KT-Phase");
    }
    if( BiFits)
    {	for( i=1, j=0;  i<=rmax;  ++i)	/** 1/(r=0) gilt nicht **/
	  if( maxBi[i] > 0.0)
	  {	pg_xval[j]= log10((float) naxis1/(float)i);
		pg_yval[j]= (maxBi[i] < ZWEIPI) ? (1.0 / (ZWEIPI/maxBi[i] - 1.0)) : 1.0;
		++j;
	  }	/** j ist jetzt die Anzahl der zu plotenden Punkte **/
	cpgsls(3);
	cpgline(j,pg_xval,pg_yval);
	line_and_text(val, 0.14*min + 0.86*max, 0.15*min+0.85*max,"from Bisp-Phase");
    }
    cpgsls(1);
    free(minV);
  }
  else
  { cpgswin( 0.0, rmax+2, min, max);
    cpgbox("BCNST", 0, 0, "BCNST", 0, 0);

    for( pa= pa_min;  pa<=pa_max;  pa += pa_stp)
    {
	Projektion(ff,dff,ProjData,ProjDDat,NProj,NULL, RAD(pa),rmin,rmax,0);

	for( i=rmax+1, j=0;  i<npnts;  ++i)
	  if( NProj[i] > 1)
	  {	pg_xval[j]= (float)(i-(rmax+1));
		pg_yval[j]= (float)ProjData[i];
		pg_eval[j]= (float)ProjDDat[i];
		++j;
	  }	/** j ist jetzt die Anzahl der zu plotenden Punkte **/
	cpgline(j,pg_xval,pg_yval);
	cpgerrb(6,j,pg_xval,pg_yval,pg_eval,1.0);

	for( i=rmax, j=1;  i>0;  --i)
	  if( NProj[i] > 1)
	  {	pg_xval[j]= (float)((rmax+1)-i);
		pg_yval[j]= (float)ProjData[i];
		pg_eval[j]= (float)ProjDDat[i];
		++j;
	  }	/** j ist jetzt die Anzahl der zu plotenden Punkte **/
	cpgline(j,pg_xval,pg_yval);
	cpgerrb(6,j,pg_xval,pg_yval,pg_eval,1.0);
    }
  }
  return TCL_OK;
}


/********************************************************************************
 * command to compute and return maxbright
 ********************************************************************************/

#define FROM_VIS 0
#define FROM_PHA 1

static int tcl_maxbright(ClientData data, Tcl_Interp *interp, int argc, char *argv[])
{
	float	*minV=NULL, *maxKT, *maxBi;
	double	pa_min, pa_max, pa_stp, rmind, rmaxd;
	int	i, rmin,rmax;
	short	from_what;
	char	line[88];

  if(argc != 7)
	 return usage_error(interp, "maxbright from_what pa_min pa_max pa_step rmin rmax");

	if( !strncasecmp(argv[1],"vi",2)) { from_what= FROM_VIS; }
   else if( !strncasecmp(argv[1],"ph",2)) { from_what= FROM_PHA; }
   else	{ Tcl_AppendResult(interp, "Don't know what to do with ",argv[1],NULL);
	  return TCL_ERROR;
	}

  if(	Tcl_GetDouble(interp, argv[2], &pa_min) == TCL_ERROR ||
	Tcl_GetDouble(interp, argv[3], &pa_max) == TCL_ERROR ||
	Tcl_GetDouble(interp, argv[4], &pa_stp) == TCL_ERROR ||
	Tcl_GetDouble(interp, argv[5], &rmind) == TCL_ERROR ||
	Tcl_GetDouble(interp, argv[6], &rmaxd) == TCL_ERROR)	return TCL_ERROR;

  rmin= (int)rmind;	rmax= (int) rmaxd;

  if( pa_min > pa_max)	return usage_error(interp,"pa_min larger than pa_max");
  if( pa_stp == 0.0)	return usage_error(interp,"pa_step is zero");

  if( !(minV= AllocBuffer( 3 * (rmax+2) * sizeof(float),"maximal brightnesses")))
  {	Tcl_AppendResult(interp, "Out of chips",NULL);
	return TCL_ERROR;
  }
  maxKT= minV + rmax+2;
  maxBi= maxKT+ rmax+2;
  FindMinVisMaxPha(interp,minV,maxKT,maxBi,pa_min,pa_max,pa_stp,rmin,rmax);

  for( i=rmax;  i>0;  --i)	/** 1/(r=0) gilt nicht **/
  { line[0]= 0;
    switch(from_what)
    {
	case FROM_VIS:	if( minV[i] < FLT_MAX)
			    sprintf(line,"%f %f",(float) naxis1/2.0 / (float)i,
							(1.0-minV[i]) / (1.0+minV[i]));
			break;
	case FROM_PHA:	if( maxKT[i] > 0.0)
			    sprintf(line,"%f %f %f",(float) naxis1/(float)i,
				(maxKT[i]<ZWEIPI) ? (1.0 / (ZWEIPI/maxKT[i]-1.0)) : 1.0,
				(maxBi[i]<ZWEIPI) ? (1.0 / (ZWEIPI/maxBi[i]-1.0)) : 1.0);
			break;
    }
    if(line[0])	 Tcl_AppendElement(interp,line);
  }
  free(minV);
  return TCL_OK;
}

/********************************************************************************
 * command to center phases
 ********************************************************************************/

static int tcl_center_phase(ClientData data, Tcl_Interp *interp, int argc, char *argv[])
{
	struct FitsFile *ff=NULL, *dff=NULL;
	char line[80], *was;
	long x,y, xm,ym, xsize;
	double shift;

  if(argc != 2)  return usage_error(interp, "center_phase KT|Bi");

  if( !strncasecmp(argv[1],"KT",2)) { ff= KTFits;  dff= DKTFits;  was= "KT";   }
  else
  if( !strncasecmp(argv[1],"Bi",2)) { ff= BiFits;  dff= DBiFits;  was= "Bisp"; }
  else
	return usage_error(interp, "center_phase KT|Bi");

  xm= ff->n1 / 2;	ym= ff->n2 / 2;		xsize= ff->n1;

  shift= ff->Buffer.d[ xm + ym * xsize];	/** offset at (0;0)? **/
  if( shift < -1e-6 || shift > 1e-6)
  {	sprintf(line,"Subtracting %g from %s-phase frame\n",shift,was);
	Tcl_AppendResult(interp,line,NULL);

	for( x=0;  x < ff->n1;  ++x)
	  for( y=0;  y < ff->n2;  ++y)
		ff->Buffer.d[x + xsize * y] -= shift;
  }

  shift= (ff->Buffer.d[ xm+1 + ym * xsize] - ff->Buffer.d[ xm-1 + ym * xsize]) / 2.0;
  if( shift < -1e-6 || shift > 1e-6)		/** offset at (-1:0) or (1;0) ? **/
  {	sprintf(line,"Subtracting u * %g from %s-phase frame\n",shift,was);
	Tcl_AppendResult(interp,line,NULL);

	for( x=0;  x < ff->n1;  ++x)
	  for( y=0;  y < ff->n2;  ++y)
		ff->Buffer.d[x + xsize * y] -= (double)(x-xm) * shift;
  }

  shift= (ff->Buffer.d[ xm + (ym+1) * xsize] - ff->Buffer.d[ xm + (ym-1) * xsize]) / 2.0;
  if( shift < -1e-6 || shift > 1e-6)		/** offset at (-1:0) or (1;0) ? **/
  {	sprintf(line,"Subtracting v * %g from %s-phase frame\n",shift,was);
	Tcl_AppendResult(interp,line,NULL);

	for( x=0;  x < ff->n1;  ++x)
	  for( y=0;  y < ff->n2;  ++y)
		ff->Buffer.d[x + xsize * y] -= (double)(y-ym) * shift;
  }
  return TCL_OK;
}

/********************************************************************************
 * command to check centering of phases
 ********************************************************************************/

static int tcl_check_center(ClientData data, Tcl_Interp *interp, int argc, char *argv[])
{
	struct FitsFile *ff=NULL, *dff=NULL;
	char line[80], *was;
	long xm,ym, xsize;
	double shift;

  if(argc != 2)  return usage_error(interp, "check_center KT|Bi");

  if( !strncasecmp(argv[1],"KT",2)) { ff= KTFits;  dff= DKTFits;  was= "KT";  }
  else
  if( !strncasecmp(argv[1],"Bi",2)) { ff= BiFits;  dff= DBiFits;  was= "Bisp"; }
  else
	return usage_error(interp, "check_center KT|Bi");

  xm= ff->n1 / 2;	ym= ff->n2 / 2;		xsize= ff->n1;

  shift= fabs(ff->Buffer.d[ xm + ym * xsize]);	/** offset at (0;0)? **/
  if( shift > 1e-6)
  {	sprintf(line,"%s-phase at (0;0) is %g instead of zero.\n",was,shift);
	Tcl_AppendResult(interp,line,NULL);
  }
  shift= fabs(ff->Buffer.d[ xm+1 + ym * xsize] - ff->Buffer.d[ xm-1 + ym * xsize]);
  if( shift > 1e-6)				/** offset at (-1:0) or (1;0) ? **/
  {	sprintf(line,"Difference between %s-phase at (1;0) and (-1;0) is %g instead of zero\n",
			was,shift);
	Tcl_AppendResult(interp,line,NULL);
  }
  shift= fabs(ff->Buffer.d[ xm + (ym+1) * xsize] - ff->Buffer.d[ xm + (ym-1) * xsize]);
  if( shift > 1e-6)				/** offset at (-1:0) or (1;0) ? **/
  {	sprintf(line,"Difference between %s-phase at (0;1) and (0;-1) is %g instead of zero\n",
			was,shift);
	Tcl_AppendResult(interp,line,NULL);
  }
  return TCL_OK;
}

/********************************************************************************
 * command to write visibility as pgm-file
 * USAGE: vistopgm filename [rmax [min [max]]]
 *		rmax is used to limit the search for min/max values of the img
 ********************************************************************************/

void FindMinMaxRmax(struct FitsFile *ff, long rmax, double *minp, double *maxp)
{
	long	x,y, rmaxq= rmax*rmax;
	long	halfx= ff->n1/2, halfy= ff->n2/2;
	double	val;

  *minp= DBL_MAX;  *maxp= -DBL_MAX;
  for( y= -halfy;  y<halfy;  ++y)
  {  for( x= -halfx;  x<halfx;  ++x)
	if( x*x + y*y < rmaxq)
	{	val= ff->Buffer.d[x+halfx + ff->n1*(y+halfy)];
		if( finite(val))
		{	if( *minp > val)  *minp= val;
			if( *maxp < val)  *maxp= val;
		}
	}
  }
}

int Vis2PGM(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
	FILE	*fp;
	long	x, y, rmax= 9999;
	double	val,min,max;

  if(argc<2)	return TCL_ERROR;

  if( !(fp= fopen(argv[1],"w")))  return TCL_ERROR;

  if( argc>2 && *argv[2]) rmax= atoi(argv[2]);
  FindMinMaxRmax(VisFits,rmax,&min,&max);

  if( argc>3 && *argv[3]) min= atof(argv[3]);
  if( argc>4 && *argv[4]) max= atof(argv[4]);
  printf("min is %g, max is %g\n",min,max);

  fprintf(fp,"P5\n%d %d 255\n",VisFits->n1,VisFits->n2);

  for( y=VisFits->n2-1;  y>=0;  --y)
    for( x=0;  x<VisFits->n1;  ++x)
      {	val= VisFits->Buffer.d[x+VisFits->n1*y];
	if( val >= max) { fputc(255,fp); }
	else
	if( val <= min) { fputc(0,fp); }
	else
	  fputc((int)((val-min) / (max-min)*255),fp);
    }
  fclose(fp);
  return TCL_OK;
}

int Mask2PBM(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
	FILE	*fp;
	long	x, y;
	char	line[100];

  if(argc<2 || MaskFits==NULL)	return TCL_ERROR;

  sprintf(line,"pnmenlarge 2 | pbmtoxbm > %s",argv[1]);
  if( !(fp= popen(line,"w")))  return TCL_ERROR;

  fprintf(fp,"P1\n%d %d\n",MaskFits->n1,MaskFits->n2);

  for( y=MaskFits->n2-1;  y>=0;  --y)
  {
    for( x=0;  x<MaskFits->n1;  ++x)
    {	fputc('0'+(MaskFits->Buffer.s[x+MaskFits->n1*y] != 0), fp);
	fputc(' ',fp);
    }
    fputc('\n',fp);
  }
  pclose(fp);
  return TCL_OK;
}

int Mask_command(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
	char *MaskName;
	long framesz;

  if(argc<2)
  {	Tcl_AppendResult(tclipp, "Usage: ", argv[0], "create|clear|range", NULL);
	return TCL_ERROR;
  }
  if( MaskFits)  framesz= MaskFits->n1 * MaskFits->n2;

  if( 0==strncmp(argv[1],"cre",3))				/***** create *****/
  {	MaskName= (argc>2)? argv[2] : "maxbright-mask.fits";

	if( !(MaskFits= AllocBuffer(sizeof(struct FitsFile),"Mask FitsFile")))
		return TCL_ERROR;

	MaskFits->flags= 0;
	MaskFits->filename= MaskName;
	MaskFits->fp = NULL;
	MaskFits->bitpix= BITPIX_SHORT;
	MaskFits->naxis = 2;
	MaskFits->n1 = VisFits->n1;
	MaskFits->n2 = VisFits->n2;
	MaskFits->n3 = 1;
	strcpy(MaskFits->object,"badpixel mask");
	MaskFits->bscale = 1.;	MaskFits->bzero = 0.;

	MaskFits->HeaderSize= VisFits->HeaderSize;
	if( !(MaskFits->Header= AllocBuffer(MaskFits->HeaderSize,"Mask Header")))
	{	free(MaskFits);  MaskFits= NULL;  return TCL_ERROR;	}
	memcpy( MaskFits->Header, VisFits->Header, MaskFits->HeaderSize);

	framesz= MaskFits->n1 * MaskFits->n2;
	if( !(MaskFits->Buffer.s= AllocBuffer(framesz*sizeof(short),"Mask Buffer")))
	{	FitsClose(MaskFits);  MaskFits= NULL;  return TCL_ERROR;	}

	memset(MaskFits->Buffer.s, 0, framesz*sizeof(short));

	MaskFits->SA_hdrline= NULL;
	MaskFits->SA_xmin = VisFits->SA_xmin;  MaskFits->SA_xmax = VisFits->SA_xmax;
	MaskFits->SA_ymin = VisFits->SA_ymin;  MaskFits->SA_ymax = VisFits->SA_ymax;
  }
  else if( MaskFits==NULL)		/** all the following cmds need a mask **/
  {	Tcl_AppendResult(tclipp, "No mask defined", NULL);
	return TCL_ERROR;
  }
  else if( 0==strncmp(argv[1],"cle",3))				/***** clear *****/
  {	memset(MaskFits->Buffer.s, 0, framesz*sizeof(short));
  }
  else if( 0==strncmp(argv[1],"ran",3))				/***** range *****/
  {	long i, cnt=0;
	double min= -DBL_MAX, max= DBL_MAX;

    if(argc>2 && *argv[2]) min= atof(argv[2]);
    if(argc>3 && *argv[3]) max= atof(argv[3]);
    printf("excluding pixels < %g or > %g\n",min,max);

    for(i=0;  i<framesz;  ++i)
      if( VisFits->Buffer.d[i] < min || VisFits->Buffer.d[i] > max)
	{ MaskFits->Buffer.s[i] = 42;	cnt++;	}
    printf("Added %ld pixels\n",cnt);
  }
  else if( 0==strncmp(argv[1],"sav",3))				/***** save *****/
  {	char *fname= MaskFits->filename;

    if(argc>2 && *argv[2]) fname= argv[2];
    if(FitsWriteAny( fname, MaskFits, MaskFits->Buffer.s, MaskFits->bitpix))
	return TCL_ERROR;
  }
  return TCL_OK;
}

/********************************************************************************
 * command to get histogram of visibility
 * returns: min max step histgram-values
 ********************************************************************************/

int Histogram(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
	unsigned long *histogram;
	long	rmax= 9999;
	int	numbins=200, i, o, framesz= VisFits->n1 * VisFits->n2;
	double	min, max, step;
	char	line[42];

  if( argc>1 && *argv[1])  numbins= atoi(argv[1]);
  if( argc>2 && *argv[2])  rmax	  = atoi(argv[2]);

  if( !(histogram= malloc((numbins+4)*sizeof(unsigned long))))
  {	Tcl_AppendResult(tclipp,"Not enough memory for histogram",NULL);
	return TCL_ERROR;
  }
  for( i=0;  i<numbins;  ++i)	histogram[i]= 0;

  FindMinMaxRmax(VisFits,rmax,&min,&max);
  step= (max - min) / (double)(numbins-1);

  sprintf(line,"%g",min);	Tcl_AppendElement(tclipp,line);
  sprintf(line,"%g",max);	Tcl_AppendElement(tclipp,line);
  sprintf(line,"%g",step);	Tcl_AppendElement(tclipp,line);

  for( i=0;  i<framesz;  ++i)
    {	o= (int)((VisFits->Buffer.d[i] - min) / step);
	if( o >= 0 && o < numbins)  ++histogram[o];
  }
  for( i=0;  i<numbins;  ++i)
  {	sprintf(line,"%lu",histogram[i]);
	Tcl_AppendElement(tclipp,line);
  }
  free(histogram);
  return TCL_OK;
}

/********************************************************************************
 * command to tell who we are
 ********************************************************************************/

static int tcl_about(ClientData data, Tcl_Interp *interp, int argc, char *argv[])
{
  Tcl_AppendResult(interp,"Maxbright TNG by Rainer Köhler\n",
			  "$Revision$\n",
			  "compiled on " __DATE__, NULL);
  return TCL_OK;
}


/********************************* XTK-stuff *********************************/

int main(int argc, char *argv[])
{
  Tk_Main(argc, argv, MB_AppInit);

  /******** cleanup ********/
  if(VisFits )	FitsClose(VisFits );
  if(DVisFits)  FitsClose(DVisFits);
  if(KTFits  )  FitsClose(KTFits  );
  if(DKTFits )  FitsClose(DKTFits );
  if(BiFits  )  FitsClose(BiFits  );
  if(DBiFits )  FitsClose(DBiFits );
  if(MaskFits)  FitsClose(MaskFits);
  free(ProjData);
  free(pg_xval);

  return 0;
}

static int MB_AppInit(Tcl_Interp *interp)
{
  if(Tcl_Init(interp) == TCL_ERROR || Tk_Init(interp) == TCL_ERROR)
	return TCL_ERROR;

#ifdef HAVE_TKPGPLOT
  if(Tkpgplot_Init(interp) == TCL_ERROR)  return TCL_ERROR;
#endif

  Tcl_CreateCommand(interp, "about",	tcl_about,	NULL, 0);
  Tcl_CreateCommand(interp, "pgopen",	tcl_pgopen,	NULL, 0);
  Tcl_CreateCommand(interp, "pgclose",	tcl_pgclose,	NULL, 0);
  Tcl_CreateCommand(interp, "loadfits", tcl_loadfits,	NULL, 0);
  Tcl_CreateCommand(interp, "plot",	tcl_plot,	NULL, 0);
  Tcl_CreateCommand(interp, "maxbright", tcl_maxbright,	NULL, 0);
  Tcl_CreateCommand(interp, "check_center",tcl_check_center,NULL, 0);
  Tcl_CreateCommand(interp, "center_phase",tcl_center_phase,NULL, 0);
  Tcl_CreateCommand(interp, "vistopgm",  Vis2PGM,	NULL,NULL);
  Tcl_CreateCommand(interp, "masktopbm", Mask2PBM,	NULL,NULL);
  Tcl_CreateCommand(interp, "histogram", Histogram,	NULL,NULL);
  Tcl_CreateCommand(interp, "mask",	 Mask_command,	NULL,NULL);
  return TCL_OK;
}

/*
 * Implement the pgopen Tcl command. This takes a single
 * PGPLOT device specification argument and returns the
 * corresponding PGPLOT id for use with cpgslct().
 */
static int tcl_pgopen(ClientData data, Tcl_Interp *interp, int argc, char *argv[])
{
  char result[20];
  int id;
/*
 * Make sure that the right number of arguments have been provided.
 */
  if(argc != 2)
    return usage_error(interp, "pgopen device");

  id = cpgopen(argv[1]);  /* Attempt to open the PGPLOT device specified in argv[1]. */
  if(id <= 0) {
    Tcl_AppendResult(interp, "Unable to open device: ", argv[1], NULL);
    return TCL_ERROR;
  };
  cpgask(0);	/** Turn off new-page prompting. **/
  cpgscf(1);	/** set character font (1=normal)**/
/*
 * Return the PGPLOT id of the device via the Tcl result string.
 */
  sprintf(result, "%d", id);
  Tcl_AppendResult(interp, result, NULL);
  return TCL_OK;
}

/*
 * Implement the pgclose Tcl command.
 */
static int tcl_pgclose(ClientData data, Tcl_Interp *interp, int argc, char *argv[])
{
  cpgclos();
  return TCL_OK;
}

/*
 * The final function is just a utility function for reporting
 * command-usage errors and returning the standard Tcl error code.
 */
static int usage_error(Tcl_Interp *interp, char *usage)
{
  Tcl_AppendResult(interp, "Usage: ", usage, NULL);
  return TCL_ERROR;
}
