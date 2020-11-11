/* $Id: binplot.c,v 1.2 2002/05/12 08:12:19 koehler Exp koehler $
 *
 * Last Change: Fri Nov 23 12:42:08 2012
 *
 * $Log: binplot.c,v $
 * Revision 1.2  2002/05/12 08:12:19  koehler
 * Change tcl-C interaction to "official" method:
 * The C-program is an extended wish that gets the name of the
 * script as first argument.
 *
 * Revision 1.1  1997/08/02  11:37:47  koehler
 * Initial revision
 */

#include "binplot.h"
#include <memory.h>
//#include <malloc.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAVE_TKPGPLOT
# include "tkpgplot.h" /* Needed only for tkpgplot_Init() */
#endif

#define DEBUG 1

#ifdef DEBUG
# define debug(s)	fprintf(stderr,s)
#else
# define debug(s)	{}
#endif

struct FitsFile *VisFits= NULL, *PhaFits= NULL, *MaskFits=NULL;
struct FitsFile *DVisFits= NULL, *DPhaFits= NULL;
char *VisName=NULL, *PhaName=NULL, *Title=NULL, *MaskName=NULL;
char *DVisName=NULL, *DPhaName=NULL;

struct FitsFile VisModel, PhaModel;
	/** nur, um Buffer und sizex/sizey zusammen zu haben **/

double *ProjData, *ProjDDat, *ProjModl, *ProjDMod, *ProjSoll;
float *pg_xval, *pg_yval, *pg_eval, *pg_mval;
short *NProj;

short naxis1=0, naxis2;

int rmin=0, rmax=42, rfit=0, stripw=0;

float pangle=0.0, visfak=1.0;
double vismin= -DBL_MAX, vismax= DBL_MAX, phamin= -DBL_MAX, phamax= DBL_MAX;

/****************************** small subroutines *******************************/

char *FilenamePlus(char *templ)
{
	char *src, *end, *new;

  if( *templ=='+')
  {	     if( VisName)  src= VisName;
	else if( PhaName)  src= PhaName;
	else		   return(NULL);

	if( (end= strrchr(src,'_')))	*end=0;

	if( !(new= AllocBuffer(strlen(src)+strlen(templ),"filename")))	return NULL;
	strcpy(new,src);	strcat(new,templ+1);
	if( end)  *end= '_';
  }
  else	new= strdup(templ);
  return new;
}


double atoangle(char *str)
{
	double angle= atof(str);

  while( angle <    0.0)  angle += 360.0;
  while( angle >= 360.0)  angle -= 360.0;
  return angle;
}


char *getvisual(void)
{
	char *strp;

  if( !(strp= getenv("VISUAL")))
  {	fprintf(stderr,"VISUAL not set, using \"emacs\"\a\n");
	return "emacs";
  }
  else	return strp;
}


static void InitPGPlot(char *device, int ny)
{
  if( cpgbeg(0,device,1,ny) != 1)
  {	fprintf(stderr," Can't start pgplot!\a\n");	exit(42);	}

  cpgask(0);
  cpgscf(2);	/** set char font = roman **/
}

/************************ Plot or Print ****************************/

static void DoPlot(int color, float min, float max, char *titel, int npt)
{
  cpgbbuf();
  cpgeras();
  cpgsch(1.4);
  cpgsvp( 0.05,0.98, 0.07,0.92);
  cpgswin((float)-rmax-5, (float)rmax+5, min, max);
  cpgbox( "BCNST",0.0,0, "BCNST",0.0,0);

  cpgsch(2.0);	cpgmtxt("T", 0.4, 0.5, 0.5, titel);

  if(color)  cpgsci(2);	/** set color index -> 2=Red **/
  cpgpt(    npt,pg_xval,pg_yval,1);
  cpgerrb(6,npt,pg_xval,pg_yval,pg_eval,1.0);

  if(color)  cpgsci(3);	/** set color index -> 3=Green **/
  cpgline(npt,pg_xval,pg_mval);

  if(color)  cpgsci(1);	/** set color index -> 1=White=default **/
  cpgebuf();
}

static short DoPrint(char *fname, int npt)
{
	FILE *fp;
	long i;

  if( !(fp= fopen(fname,"w")))
  {	fprintf(stderr," Can't write to \"%s\"\a\n",fname);	return 1;	}

#ifdef DEBUG
  fprintf(stderr,"Writing %s\n",fname);
#endif

  fprintf(fp,"# Radius Data Error Model\n");

  for( i=0;  i<npt;  ++i)
    fprintf(fp,"%g %g %g %g\n", pg_xval[i], pg_yval[i], pg_eval[i], pg_mval[i]);

  fclose(fp);
  return 0;
}

