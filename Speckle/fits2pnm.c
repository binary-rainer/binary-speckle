/* $Id: fits2pnm.c,v 1.4 1996/01/27 17:41:20 koehler Exp koehler $
 * $Log: fits2pnm.c,v $
 * Revision 1.4  1996/01/27  17:41:20  koehler
 * Seitenlinie im Verzeichnis fitsmovie: option -lut
 *
 * Revision 1.3  1995/03/05  17:09:36  koehler
 * Logscale in Saoimage-style
 *
 * Revision 1.2  1995/02/27  17:43:13  koehler
 * vor dem Umstricken auf Saoimage-style-log
 *
 * Revision 1.1  1995/02/17  15:00:41  koehler
 * Initial revision
 *
 * Revision 1.1  1995/01/02  21:28:08  koehler
 * Initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <ctype.h>	/** for isspace(), isdigit() **/
#include "fitsio.h"

#define RAWPNM 1

#define DEBUG	1

#ifdef DEBUG
# define debug(f,a,b)	fprintf(stderr,f,a,b)
#else
# define debug(f,a,b)	;
#endif

#define LUT_GRAY	1	/* grayscale		*/
#define LUT_BLACKBODY	2	/* "BB" in saoimage	*/
#define LUT_HEAT	3
#define LUT_RAINBOW	4

short Verbose=0;

/************************************* PGM/PPM ********************************/

unsigned char *ColorMap=NULL;	/** R/G/B nacheinander **/
long CMapSize=256;		/** fuer PGM **/

int PPMGetVal(FILE *fp)
{
	char ch, str[10];
	long i;

  while(1)
  {	while( isspace((int)(ch= getc(fp)))) ;		/** skip whitespace **/
	if( ch!='#')  break;
	while( (ch= getc(fp)) != '\n') ;	/** skip comment **/
  }
  str[0]= ch;
  for( i=1; i<9; ++i)	if( !isdigit((int)(str[i]= getc(fp))))  break;
  str[i]= 0;
  return atoi(str);
}

void ReadColorMap(char *name)
{
	FILE *fp;
	unsigned short wd, ht;
	char ch, type;
	long i;

  if( !(fp= fopen(name,"rb")))
  {	fprintf(stderr," Can't open color map %s!\a\n",name);	return;	}

  if(Verbose)	fprintf(stderr," Reading %s...\n",name);
  ch= getc(fp);		type= getc(fp);
  if( (ch != 'P') || ((type != '3') && (type != '6')))
  {	fprintf(stderr," Wrong magic number: %c%c!\a\n",ch,type);  fclose(fp);  return;	}

  wd= PPMGetVal(fp);	ht= PPMGetVal(fp);	PPMGetVal(fp);	/** max. interessiert nicht **/

  CMapSize= 3*wd*ht;	/** 3 bytes per pixel **/
  if( !(ColorMap= AllocBuffer(CMapSize,"color map")))	{ fclose(fp);	return;	}

  if( type=='3')
	for( i=0; i<CMapSize; ++i)	ColorMap[i]= PPMGetVal(fp);
  else	fread(ColorMap,sizeof(unsigned char),CMapSize,fp);
  fclose(fp);
  CMapSize /= 3;	/** Spaeter interessiert nur, wieviel Eintraege da drin sind **/
}


void WritePNM( char *Outname, char *Srcname,
			struct FitsFile *Src, double max, double min, double scale,
			short enlarge, short logflg, short revid)
{	FILE *fp;
	double val, prelogscl;
	short rx,ry,c;
	long x,y;

  if( !(fp= Outname? fopen(Outname,"wb") : stdout))
  {	fprintf(stderr," Can't open %s!\a\n",Outname);	return;	}

  if( Outname)	fprintf(stderr," Writing %s...\n",Outname);
  fprintf(fp,"P%c\n# Source: %s\n%d %d\n255\n",
#ifdef RAWPNM
		ColorMap? '6':'5',
#else
		ColorMap? '3':'2',
#endif
		Srcname, Src->n1*enlarge, Src->n2*enlarge);

  prelogscl= (exp(10.)-1.0) / (max-min);

  for( y=Src->n2-1;  y>=0;  --y)	/** Zeilen von oben nach unten **/
   for( ry=0;  ry<enlarge;  ++ry)	/** repeat lines **/
   { for( x=0;  x<Src->n1;  ++x)	/** Spalten von links nach rechts **/
     {
	val= Src->Buffer.d[x + Src->n1*y];
	if( val > max)	val= max;
	if( val <=min)	val= 0.0;
		else	val -= min;			/** min<= val <=max **/
	if(logflg)  val= log( val*prelogscl + 1.0) / 10.0;	/**  0 <= val <= 1  **/
		/** believe it or not, this is the same as saoimage's log-scale **/

	c= (short)(val * scale) % CMapSize;
	if(revid)  c= CMapSize-1-c;

	for( rx=0;  rx<enlarge;  ++rx)		/** repeat columns **/
	{
#ifdef RAWPNM
	  if( ColorMap)
		fprintf(fp,"%c%c%c",ColorMap[3*c],ColorMap[3*c+1],ColorMap[3*c+2]);
	  else	fprintf(fp,"%c",c);
#else
	  if( ColorMap)
		fprintf(fp,"%d %d %d\n",ColorMap[3*c],ColorMap[3*c+1],ColorMap[3*c+2]);
	  else	fprintf(fp,"%d\n",c);
#endif
	}
     }
   }
  if(Outname)	fclose(fp);	/** don't close stdout!! **/
}


