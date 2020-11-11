/* $Id: saa.c 1.3 1996/04/04 07:34:15 koehler Exp koehler $
 * $Log: saa.c $
 * Revision 1.3  1996/04/04  07:34:15  koehler
 * vor Aenderungen fuer mehrere Input-files
 *
 * Revision 1.2  1995/08/22  14:46:13  koehler
 * Revision 1.1  1995/03/05  17:09:36  koehler
 * Initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <unistd.h>
#include "fitsio.h"


short Verbose=0;

/*****************************************************************************/

void FindMaxPos(double *buf, long sizex, long sizey,
		long xstart, long xend, long ystart, long yend,
		long *xmax,  long *ymax )
{
	double max;
	long x, y;

  *xmax= *ymax= 0;

  if( xstart<0)	xstart=0;	if( xend>sizex)	xend=sizex;
  if( ystart<0)	ystart=0;	if( yend>sizey)	yend=sizey;

  max= buf[xstart + sizex*ystart];
  for( x=xstart;  x<xend;  ++x)
    for( y=ystart;  y<yend;  ++y)
	if( max < buf[x + sizex*y])
	{	max= buf[x + sizex*y];	*xmax= x;  *ymax= y;	}
}

/*****************************************************************************/

void FindCenter(double *buf, long sizex, long sizey,
		long xstart, long xend, long ystart, long yend,
		long *centx, long *centy)
{
	long   x,y;
	double sum,sumx,sumy, help;

  sum= sumx= sumy= 0.0;
  for( x=xstart;  x<xend && x<sizex;  ++x)
    for( y=ystart;  y<yend && y<sizey;  ++y)
    {	help= buf[x+y*sizex];
	sum  += help;
	sumx += (double)x * help;
	sumy += (double)y * help;
    }
  *centx= (long)(sumx/sum + 0.5);
  *centy= (long)(sumy/sum + 0.5);
}

/*****************************************************************************/

static void AskForStartPos(char *fname, long *xto, long *yto)
{
  char line[256], *strp= malloc(30+strlen(fname));
  FILE *fp;
  short found=0;

  /** tell ds9 to display the file **/
  strcpy(strp,"xpaset -p 'ds9*' file \"");
  strcat(strp,fname);
  strcat(strp,"\"");
  system(strp);
  free(strp);

  /** wait for some key **/
  /**fgets(line,255,stdin);**/

  while(found==0)
  {
    if( !(fp= popen("xpaget \"ds9*\" regions","r")))
    {	printf("Couldn't read region!\a\n");	return;	}

    while( fgets(line,255,fp))
    {
	if(Verbose>2)  printf("regline: %s",line);
	strp= line;
	if( *strp != '#')
	{ if( *strp == ' ')  ++strp;
	  if( !strncasecmp(strp,"POINT(",6))	/** saoimage-point **/
	  {
	    *xto = (long)(atof(strp+6)-0.5);	/** subtract 1 and round **/
	    strp = strchr(strp,',')+1;
	    *yto = (short)(atof(strp)-0.5);	/** subtract 1 and round **/
	    found= 1;
	    break;
	  }
	  else if( !strncasecmp(strp,"circle point ",13))	/** ds9 v.1.9.4 **/
	  {
	    strp += 13;
	    *xto = (long)(atof(strp)-0.5);	/** subtract 1 and round **/
	    strp = strchr(strp,' ')+1;
	    *yto = (long)(atof(strp)-0.5);
	    found= 1;
	    break;
	  }
	  else if( !strncasecmp(strp,"image;point(",12))	/** ds9 v.1.9.6 **/
	  {
	    strp += 12;
	    *xto = (long)(atof(strp)-0.5);	/** subtract 1 and round **/
	    strp = strchr(strp,',')+1;
	    *yto = (long)(atof(strp)-0.5);
	    found= 1;
	    break;
	  }
	  else if( !strncasecmp(strp,"image;circle(",13))	/** ds9 v.1.9.6 **/
	  {
	    strp += 13;
	    *xto = (long)(atof(strp)-0.5);	/** subtract 1 and round **/
	    strp = strchr(strp,',')+1;
	    *yto = (long)(atof(strp)-0.5);
	    found= 1;
	    break;
	  }
	}
    }	/** while(fgets... **/
    if(!found)  sleep(1);
  } /** while not found **/

  if(Verbose>1)  printf("found something at %ld,%ld\n",*xto,*yto);
  pclose(fp);
}