static void PlotVisib(short color, short print)
{
	float val, min,max;
	int i,j, npnts= 2*rmax+2;

  Projektion( VisFits,DVisFits,ProjData,ProjDDat,NProj,NULL, RAD(pangle),rmin,rmax,stripw);
  Projektion( &VisModel,NULL,  ProjModl,ProjDMod,NProj,NULL, RAD(pangle),rmin,rmax,stripw);

  min= FLT_MAX;	 max= -FLT_MAX;

  for( i=j=0;  i<npnts;  ++i)
    if( NProj[i] > 1)
    {	pg_xval[j]= (float)(i-(rmax+1));

	val= (ProjData[i]+ProjDDat[i]) * visfak;	if(max<val)  max= val;
	val= (ProjData[i]-ProjDDat[i]) * visfak;	if(min>val)  min= val;
	pg_yval[j]= ProjData[i] * visfak;
	pg_eval[j]= ProjDDat[i] * visfak;
	pg_mval[j]= ProjModl[i];

	if( max<ProjModl[i] )	max= ProjModl[i];
	if( min>ProjModl[i] )	min= ProjModl[i];
	++j;
    }
  /** j ist jetzt die Anzahl der zu plotenden Punkte **/
  min -= 0.1;	if(vismin >-DBL_MAX)	min= vismin;
  max += 0.1;	if(vismax < DBL_MAX)	max= vismax;

  if(print)  DoPrint("binplot_vis",j);
	else DoPlot(color,min,max,"Visibility",j);
}


static void PlotPhase(short color, short print)
{
	float val, min,max;
	int i,j, npnts= 2*rmax+2;

  Projektion( PhaFits,DPhaFits,ProjData,ProjDDat,NProj,NULL, RAD(pangle),rmin,rmax,stripw);
  Projektion( &PhaModel,NULL,  ProjModl,ProjDMod,NProj,NULL, RAD(pangle),rmin,rmax,stripw);

  min= FLT_MAX;	 max= -FLT_MAX;

  for( i=j=0;  i<npnts;  ++i)
    if( NProj[i] > 1)
    {	pg_xval[j]= (float)(i-(rmax+1));

	val= ProjData[i];
/*
	while( val > PI)  val -= ZWEIPI;
	while( val <-PI)  val += ZWEIPI;
 */
	if( max < val+ProjDDat[i])  max= val+ProjDDat[i];
	if( min > val-ProjDDat[i])  min= val-ProjDDat[i];
	pg_yval[j]= val;
	pg_eval[j]= ProjDDat[i];

	val= ProjModl[i];
/*
	while( val > PI)  val -= ZWEIPI;
	while( val <-PI)  val += ZWEIPI;
 */
	if( max < val+ProjDDat[i])  max= val+ProjDDat[i];
	if( min > val-ProjDDat[i])  min= val-ProjDDat[i];
	pg_mval[j]= val;

	++j;
    }
  /** j ist jetzt die Anzahl der zu plotenden Punkte **/
  min -= 0.1;	if(phamin >-DBL_MAX)	min= phamin;
  max += 0.1;	if(phamax < DBL_MAX)	max= phamax;

  if(print)  DoPrint("binplot_pha",j);
	else DoPlot(color,min,max,"Phase",j);
}


void EstimSep_BRatio(void)
{
	float v, min= FLT_MAX, max= -FLT_MAX;
	int i,j, npnts= 2*rmax+2, minr;

  Projektion( VisFits,DVisFits,ProjData,ProjDDat,NProj,NULL, RAD(pangle),rmin,rmax,stripw);
  for( i=j=0;  i<npnts;  ++i)
    if( NProj[i] > 1)
    {	v= ProjData[i];
	pg_yval[j]= v;
	if( min>v)  {	min= v;	 minr= (rmax+1)-i; }
	if( max<v)	max= v;
	++j;
    }
  if(minr<0)	minr= -minr;
  Modell[DIST1] = (double)(naxis1/2) / (double)minr;
  Modell[BRGTN1]= (max-min) / (max+min);
}


/******************************************************************************/