/************************************** LUT stuff ***************************************/

#define MAXCOLORVALUE 255	/** highest color value **/
#define MAXCOLORINDEX 255	/** highest color index **/

#define RED	0
#define GREEN	1
#define BLUE	2

void Setlut (int color, double x0, double y0, double x1, double y1)
{
	float incry;
	int realx0,realy0;
	int realx1,realy1;
	int tmp;
	int count;

  realx0= (int)((float)(MAXCOLORINDEX)*x0);
  realx1= (int)((float)(MAXCOLORINDEX)*x1);
  realy0= (int)((float)(MAXCOLORVALUE)*y0);
  realy1= (int)((float)(MAXCOLORVALUE)*y1);

  if (realx0 > realx1)
  {	tmp=realx1; realx1=realx0; realx0=tmp;		}

  incry= (realx1==realx0) ? 0. : (float)(realy1-realy0) / (float)(realx1-realx0);

  for (count=realx0; count<=realx1; count++ )
	ColorMap[ count*3 + color]=
		realy0 + (int)(incry * (count-realx0) + 0.5);
}

void CreateLut(short lut)
{
  CMapSize= 256;
  if( !(ColorMap= AllocBuffer(3*CMapSize,"color map")))	return;

  switch( lut)
  { case LUT_BLACKBODY:	   /* x0   y0	x1   y1	*/
		Setlut( RED,  0.0 ,0.0, 0.5 ,1.0 );
		Setlut( RED,  0.5 ,1.0, 1.0 ,1.0 );
		Setlut(GREEN, 0.0 ,0.0, 0.25,0.0 );
		Setlut(GREEN, 0.25,0.0, 0.75,1.0 );
		Setlut(GREEN, 0.75,1.0, 1.0 ,1.0 );
		Setlut( BLUE, 0.0 ,0.0, 0.5 ,0.0 );
		Setlut( BLUE, 0.5 ,0.0, 1.0 ,1.0 );
		break;
    case LUT_HEAT:	   /* x0   y0	x1   y1	*/
		Setlut( RED,  0.0 ,0.0, 0.25,0.0 );
		Setlut( RED,  0.25,0.0, 0.5 ,1.0 );
		Setlut( RED,  0.5 ,1.0, 1.0 ,1.0 );
 		Setlut(GREEN, 0.0 ,0.0, 0.5 ,0.0 );
		Setlut(GREEN, 0.5 ,0.0, 0.75,1.0 );
		Setlut(GREEN, 0.75,1.0, 1.0 ,1.0 );
		Setlut( BLUE, 0.0 ,0.0, 0.25,1.0 );
		Setlut( BLUE, 0.25,1.0, 0.5 ,0.0 );
		Setlut( BLUE, 0.5 ,0.0, 0.75,0.0 );
		Setlut( BLUE, 0.75,0.0, 1.0 ,1.0 );
		break;
    case LUT_RAINBOW:	   /* x0   y0	x1   y1	*/
		Setlut( RED,  0.0 ,1.0, 0.33,0.0 );
		Setlut( RED,  0.33,0.0, 0.67,0.0 );
		Setlut( RED,  0.67,0.0, 1.0 ,1.0 );
 		Setlut(GREEN, 0.0 ,0.0, 0.33,1.0 );
		Setlut(GREEN, 0.33,1.0, 0.67,0.0 );
		Setlut(GREEN, 0.67,0.0, 1.0 ,0.0 );
		Setlut( BLUE, 0.0 ,0.0, 0.33,0.0 );
		Setlut( BLUE, 0.33,0.0, 0.67,1.0 );
		Setlut( BLUE, 0.67,1.0, 1.0 ,0.0 );
		break;
    default:			/** grayscale **/
		Setlut( RED,  0.0,0.0, 1.0,1.0 );
		Setlut(GREEN, 0.0,0.0, 1.0,1.0 );
		Setlut( BLUE, 0.0,0.0, 1.0,1.0 );
		break;
  }
}


