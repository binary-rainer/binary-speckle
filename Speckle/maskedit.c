/* $Id$
 * $Log$
 * Created:     Wed Nov  1 13:32:12 2000 by Koehler@hex.ucsd.edu
 * Last change: Fri Feb 15 11:35:06 2008
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "fitsio.h"

short verbose = 1;	/** silent/verbose-Flag **/

struct FitsFile *ImgFits=NULL, *MaskFits=NULL;
char MaskName[256], ImgName[256];
/** the names are read from line-variable in int main(),
 ** which is 256 chars, so it doesn't make sense to strdup()/free() them **/

/*****************************************************************************/

int read_mask(char *name)
{
	short *buf;
	long framesz, i;

  if( name==NULL || *name==0)
  {	fprintf(stderr,"Please give the filename of the mask\a\n");
	return 1;
  }
  FitsClose(MaskFits);
  strcpy(MaskName,name);
  if( !(MaskFits= FitsOpenR(MaskName)))  return 2;

  framesz= MaskFits->n1 * MaskFits->n2;
  if( !(buf= AllocBuffer(framesz*sizeof(short),"mask buffer")))
  {	FitsClose(MaskFits);	MaskFits= NULL;	  return 3;		}
  MaskFits->Buffer.s = buf;

  for( i=0;  i<framesz;  ++i)
	*buf++ = FitsReadShort(MaskFits);

  fclose(MaskFits->fp);  MaskFits->fp= NULL;	/** keep the Header **/

  if( MaskFits->flags & (FITS_FLG_EOF|FITS_FLG_ERROR))  return i+100;
  return 0;
}

/*****************************************************************************/

int read_img(char *name)
{
	long framesz, i;

  if( name==NULL || *name==0)
  {	fprintf(stderr,"Please give the filename of the image\a\n");
	return 1;
  }
  FitsClose(ImgFits);
  strcpy(ImgName,name);
  if( !(ImgFits= FitsOpenR(ImgName)))  return 2;

  framesz= ImgFits->n1 * ImgFits->n2;
  if( !(ImgFits->Buffer.d = AllocBuffer(framesz*sizeof(double),"image buffer")))
  {	FitsClose(ImgFits);	ImgFits= NULL;	  return 3;		}

  i= FitsReadDoubleBuf(ImgFits, ImgFits->Buffer.d, framesz);
  fclose(ImgFits->fp);  ImgFits->fp= NULL;	/** keep the Header **/

  if( ImgFits->flags & (FITS_FLG_EOF|FITS_FLG_ERROR))  return i+100;
  return 0;
}

/*****************************************************************************/