void CalcErrors( double *errvis, double *errpha)
{
	double cnt, val, dval;
	long halfx= naxis1/2, halfy= naxis2/2, rad_quad= rmax*rmax;
	long x,y, xc,yc, i;

  *errvis= *errpha= cnt= 0.0;

  if( (x= halfx-rmax) < 0)	x=0;
  for( ;  x <= halfx+rmax && x < naxis1;  ++x)
  {	xc= x-halfx;

    if( (y= halfy-rmax) < 0)	y=0;
    for( ;  y <= halfy+rmax && y < naxis2;  ++y)
    {	yc= y-halfy;

      if( xc*xc + yc*yc <= rad_quad)
      {	i= x + naxis1 * y;
	if( !MaskFits || MaskFits->Buffer.s[i]==0)
	{
	  if( VisFits)
	  {	val= VisModel.Buffer.d[i] / visfak - VisFits->Buffer.d[i];
		if(DVisFits)	val /= DVisFits->Buffer.d[i];
		*errvis += val * val;
	  }
	  if( PhaFits)
 	  {	val= PhaModel.Buffer.d[i] - PhaFits->Buffer.d[i];
		while( val >  PI)  val -= ZWEIPI;
		while( val < -PI)  val += ZWEIPI;
		dval= DPhaFits? DPhaFits->Buffer.d[i] : 0.01;
		if(dval < 0.001)  dval= 0.001;
		val /= dval;
		*errpha += val * val;
	  }
	  cnt += 1.0;
	}
      }
    }
  }
  *errvis /= cnt;	*errpha /= cnt;

  SetDoubleVar("%.3g", *errvis, "errvis");
  SetDoubleVar("%.3g", *errpha, "errpha");
  SetDoubleVar("%.3g", *errvis + *errpha, "errtot");
}

/********************************** Create Hardcopy ***************************/

void Hardcopy(char *fname)
{
	FILE *texfp;
	char cmdline[256];
	long i;
	time_t now;

  if( VisFits)  { InitPGPlot("ProjVis.ps/PS",1);  PlotVisib(0,0);  cpgend();  }
  if( PhaFits)  { InitPGPlot("ProjPha.ps/PS",1);  PlotPhase(0,0);  cpgend();  }
  InitPGPlot(".plw.plot/XTK",2);
  now= time(NULL);

  if( !fname)	fname= "binplot";
  strcpy(cmdline,fname);	strcat(cmdline,".tex");

  if((texfp= fopen( cmdline,"w"))==NULL)
  {	fprintf(stderr," Can't open %s!\a\n",cmdline);	 return;	}

  fprintf(texfp,"%% created by binplot\n"
		"\\magnification=\\magstep1\\parindent0pt\n"
		"\\input psfig.sty\n\\pssilent\n"
		"\\voffset1cm\n"
		"\\font\\eightpt=cmr8\n\n"	);
  Title= Tcl_GetVar(tclip,"title",TCL_GLOBAL_ONLY);
  fprintf(texfp,"\\headline={\\centerline{\\bf %s}}\n",Title? Title:"");
  fprintf(texfp,"\\footline={\\centerline{%s}}\n\n", ctime(&now));

  if(VisFits)
	fprintf(texfp,"\\vbox{%%\n"
		"\\centerline{\\psfig{figure=ProjVis.ps,height=0.35\\vsize,angle=270}}\n"
		"\\centerline{\\eightpt Pixel}}\n\\vfill\n\n");
  if(PhaFits)
	fprintf(texfp,"\\vbox{%%\n"
		"\\centerline{\\psfig{figure=ProjPha.ps,height=0.35\\vsize,angle=270}}\n"
		"\\centerline{\\eightpt Pixel}}\n\\vfill\n\n");

  fprintf(texfp,"\\catcode`\\_=12\n\\halign{\\strut\\hfil# && #\\hfil\\cr\n");
  if( VisName)	fprintf(texfp,"       Visibility: & \\tt %s\\span\\span\\span\\cr\n", VisName);
  if(DVisName)	fprintf(texfp," Delta Visibility: & \\tt %s\\span\\span\\span\\cr\n",DVisName);
  if( PhaName)	fprintf(texfp,"            Phase: & \\tt %s\\span\\span\\span\\cr\n", PhaName);
  if(DPhaName)	fprintf(texfp,"      Delta Phase: & \\tt %s\\span\\span\\span\\cr\n",DPhaName);
  if(MaskName)	fprintf(texfp,"	   Badpixel-Mask: & \\tt %s\\span\\span\\span\\cr\n", MaskName);
  fprintf(texfp," Projection: & angle \\hfill$%.4f^\\circ$, & "
					"radius $%d\\ldots%d$ pixels\\span", pangle, rmin,rmax);
  if(stripw)	fprintf(texfp,", strip width %d pixels\\cr\n",stripw);
	else	fprintf(texfp,"\\cr\n");

  for( i=0;  i<N_comp;  ++i)
  {	fprintf(texfp," Companion %ld: & ", i+1);
	fprintf(texfp,	"angle \\hfill$%.4f^\\circ$, & ", Modell[ANGLE1+3*i]);
	fprintf(texfp,	"distance \\hfill$%.4f$&, "	, Modell[DIST1+3*i] );
	fprintf(texfp,	"brightness ratio \\hfill$%.5f$\\cr\n", Modell[BRGTN1+3*i] );
  }
  fprintf(texfp,"    Center: & angle \\hfill$%.4f^\\circ$, & distance \\hfill$%.4f$\\cr\n",
							Modell[CE_ANGLE], Modell[CE_DIST] );
  fprintf(texfp," Visib.~factor: & $%f$\\cr\n", visfak	);
  fprintf(texfp," Radius of fit: & $%d$\\cr\n", rfit	);
  fprintf(texfp,"}\\bye\n");
  fclose(texfp);

  sprintf(cmdline,"tex %s.tex\n",fname);
  if( system(cmdline))	fprintf(stderr,"\n Cannot call tex!\a\n");
  else
  { sprintf(cmdline,"dvips %s.dvi\n",fname);
    if( system(cmdline))  fprintf(stderr,"\n Cannot call dvips!\a\n");
    else
    { sprintf(cmdline,"\\rm %s.log %s.dvi ProjVis.ps ProjPha.ps; %s %s.ps\n",
			    fname, fname,		  GHOSTVIEW, fname);
      if( system(cmdline))  fprintf(stderr,"\n Cannot call ghostview!\a\n");
      printf("\n Hardcopy created in file %s.ps\n",fname);
    }
  }
}