/*****************************************************************************/

static short FindMidasCuts( struct FitsFile *ff,
				double *minp, double *maxp, short minflg, short maxflg)
{
	char *ptr, ch;

  for( ptr= ff->Header+80;  ptr < ff->Header+ff->HeaderSize;  ptr+=80)
	if( !strncmp(ptr,"HISTORY  'LHCUTS'",17))	break;

  if( ptr >= ff->Header+ff->HeaderSize)
  {	fprintf(stderr," No MIDAS-cuts found!\a\n");	return 1;	}

  ptr += 89;		/** Cuts are in the next line behind "HISTORY  " **/
  if( !minflg)		/** no min on cmd line given **/
  {	ch = ptr[14];	ptr[14]= 0;	/** start of next value **/
	*minp = atof(ptr);
	ptr[14]= ch;
  }
  ptr += 14;
  if( !maxflg)		/** no max on cmd line given **/
  {	ch = ptr[14];	ptr[14]= 0;	/** start of next value **/
	*maxp = atof(ptr);
	ptr[14]= ch;
  }
  if(Verbose)
	fprintf(stderr," Using cuts: min = %g, max = %g\n", *minp, *maxp);
  return 0;
}

/*****************************************************************************/

static void FindMinMaxCube( struct FitsFile *ff,
				double *minp, double *maxp, short minflg, short maxflg)
{
	long x,y,fr, minx,miny,minfr, maxx,maxy,maxfr, offs;
	double val;

  minx= maxx= miny= maxy= minfr= maxfr= -9999;
  if( !minflg)  *minp=  DBL_MAX;
  if( !maxflg)  *maxp= -DBL_MAX;

  for( fr=0;  fr<ff->n3;  ++fr)
    for( y= ff->n2-2;  y>1;  --y)	/** ignore 1 pixel at edges **/
    {	offs= (fr*ff->n2 + y) * ff->n1;
	for( x= ff->n1-2;  x>1;  --x)
	{ val= ff->Buffer.d[offs + x];
	  if( !minflg && *minp > val) {	*minp= val;  minx= x;  miny= y;  minfr= fr;  }
	  if( !maxflg && *maxp < val) {	*maxp= val;  maxx= x;  maxy= y;  maxfr= fr;  }
	}
    }
  if(Verbose)
  { fprintf(stderr," min = %g ", *minp);
    if( !minflg) fprintf(stderr,"@ (%ld,%ld,%ld)", minx,miny,minfr);
	else	 fprintf(stderr,"(user-given)");

    fprintf(stderr,"; max = %g ", *maxp);
    if( !maxflg) fprintf(stderr,"@ (%ld,%ld,%ld)\n", maxx,maxy,maxfr);
	else	 fprintf(stderr,"(user-given)");
  }
}

/*****************************************************************************/

static void Usage(char *prg)
{
  printf(
	" USAGE: %s [-v] [-log] [-rv] [-wrap n] [-enlarge n] [-cmap colors-ppm]\n"
	"		[-lut LUT] [-min num] [-max num] [-midas_cuts]\n"
	"		[-o output-ppm] input-fits\n",prg);
}

static void Help(char *prg)
{
 Usage(prg);
 printf("\n where:"
	"\t-v		makes it verbose\n"
	"\t-log		selects logarithmic scaling\n"
	"\t-rv		selects \"reverse video\"\n"
	"\t-wrap n\t	wraps colormap n times\n"
	"\t-enlarge n	enlarges image by integer factor n\n"
	"\t-cmap ppm	uses ppm-file as colormap\n"
	"\t-lut (gray|BB|heat|rainbow)	use inbuild color look-up table\n"
	"\t-min value	minimum data value (scaled to lowest color)\n"
	"\t-max value	maximum data value (scaled to highest color)\n"
	"\t-midas_cuts	searches for MIDAS-cuts in Fitsheader\n"
	"\t-o output	name of output file (default stdout)\n"
	"\tinput	the fits file to convert\n\n");
}