int read_region(char *name)
{
	FILE *fp;
	char line[256], *strp;
	long npnts= 0;
	short x,y, naxis1,naxis2;

  if( !MaskFits)
  {	fprintf(stderr,"No mask loaded\n");  return 1;	}

  naxis1= MaskFits->n1;
  naxis2= MaskFits->n2;

  if( name==NULL || *name==0)
	fp= popen("xpaget \"ds9*\" regions","r");
  else	fp= fopen(name,"r");

  if(fp==NULL)
  {	printf("Couldn't read region!\a\n");	return 2;	}

  while( fgets(line,255,fp))
  {
    if(verbose>1)  printf("regline: %s",line);
    strp= line;
    if( *strp != '#')
    { if( *strp == ' ')  ++strp;
      if( !strncasecmp(strp,"POINT(",6))	/** saoimage-point **/
      {
 	x= (short)(atof(strp+6)-0.5);	/** subtract 1 and round **/
	strp= strchr(strp,',')+1;
	y= (short)(atof(strp)-0.5);	/** subtract 1 and round **/
	if( (strp= strchr(strp,'\n')))	*strp= 0;

	if( x>=0 && y>=0 && x<naxis1 && y<naxis2 )
	{	if(verbose>1)  printf("\t%s => %d,%d\n",line,x,y);
		MaskFits->Buffer.s[ x + naxis1*y]= 42;
		npnts++;
	}
	else	if(verbose>1)  printf("\t%s => %d,%d out of range!\n",line,x,y);
      }
      else if( !strncasecmp(strp,"circle point ",13))	/** ds9 v.1.9.4 **/
      {
 	strp += 13;
	x= (short)(atof(strp)-0.5);	/** subtract 1 and round **/
	strp= strchr(strp,' ')+1;
	y= (short)(atof(strp)-0.5);
	if( (strp= strchr(strp,'\n')))	*strp= 0;

	if( x>=0 && y>=0 && x<naxis1 && y<naxis2 )
	{	if(verbose>1)  printf("\t%s => %d,%d\n",line,x,y);
		MaskFits->Buffer.s[ x + naxis1*y]= 42;
		npnts++;
	}
	else	if(verbose>1)  printf("\t%s => %d,%d out of range!\n",line,x,y);
      }
      else if( !strncasecmp(strp,"image;point(",12))	/** ds9 v.1.9.6 **/
      {
 	strp += 12;
	x= (short)(atof(strp)-0.5);	/** subtract 1 and round **/
	strp= strchr(strp,',')+1;
	y= (short)(atof(strp)-0.5);
	if( (strp= strchr(strp,'\n')))	*strp= 0;

	if( x>=0 && y>=0 && x<naxis1 && y<naxis2 )
	{	if(verbose>1)  printf("\t%s => %d,%d\n",line,x,y);
		MaskFits->Buffer.s[ x + naxis1*y]= 42;
		npnts++;
	}
	else	if(verbose>1)  printf("\t%s => %d,%d out of range!\n",line,x,y);
      }
      else if( !strncasecmp(strp,"image;box(",10))	/** ds9 1.9.6 **/
      {		float rx,ry, xx,yy;

	strp += 10;
	xx= atof(strp)-1.;	strp= strchr(strp,',')+1;
	yy= atof(strp)-1.;	strp= strchr(strp,',')+1;
	rx= atof(strp)/2.;	strp= strchr(strp,',')+1;
	ry= atof(strp)/2.;
	if( (strp= strchr(strp,'\n')))	*strp= 0;

	if(verbose>1)  printf("	%s => (%f,%f)...(%f,%f)\n",line,xx-rx,yy-ry,xx+rx,yy+ry);
	for( x= xx>rx? xx-rx:0;  x<naxis1 && x<xx+rx;  ++x)
	  for( y= yy>ry? yy-ry:0;  y<naxis2 && y<yy+ry;  ++y)
	  {	MaskFits->Buffer.s[ x + naxis1*y]= 43;
		npnts++;
	  }
      }
      else if( !strncasecmp(strp,"BOX(",4))
      {		short rx,ry, xx,yy;

	strp += 4;
 	x = (short)(atof(strp)-0.5);	strp= strchr(strp,',')+1;
	y = (short)(atof(strp)-0.5);	strp= strchr(strp,',')+1;
	rx= (short)(atof(strp)+0.5)/2;	strp= strchr(strp,',')+1;
	ry= (short)(atof(strp)+0.5)/2;
	if( (strp= strchr(strp,'\n')))	*strp= 0;

	if(verbose>1)  printf("	%s => (%d,%d)...(%d,%d)\n",line,x-rx,y-ry,x+rx,y+ry);
	for( xx= x>rx? x-rx:0;  xx<naxis1 && xx<x+rx;  ++xx)
	  for( yy= y>ry? y-ry:0;  yy<naxis2 && yy<y+ry;  ++yy)
	  {	MaskFits->Buffer.s[ xx + naxis1*yy]= 43;
		npnts++;
	  }
      }
    }
  }	/** while(fgets... **/
  if( name==NULL || *name==0)	pclose(fp);
		else		fclose(fp);

  if(verbose>1)  printf("added %ld pixels to mask\n",npnts);
  return 0;
}


/*****************************************************************************/

int set_pixel(char *coords)
{
  int x, y, val= 80;

  if( !MaskFits)
  {	fprintf(stderr,"No mask loaded\n");  return 1;	}

  x= atoi(coords)-1;
  if( !(coords= strchr(coords,' ')))
  {	printf("USAGE: set x y [value]\n");
	return 1;
  }
  while( *coords==' ')  ++coords;
  y= atoi(coords)-1;
  if( (coords= strchr(coords,' ')))
  {	while( *coords==' ')  ++coords;
	val= atoi(coords);
  }
  if( x<0 || y<0 || x>=MaskFits->n1 || y>=MaskFits->n2 )
  {	printf("pixel (%d,%d) is not in mask\n",x+1,y+1);
	return 2;
  }
  else
  {	printf("Setting pixel (%d,%d) to %d\n",x+1,y+1,val);
	MaskFits->Buffer.s[ x + MaskFits->n1*y]= val;
  }
  return 0;
}

/*****************************************************************************/

int invert_mask(char *dummy)
{
  int i;

  if( !MaskFits)
  {	fprintf(stderr,"No mask loaded!\a\n");
	return 2;
  }

  for( i=0;  i < MaskFits->n1 * MaskFits->n2;  ++i) {
	MaskFits->Buffer.s[i] = !MaskFits->Buffer.s[i];
  }
  return 0;
}


/*****************************************************************************/