void WriteDatafiles(void)
{
  if( VisFits)  { PlotVisib(0,1); }
  if( PhaFits)  { PlotPhase(0,1); }
}

/***************************** Save w/o. Companion *********************************/

int SaveCompSub(int argc, char **argv)
{
	struct FitsFile OutFits;
	double *CSVis, *CSPha;
	double	vis,pha, real,imag;
	long	i, framesz= naxis1*naxis2;

  if( !(CSVis= AllocBuffer(framesz * 2 * sizeof(double),"buffers")))	return 1;
  CSPha= CSVis + framesz;

  OutFits.flags= 0;	OutFits.fp= NULL;
  OutFits.bitpix= -32;	OutFits.naxis= 2;
  OutFits.n1= naxis1;	OutFits.n2= naxis2;	OutFits.n3= 1;
  strcpy( OutFits.object, VisFits->object);
  OutFits.bscale= 1.0;	OutFits.bzero= 0.0;
  OutFits.Header= VisFits->Header;
  OutFits.HeaderSize= VisFits->HeaderSize;

  FT_Modell( VisModel.Buffer.d, PhaModel.Buffer.d, naxis1,naxis2,naxis1+naxis2);
								/** radius > frame **/
  for( i=0;  i<framesz;  ++i)
  {
	vis= VisFits->Buffer.d[i]*visfak - VisModel.Buffer.d[i];
	if(PhaFits)
	{    pha= PhaFits->Buffer.d[i]	 - PhaModel.Buffer.d[i]; }
	else pha= 0.0;

	real= vis * cos(pha) + 1.0;	/** die 1.0 ist der neue Primary **/
	imag= vis * sin(pha);

	CSVis[i]= sqrt(real*real + imag*imag);
	CSPha[i]= atan2(imag,real);
  }
  /** now re-normalize visibility and delta vis**/
  {	long x,y, xm= naxis1/2, ym= naxis2/2;
	double shift;

    shift= CSVis[ xm + naxis1 * ym];	/** middle of frame = freq. 0,0 **/
    for( i=0;  i<framesz;  ++i)	 CSVis[i] /= shift;

    if( (shift= CSPha[ xm + ym*naxis1]) != 0.0)	/** offset at (0;0)? **/
	for( i=0;  i < framesz;  ++i)	CSPha[i] -= shift;

    shift= (CSPha[ xm+1 + ym * naxis1] - CSPha[ xm-1 + ym * naxis1]) / 2.0;
    if( shift != 0.0)				/** offset at (-1:0) or (1;0) ? **/
    {	for( x=0;  x < naxis1;  ++x)
	  for( y=0;  y < naxis2;  ++y)
		CSPha[x + naxis1 * y] -= (double)(x-xm) * shift;
    }
    shift= (CSPha[ xm + (ym+1) * naxis1] - CSPha[ xm + (ym-1) * naxis1]) / 2.0;
    if( shift != 0.0)				/** offset at (-1:0) or (1;0) ? **/
    {	for( x=0;  x < naxis1;  ++x)
	  for( y=0;  y < naxis2;  ++y)
		CSPha[x + naxis1 * y] -= (double)(y-ym) * shift;
    }
  }
  if(argc>1)  {	OutFits.filename= argv[1];  FitsWriteAny(argv[1],&OutFits,CSVis,-64); }
  if(argc>3)  {	OutFits.filename= argv[3];  FitsWriteAny(argv[3],&OutFits,CSPha,-64); }

  for( i=0;  i<framesz;  ++i)	CSVis[i]= DVisFits->Buffer.d[i] * visfak/real;

  if(argc>2)  {	OutFits.filename= argv[2];  FitsWriteAny(argv[2],&OutFits,CSVis,-64); }
  if(argc>4 && DPhaFits)
  {	OutFits.filename= argv[4];  FitsWriteAny(argv[4],&OutFits,DPhaFits->Buffer.d,-64); }

  free(CSVis);
  return 0;
}