int main( int argc, char **argv)
{
	struct FitsFile *InFits;
	union anypntr save_bufp;
	char  Outnmbuf[40], *Outname= NULL;
	double min=0.0, max=0.0, val;
	long  cubesz, arg, i;
	short revid=0, logflg=0,wrap=1,enlarge=1,minflg=0,maxflg=0,midas=0,lut=LUT_GRAY;

  for( arg=1;  arg<argc;  ++arg)
  {	debug(" argv[%ld] = %s\n",arg,argv[arg]);

	if( !strcmp( argv[arg],"-v" ))	 Verbose= 1;
   else	if( !strcmp( argv[arg],"-rv" ))	 revid= 1;
   else	if( !strcmp( argv[arg],"-log"))  logflg=1;
   else	if( !strcmp( argv[arg],"-o" )	&& ++arg<argc)  Outname= argv[arg];
   else	if( !strncmp(argv[arg],"-cm",3) && ++arg<argc)	ReadColorMap(argv[arg]);
   else	if( !strncmp(argv[arg],"-enl",4)&& ++arg<argc)	enlarge= atoi(argv[arg]);
   else	if( !strncmp(argv[arg],"-w",2)	&& ++arg<argc)  wrap= atoi(argv[arg]);
   else	if( !strcmp( argv[arg],"-lut")	&& ++arg<argc)
	{
	  switch( argv[arg][0])
	  { default:	fprintf(stderr," Bad lut \"%s\", using \"gray\"\a\n",argv[arg]);
	    case 'g':
	    case 'G':	lut= LUT_GRAY;		break;
	    case 'b':
	    case 'B':	lut= LUT_BLACKBODY;	break;
	    case 'h':
	    case 'H':	lut= LUT_HEAT;		break;
	    case 'r':
	    case 'R':	lut= LUT_RAINBOW;	break;
	  }
	  if( lut != LUT_GRAY)	CreateLut(lut);
	}
   else	if( !strcmp( argv[arg],"-min") && ++arg<argc)	{ min= atof(argv[arg]);	 minflg=1; }
   else	if( !strcmp( argv[arg],"-max") && ++arg<argc)	{ max= atof(argv[arg]);	 maxflg=1; }
   else	if( !strcmp( argv[arg],"-midas_cuts"))	{	midas=1;	}
   else	if( !strncmp(argv[arg],"-h",2))		{	Help(argv[0]);	exit(0); }
   else	if( argv[arg][0]=='-')
	{  fprintf(stderr," unknown option \"%s\"\n",argv[arg]);  Help(argv[0]);  return 5;  }
   else break;
  }
  /** pnm-Output geht nach stdout, deshalb text nach stderr **/
  if(Verbose)
	fprintf(stderr,"\nfits2pnm by Rainer Köhler, compiled on "__DATE__
			" =========================\n\n");

  if( arg>=argc)
  {	fprintf(stderr," No inputfile given!\n");  Usage(argv[0]);  return 15;	}

  if(wrap<=0)  { fprintf(stderr," Bad wrap count %d, using 1\a\n",wrap);  wrap=1;  }
  if(enlarge<=0)
  { fprintf(stderr," Bad enlargement factor %d, using 1\a\n",enlarge);  enlarge=1; }

  if( !(InFits= FitsOpenR( argv[arg])))	return 20;

  cubesz= InFits->n1 * InFits->n2 * InFits->n3;

  if( !(InFits->Buffer.d = AllocBuffer(sizeof(double)*cubesz,"input buffer")))
  {	FitsClose(InFits);	return 20;	}

  if(Verbose)	fprintf(stderr," Reading %s...(%d x %d x %d)\n",
				argv[arg],InFits->n1,InFits->n2,InFits->n3);
  if( FitsReadDoubleBuf( InFits, InFits->Buffer.d, cubesz) < cubesz)
  {	fprintf(stderr,"Not enough data in %s\n",argv[arg]);
	FitsClose(InFits);	return 20;
  }

  if( !minflg || !maxflg)	/** wenn beide gegeben sind, brauchen wir nicht suchen **/
  {	if( midas==0 || FindMidasCuts( InFits, &min,&max,minflg,maxflg))
		FindMinMaxCube( InFits, &min,&max, minflg,maxflg);
  }
  val= (double)(CMapSize*wrap - 1);
  if( !logflg)	val /= (max-min);	/** scale factor: max->CMapSize*wrap-1 **/
  debug(", scale = %g%c\n",val,' ');

  if( InFits->n3==1)
	WritePNM(Outname,argv[arg],InFits,max,min,val,enlarge,logflg,revid);
  else
  { save_bufp.d = InFits->Buffer.d;
    for( i=0;  i<InFits->n3;  ++i)
    {	if( Outname)	strcpy(Outnmbuf,Outname);
		else	Outnmbuf[0]= 0;
	sprintf( Outnmbuf+strlen(Outnmbuf),"%04ld.ppm",i);

	InFits->Buffer.d = save_bufp.d + i * InFits->n1 * InFits->n2;
	WritePNM(Outnmbuf,argv[arg],InFits,max,min,val,enlarge,logflg,revid);
    }
    InFits->Buffer.d = save_bufp.d;
  }
  FitsClose(InFits);
  if(ColorMap)	free(ColorMap);
  return 0;
}