int write_mask(char *name)
{
  if( !MaskFits)
  {	fprintf(stderr,"No mask loaded!\a\n");
	return 2;
  }
  if( name==NULL || *name==0)	name= MaskFits->filename;

  return FitsWriteAny( name, MaskFits, MaskFits->Buffer.s, BITPIX_SHORT);
}


/*****************************************************************************/

int write_image(char *name)
{
  if( !ImgFits)
  {	fprintf(stderr,"No image loaded!\a\n");
	return 2;
  }
  if( name==NULL || *name==0)	name= ImgFits->filename;

  /***** Correct the bad pixels first... *****/
  {	long x,y,xx,yy, offs, r, cnt;
	long n1 = ImgFits->n1, n2 = ImgFits->n2;
	double sum;

    for( x=0;  x<n1;  ++x)
      for( y=0;  y<n2;  ++y)
	if( MaskFits->Buffer.s[ x + y*n1])	/** bad pixel! **/
	{ r = 2;
	  do{	cnt=0;  sum=0.0;
		xx= x-r;  if( xx<0) xx=0;
		for( ;  xx<n1 && xx<=x+r;  ++xx)
		{ yy= y-r;  if( yy<0) yy=0;
		  for( ; yy<n2 && yy<=y+r;  ++yy)
		  { offs= xx + yy*n1;
		    if( !MaskFits->Buffer.s[offs])
		    {	cnt++;	sum += ImgFits->Buffer.d[offs];	}
		  }
		}
		if( cnt<4)  ++r;
	    }while( cnt<4);	/** we want mean of at least 4 good pix **/
	  ImgFits->Buffer.d[x + y*n1] = sum/(double)cnt;
	  if( verbose && r>2)
		printf(" Too many bad pixels around (%ld,%ld), increased radius to %ld\n",x,y,r);
	}
  }
  return FitsWriteAny( name, ImgFits, ImgFits->Buffer.d, BITPIX_DOUBLE);
}


/*****************************************************************************/

void do_command(char *cmd)
{
	char *spc;

  while( *cmd==' ' || *cmd=='\t')  ++cmd;	/** skip leading spc **/

  if( *cmd=='q' || !strncmp(cmd,"exit",4) || !strncmp(cmd,"bye", 3))	/**** q = exit = bye ****/
  {
    /** ask to save mask first? **/
	printf("Good Bye!\n\n");  fflush(stdout);	exit(0);
  }
  if( (spc= strchr(cmd,'\n')))  *spc= 0;
  for( spc= strchr(cmd,' ');  spc && *spc && *spc==' ';  ++spc)	;

  if( !strncmp(cmd,"-h",2) || cmd[0]=='?' || cmd[0]=='h')	/** help **/
  {	printf(	"Available commands are:\n"
		"	exit   quit   bye\n"
		"	silent\n"
		"	verbose\n"
		"	mask <filename>\n"
		"	image <filename>\n"
		"	invert\n"
		"	region [<filename>] (default: xpaget from ds9)\n"
		"	set x y [value] (first pixel is coordinate 1)\n"
		"	wmask [<filename>]\n"
		"	wimage [<filename>]\n"	);
  }
  else if( cmd[0]=='s')
  {	if( cmd[1]=='i')  verbose = 0;		/** silent **/
	if( cmd[1]=='e')  set_pixel(spc);	/** set **/
  }
  else if( cmd[0]=='v')   verbose = 2;		/** verbose **/
  else if( cmd[0]=='m')	  read_mask(spc);	/** mask **/
  else if( cmd[0]=='i')
  {	if( cmd[1]=='m')  read_img(spc);	/** image **/
	if( cmd[1]=='n')  invert_mask(spc);	/** invert **/
  }
  else if( cmd[0]=='r')	  read_region(spc);	/** region **/
  else if( cmd[0]=='w')
  {	if( cmd[1]=='m')  write_mask(spc);	/** wmask **/
	if( cmd[1]=='i')  write_image(spc);	/** wimage **/
  }
}


/*****************************************************************************/

int main( int argc, char **argv)
{
	char line[256];
	long i;

  printf("maskedit, compiled on " __DATE__
		 " ============================================\n\n");

  for( i=1;  i<argc;  ++i)	do_command(argv[i]);

  for(;;)	/** forever **/
  {
    if( verbose)
    { if(verbose>1)
        printf("\nMask: %s, Image: %s\n",
		MaskFits? (MaskFits->filename? MaskFits->filename : "(internal)") : "(none)",
		(ImgFits && ImgFits->filename) ? ImgFits->filename : "(none)" );
      printf("maskedit> ");
      fflush(stdout);
    }
    if( !fgets(line,255,stdin))  break;
    do_command(line);
  }
  return 0;
}