/***************************** Save Residuals *********************************/

int SaveResiduals(int argc, char **argv)
{
	struct FitsFile OutFits;
	double *ResVis, *ResPha;
	long	i, framesz= naxis1*naxis2;

  if( !(ResVis= AllocBuffer(framesz * 2 * sizeof(double),"buffers")))	return 1;
  ResPha= ResVis + framesz;

  OutFits.flags= 0;	OutFits.fp= NULL;
  OutFits.bitpix= -32;	OutFits.naxis= 2;
  OutFits.n1= naxis1;	OutFits.n2= naxis2;	OutFits.n3= 1;
  strcpy( OutFits.object, VisFits->object);
  OutFits.bscale= 1.0;	OutFits.bzero= 0.0;
  OutFits.Header= VisFits->Header;
  OutFits.HeaderSize= VisFits->HeaderSize;

  FT_Modell( VisModel.Buffer.d, PhaModel.Buffer.d, naxis1,naxis2,naxis1+naxis2);
								/** radius > frame **/
  for( i=0;  i<framesz;  ++i)
  {
	ResVis[i]= VisFits->Buffer.d[i]*visfak - VisModel.Buffer.d[i];
	if(PhaFits)
	{ ResPha[i]= PhaFits->Buffer.d[i] - PhaModel.Buffer.d[i]; }
  }
  if(argc>1)  {	OutFits.filename= argv[1];  FitsWriteAny(argv[1],&OutFits,ResVis,-64); }
  if(argc>2 && PhaFits)
	{	OutFits.filename= argv[2];  FitsWriteAny(argv[2],&OutFits,ResPha,-64); }

  free(ResVis);
  return 0;
}


/***************************** Read FitsFrame *********************************/