/*****************************************************************************/

static void Usage(char *prg)
{
  printf(" USAGE: %s [-b bitpix] [-m MapName] [-o OutName] [-verbose] [-interact]\n"
	 "	      [-xto x] [-yto y] [-xbr x] [-ybr y] [-center] fitsfile[s]\n",prg);
}

static void Help(char *prg)
{
 Usage(prg);
 printf(" where:\t-b bitpix	gives format of output\n"
	"	-m filename	writes map with positions of max pixels\n"
	"	-o filename	gives name of output\n"
	"	-xto x		position where to shift the max/center\n"
	"	-yto y		(default is position in first frame)\n"
	"	-xbr x		radius of search box in x\n"
	"	-ybr x		radius of search box in y\n"
	"	-center		uses center of light instead of maximum\n"
	"	-verbose	increases verbosity\n"
	"	-interact	ask for start pos. using ds9\n"
	"	fitsfiles[s]	the name[s] of the input file[s]\n\n"	);
}


int main( int argc, char **argv)
{
	struct FitsFile *InFits, OutFits;
	char *InName, *OutName=NULL, *MapName=NULL, *hdrline;
	double *FrmBuf, *SaaBuf;
	short *MapBuf=NULL, Center=0, Interactive=0;
	unsigned long framesz, arg, fr, i;
	long x,y,x0,y0,xto=-1,yto=-1, xbr=10,ybr=10;

  fprintf(stderr,"\nsaa, compiled on "__DATE__" ========================================\n");

  if( argc<2 || !strncmp(argv[1],"-h",2))
  {	Help(argv[0]);		return 0;	}

  OutFits.bitpix= 0;	/** Default: same as input **/
  OutFits.naxis = 0;	/** will be set when 1.file is read **/

  for( arg=1;  arg<argc;  arg++)
  {	char *argp= argv[arg];

	if( !strcmp(argp,"-xbr") && ++arg<argc)	 xbr= atol(argv[arg]);
   else	if( !strcmp(argp,"-ybr") && ++arg<argc)	 ybr= atol(argv[arg]);
   else	if( !strcmp(argp,"-xto") && ++arg<argc)	 xto= atol(argv[arg]);
   else	if( !strcmp(argp,"-yto") && ++arg<argc)	 yto= atol(argv[arg]);
   else	if( !strncmp(argp,"-b",2) && ++arg<argc)
	{	x= atoi(argv[arg]);
		if( x!=8 && x!=16 && x!=32 && x!=-32 && x!=-64)
			printf(" Unknown bitpix value %ld, using same as first input\a\n",x);
		else	OutFits.bitpix= x;
	}
   else	if( !strncmp(argp,"-o",2) && ++arg<argc)  OutName= argv[arg];
   else	if( !strncmp(argp,"-m",2) && ++arg<argc)  MapName= argv[arg];
   else	if( !strncmp(argp,"-c",2))	Center= 1;
   else	if( !strncmp(argp,"-i",2))	{ Interactive= 1; xto=0; yto=0; }
   else	if( !strncmp(argp,"-h",2))	{ Help(argv[0]);  return 0;	}
   else	if( !strncmp(argp,"-v",2))	Verbose++;
   else	if( argp[0]=='-')
	{	printf(" Unknown option: %s\a\n",argv[arg]);
		Usage(argv[0]);		return 5;
	}
	else	break;
  }
  do	/** loop over remaining arguments = files **/
  {
    InName= arg<argc? argv[arg] : NULL;
    if( !(InFits= FitsOpenR(InName)))	return 20;
    fprintf(stderr," Reading %s (%d x %d x %d)...\n",
		InName? InName:"from stdin", InFits->n1, InFits->n2, InFits->n3 );

    if( OutFits.bitpix==0)	OutFits.bitpix= InFits->bitpix;
    if( OutFits.naxis==0)	/** this is the 1.input file **/
    {	OutFits.flags= 0;   OutFits.fp= NULL;
	OutFits.naxis= 2;
	OutFits.n1= InFits->n1;	 OutFits.n2= InFits->n2;  OutFits.n3= 1;
	strcpy( OutFits.object, InFits->object);
	OutFits.Header= InFits->Header;  OutFits.HeaderSize= InFits->HeaderSize;
	InFits->Header= NULL;	/** don't free() when closing input **/

	framesz= InFits->n1 * InFits->n2;
	if( !(FrmBuf= AllocBuffer( 5*framesz*sizeof(double),"frame buffer")))
	{	FitsClose(InFits);	return 20;	}
	SaaBuf= FrmBuf+framesz;

	if( MapName)
	{ if( !(MapBuf= AllocBuffer(framesz*sizeof(short),"maxpos-map")))
		MapName=NULL;
	  else for( i=0;  i<framesz;  ++i)	MapBuf[i]=0;
	}
    }
    else	/** not 1. file **/
    {	if( InFits->n1 != OutFits.n1 || InFits->n2 != OutFits.n2)
	{ fprintf(stderr," Frame sizes do not match - skipping file\a\n");
	  FitsClose(InFits);	continue;
	}
    }
    for( fr=0, x0=xto, y0=yto;  fr<InFits->n3;  ++fr)
    {	long dx,dy;

	FitsReadDoubleBuf( InFits, FrmBuf, framesz);

	if (xto < 0 || yto < 0) {	/** we don't know where to shift it yet **/
	  if(Center) {
		FindCenter( SaaBuf,InFits->n1,InFits->n2, 0,InFits->n1, 0,InFits->n2, &xto,&yto);
		/** the position of the center might shift if we look at a box only **/
		FindCenter( FrmBuf,InFits->n1,InFits->n2, x0-xbr,x0+xbr, y0-ybr,y0+ybr, &x0,&y0);
	  } else {
		FindMaxPos( SaaBuf,InFits->n1,InFits->n2, 0,InFits->n1, 0,InFits->n2, &xto,&yto);
		x0= xto;  y0= yto;
	  }
	} else {
	  if(Interactive)  AskForStartPos(InName,&x0,&y0);

	  if(Center)
		FindCenter( FrmBuf,InFits->n1,InFits->n2, x0-xbr,x0+xbr, y0-ybr,y0+ybr, &x0,&y0);
	  else	FindMaxPos( FrmBuf,InFits->n1,InFits->n2, x0-xbr,x0+xbr, y0-ybr,y0+ybr, &x0,&y0);
		/** We search in the vicinity of the last frame's max **/

	  if(Interactive && xto==0 && yto==0) { xto= x0;  yto= y0; }
		/** I hope nobody wants to shift to 0,0 **/
	}
	if( x0 < 0 || x0 >= InFits->n1 || y0 < 0 || y0 >= InFits->n2) {
	  fprintf(stderr," max/center position out of frame: %ld, %ld\n", x0, y0);
	  continue;
	}
	if( MapBuf)	++MapBuf[x0 + InFits->n1*y0];
	if(Verbose)	fprintf(stderr," frame%4ld: (%3ld,%3ld)\n",fr,x0,y0);
	dx= x0-xto;	dy= y0-yto;

	for( x=0;  x<InFits->n1;  ++x)
	  for( y=0;  y<InFits->n2;  ++y)
		SaaBuf[ ((x-dx)%InFits->n1) + InFits->n1 * ((y-dy)%InFits->n2)]
			+= FrmBuf[ x + InFits->n1*y];
    }
    {	char histline[88];

	sprintf(histline,"Shift and Add of %s",InName? InName:"stdin");
	FitsAddHistKeyw( &OutFits,histline);
    }
    FitsClose(InFits);
  }while( ++arg<argc);

  FitsWrite( OutName, &OutFits, SaaBuf);

  if( MapBuf)
  {	OutFits.bitpix= 16;
	if( (hdrline= FitsFindEmptyHdrLine( &OutFits)))
		memcpy( hdrline,"COMMENT = Positions of brightest pixel",38);
	FitsWriteAny( MapName, &OutFits, MapBuf,16);
	free(MapBuf);
  }
  free(FrmBuf);	/** incl. SaaBuf **/
  return 0;
}