short ReadFrame(char *FileName, struct FitsFile **ffp)
{
	struct FitsFile *ff;
	long size;

  if( *ffp)  {	FitsClose(*ffp);  *ffp= NULL;	}

  if( !FileName)	return 0;
  if( !(ff = FitsOpenR(FileName)))	return 1;
#ifdef DEBUG
  fprintf(stderr,"Reading frame %s\n",FileName);
#endif

  if( naxis1==0)		/** first time we read a frame **/
  { size= (ff->n1 + ff->n2 + 2) * 2;
    if( !(ProjData= AllocBuffer(size * (5*sizeof(double)+sizeof(short)),"projected values")))
		return 3;
    ProjDDat= ProjData + size;
    ProjModl= ProjDDat + size;
    ProjDMod= ProjModl + size;
    ProjSoll= ProjDMod + size;
    NProj = (short *)(ProjSoll + size);

    if( !(pg_xval= AllocBuffer(size * 4*sizeof(float),"plot arrays")))	return 3;
    pg_yval= pg_xval+size;
    pg_eval= pg_yval+size;
    pg_mval= pg_eval+size;

    VisModel.naxis= PhaModel.naxis= 2;
    VisModel.n1	  = PhaModel.n1   = ff->n1;
    VisModel.n2	  = PhaModel.n2   = ff->n2;

    size= ff->n1 * ff->n2;
    if( !(VisModel.Buffer.d= AllocBuffer(size * 2 * sizeof(double),"model")))	return 3;
    PhaModel.Buffer.d= VisModel.Buffer.d + size;
  }
  else if( (ff->n1 != naxis1) || (ff->n2 != naxis2))
  {	fprintf(stderr,
		" I hate to bother you, but the frame sizes of %s and the other frames\n"
		" do not match.  I hope it doesn't disturb you that I'm skipping it.\a\n",
		FileName);
	FitsClose(ff);
	return 2;
  }
  size= ff->n1 * ff->n2;
  if( !(ff->Buffer.d= AllocBuffer( size *
			(ffp==&MaskFits? sizeof(short):sizeof(double)), "frame buffer")))
  {	FitsClose(ff);	return 3;	}

  if( ffp != &MaskFits)
  { if( FitsReadDoubleBuf( ff, ff->Buffer.d, size) < size)
    {	fprintf(stderr," Not enough data in %s!\a\n",FileName);
	FitsClose(ff);	return 4;
    }
  }
  else	/** read Mask into 16bit-int-buffer **/
  {	double pxl;
	short zeroflg=0;
	long i;
	char line[100];

    for( i=0;  i < size;  ++i)
    {	pxl= FitsReadDouble(ff);
	if((ff->Buffer.s[i]= (pxl < -1.0 || pxl > 1.0) ? 1756 : 0)==0)	zeroflg=1;
    }
    if( !zeroflg)
    {	fprintf(stderr,"\n %s has no zero pixels, i.e. it masks the WHOLE frame\a\n"
			 " I don't think it's a good idea to use it as a mask\a\n"
			 " Hit return to continue...",	FileName);
	fgets(line,99,stdin);
    }
  }
  *ffp= ff;	naxis1= ff->n1;	 naxis2= ff->n2;
  return 0;
}

/******************************************************************************/

short ReadFitFile(char *fname)
{
	FILE *fp;
	char line[260], *str;
	long n=0;

  if( !fname)	return 1;
  if( !(fp= fopen(fname,"r")))
  {	fprintf(stderr," Can't open \"%s\"\a\n\n",fname);  return 2;	}

  printf("Reading %s...\n",fname);	fflush(stdout);

  while( !feof(fp) && !ferror(fp))
  {
#ifdef DEBUG
    printf("Read ");	fflush(stdout);
#endif

    if( !fgets(line,256,fp))	break;
    if( (str= strchr(line,'\n')))	*str= 0;

#ifdef DEBUG
    printf("\"%s\"\n",line);	fflush(stdout);
#endif

    if( !strncmp(line,"ANZAHL   :",10))
    {	if((N_comp= atol(line+10)) > MAX_COMP)	N_comp= MAX_COMP;	}
    else
    if( !strncmp(line,"BEGLEITER:",10) && (n<MAX_COMP))
    {	for( str= line+10;  *str && (*str==' ' || *str=='\t');  ++str)	;
	Modell[ANGLE1+3*n]= atoangle(str);
	if( n==0)  pangle= Modell[ANGLE1+3*n];
	for( str= strchr(str,' ');  str && *str && *str==' ';  ++str) ;
	Modell[DIST1 +3*n]= atof(str);
	for( str= strchr(str,' ');  str && *str && *str==' ';  ++str) ;
	Modell[BRGTN1+3*n]= atof(str);
	n++;
    }
    else
    if( !strncmp(line,"SCHWERPUN:",10))
    {	for( str= line+10;  *str && (*str==' ' || *str=='\t');  ++str)	;
	Modell[CE_ANGLE]= atoangle(str);
	for( str= strchr(str,' ');  str && *str && *str==' ';  ++str) ;
	Modell[CE_DIST]= atof(str);
    }
    else
    if( !strncmp(line,"VISIBFAKT:",10))	 visfak= atof(line+10);
    else
    if( !strncmp(line,"RADIUS   :",10))	 rfit= rmax= atol(line+10);
    else
    if( !strncmp(line,"TITLE    : ",11)) Title= strdup(line+11);
    else
    if( !strncmp(line,"VISIBILITY: ",12))
    {	VisName= FilenamePlus(line+12);
	if( ReadFrame(VisName,&VisFits))  VisName= NULL;
    }
    else
    if( !strncmp(line,"PHASE     : ",12))
    {	PhaName= FilenamePlus(line+12);
	if( ReadFrame(PhaName,&PhaFits))  PhaName= NULL;
    }
    else
    if( !strncmp(line,"DELTA VISI: ",12))
    {	DVisName= FilenamePlus(line+12);
	if( ReadFrame(DVisName,&DVisFits))  DVisName= NULL;
    }
    else
    if( !strncmp(line,"DELTA PHAS: ",12))
    {	DPhaName= FilenamePlus(line+12);
	if( ReadFrame(DPhaName,&DPhaFits))  DPhaName= NULL;
    }
    else
    if( !strncmp(line,"MASK      : ",12))
    {	MaskName= FilenamePlus(line+12);
	if( ReadFrame( MaskName,&MaskFits))  MaskName= NULL;
    }
  }
  fclose(fp);
  return 0;
}


/******************************************************************************/

static void PrintFileLine(FILE *fp, char *what, char *fname)
{
	if(fname) fprintf(fp,"%-8s= %s\n",what,fname );
	else	  fprintf(fp,"#%-8s= none\n",what );
}

void WriteBff(char *bff_out)
{
	FILE *outfp;
	char line[256];
	short fp_remote;

  if( !(outfp= rfopen(bff_out,"w")))
  {	fprintf(stderr," Can't open %s\a\n",bff_out);	}
  else
  {	fp_remote= rfopen_pipe;
	fprintf(stderr," Writing bff-file %s...\n",bff_out);

	fprintf(outfp,"#\n# %s	created by binplot from "__DATE__"\n#\n",bff_out);

	PrintFileLine(outfp,"visib" ,VisName );
	PrintFileLine(outfp,"dvisib",DVisName);
	fprintf(outfp,"#dvisscl = 1.0\n");

	PrintFileLine(outfp,"phase" ,PhaName );
	PrintFileLine(outfp,"dphase",DPhaName);
	fprintf(outfp,"#dphascl = 1.0\n");

	PrintFileLine(outfp,"mask", MaskName);
	PrintFileLine(outfp,"title",Title= Tcl_GetVar(tclip,"title",TCL_GLOBAL_ONLY));

	fprintf(outfp,"\n");
	fprintf(outfp,"ce_ang  = %f %f\n", Modell[CE_ANGLE], Modell[CE_ANGLE]+10.);
	fprintf(outfp,"ce_dist = %f 10.\n",Modell[CE_DIST] );
	fprintf(outfp,"angle1  = %f %f\n", Modell[ANGLE1],Modell[ANGLE1] );
	fprintf(outfp,"dist1   = %f %f\n", Modell[DIST1] ,Modell[DIST1]  );
	fprintf(outfp,"brightn1= %f %f\n", Modell[BRGTN1],Modell[BRGTN1] );

	if( N_comp>1)
	{ fprintf(outfp,"angle2  = %f %f\n", Modell[ANGLE1+3],Modell[ANGLE1+3] );
	  fprintf(outfp,"dist2   = %f %f\n", Modell[DIST1 +3],Modell[DIST1 +3] );
	  fprintf(outfp,"brightn2= %f %f\n", Modell[BRGTN1+3],Modell[BRGTN1+3] );
	}
	fprintf(outfp,"radius  = %d %d\n\n",rmin,rmax);

	if(RefIsMult)	fprintf(outfp,"RefIsMult\n\n");

	fprintf(outfp,	"bff_out = *.bffo\n"
			"fit_out = *.fit\n"
			"#err_vis = *.fits_evis\n"
			"#err_pha = *.fits_epha\n\n"
			"#csub_vis = csub.fits_tvis\n"
			"#csub_dvis= csub.fits_dtvis\n"
			"#csub_pha = csub.fits_pha\n"
			"#csub_dpha= csub.fits_dpha\n"	);

	if(fp_remote)
	{	pclose(outfp);	}
	else
	{	fclose(outfp);
		strcpy(line,getvisual());
		strcat(line," ");
		strcat(line,bff_out);
		system(line);
	}
  }
}


/*****************************************************************************/

short Plot(void)
{
	double errvis, errpha;

  if( rmin > rmax)
  {	int ch;

	fprintf(stderr," exchanging rmin and rmax.\n");
	ch= rmin;  rmin= rmax;  rmax= ch;
  }
  if( rmin==rmax)
  {	rmin=0;		fprintf(stderr," setting rmin to 0.\n"); }
  if( rmax==0)
  {	rmax= VisFits ? (VisFits->n1 / 3) : 42;
	fprintf(stderr," setting rmax to %d.\n",rmax);
  }
  CalcErrors( &errvis,&errpha);
  /*printf(" Deviations: vis %.4g, pha %.4g, total %.4g\n",errvis,errpha,errvis+errpha);*/

  if( !VisFits && !PhaFits)	fprintf(stderr," Nothing to plot!\a\n");
  else
  {	/*cpgpage();*/
	if( VisFits) {	cpgpanl(1,1);  PlotVisib(1,0);  }
	if( PhaFits) {	cpgpanl(1,2);  PlotPhase(1,0);  }
  }
  return 0;
}


/******************************************************************************/

static void Usage(char *prg)
{
  printf(" USAGE: %s [-h] [-v vis] [-dv delta-vis] [fit-file]\n\n",prg);
}

static void Help(char *prg)
{
  Usage(prg);
  printf(" where fit-file is the result of bff or amf.\n\n");
}

char *FitFileName= "";

int main( int argc, char **argv)
{
	int BP_AppInit(Tcl_Interp *interp);
#if 0
	int argh;
	char line[256], *spc;
	long i;
#endif

  fprintf(stderr,"binplot, compiled on " __DATE__
		 " ============================\n\n");

#if 0
  for(argh=1;  argh<argc;  ++argh)
  {
#ifdef DEBUG
    fprintf(stderr,"arg %d: %s\n",argh,argv[argh]);
#endif
    if( argv[argh][0] == '-')
    {
	if( argv[argh][1] == 'h')  { Help(argv[0]);  return 5;	}
	if( argv[argh][1] == 'v' && ++argh < argc)
	{	if( (VisName = strdup(argv[argh]))
		 &&  ReadFrame(VisName,&VisFits))  VisName= NULL;
	}
	if( argv[argh][1] == 'd' && argv[argh][2] == 'v' && ++argh < argc)
	{	if( (DVisName = strdup(argv[argh]))
		 &&  ReadFrame(DVisName,&DVisFits))  DVisName= NULL;
	}
    }
    else
    {	FitFileName= argv[argh];
	if( ReadFitFile(FitFileName))  return 20;
    }
  }
#endif
  Tk_Main(argc, argv, BP_AppInit);

  /********** All done, clean up and leave **********/
  fprintf(stderr," Good Bye!\n\n");

#ifdef GNUPLOT
  unlink("binplot.data");
  unlink("binplot.model");
#endif
  debug("closing pgplot\n");
  cpgend();

  debug("Closing Fits...\n");
  if( VisFits)	{ debug("\tvis\n");	FitsClose(VisFits);	}
  if(DVisFits)	{ debug("\tdvis\n");	FitsClose(DVisFits);	}
  if( PhaFits)	{ debug("\tpha\n");	FitsClose(PhaFits);	}
  if(DPhaFits)	{ debug("\tdpha\n");	FitsClose(DPhaFits);	}

  debug("freeing arrays\n");
  if( ProjData)	free(ProjData);
  free(pg_xval);

  return 0;
}


int BP_AppInit(Tcl_Interp *interp)
{
  tclip = interp;

  if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
  }
  Tcl_SetVar(interp,"argv0","binplot",TCL_GLOBAL_ONLY);

  if (Tk_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
  }
  Tcl_StaticPackage(interp, "Tk", Tk_Init, Tk_SafeInit);

#ifdef HAVE_TKPGPLOT
  {	int result;

    if( (result= Tkpgplot_Init(tclip)) != TCL_OK)
    {	fprintf(stderr,"Tkpgplot_Init() returned %d, reason:\a\n%s",result,tclip->result);
	return TCL_ERROR;
    }
    Tcl_SetVar(interp,"plot_device",".plw.plot/XTK",TCL_GLOBAL_ONLY);
  }
#else
    Tcl_SetVar(interp,"plot_device","/XS",TCL_GLOBAL_ONLY);
#endif

  if( InitGUI())
  {	Tcl_AppendResult(interp, "Could not initialize GUI", NULL);
	return TCL_ERROR;
  }
  return TCL_OK;
}
