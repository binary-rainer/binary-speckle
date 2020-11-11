/* $Id$
 * $Log$
 * Created:     Thu Aug  6 12:18:19 1998 by Koehler@cathy
 * Last change: Sun Jun 18 12:41:10 2000
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include "fit.h"
#include "fitsio.h"

extern char *VisFName, *DVisFName, *PhaFName, *DPhaFName, *MaskFName, *Title;
extern long rad_min, radius;
extern double VisFak, DVisScale, DPhaScale, amoeba_ftol;

char *EVisFName=NULL, *EPhaFName=NULL, *CSVisFNm=NULL, *CSPhaFNm=NULL,
     *fitten_out=NULL, *bff_out=NULL, *CSDVisFNm=NULL, *CSDPhaFNm=NULL;

/******************************************************************************/

static char *FindParaValue( char *start)
{
	char *str;

  if( !(str= strchr(start,'=')) && !(str= strchr(start,':')))	return NULL;

  if( !*++str)	return NULL;	/** nix hinterm '=' **/
  while( *str==' ')	str++;	/** skip spaces	**/
  return *str? str : NULL;
}

static char *DupParaValue(char *start)
{
	char *str;

  if((str= FindParaValue(start)))  str= strdup(str);
  return str;
}

static short ParaRange( struct Range *rng, char *start)
{
	double tmp;
	char *str;

  if( !(start= FindParaValue(start)))	return 1;

  rng->min= atof(start);
  if( !(str= strchr(start,' ')) && !(str= strchr(start,',')))	return 2;
  for( start= str+1;  *start==' ';  start++) ;	  if( !*start)	return 3;

  rng->max= atof(start);
  if( rng->min > rng->max)
  {	tmp= rng->min;	rng->min= rng->max;	rng->max= tmp;	}

  if( !(str= strchr(start,' ')) && !(str= strchr(start,',')))	return 4;
  for( start= str+1;  *start==' ';  start++) ;	  if( !*start)	return 5;

  if( (rng->npnts= atol(str)) < 2)	rng->npnts= 2;
  if( !(str= strchr(start,' ')) && !(str= strchr(start,',')))	return 6;
  for( start= str+1;  *start==' ';  start++) ;	  if( !*start)	return 7;

  if( (rng->prec= atof(start)) <= 0.0)
  {	fprintf(stderr," strange precision size %f,\a",rng->prec);
	rng->prec= rng->min==rng->max? 1.0 : (rng->max - rng->min) / 100.0;
	fprintf(stderr," using %f\n",rng->prec);
  }
  return 0;
}


static char *ExpandFName(char *input, char *pattern)
/** replaces '*' in pattern by name of input file **/
{
	char *expanded, *star, *dot;
	long len;

  len= strlen(pattern) + 2;
  if( (star= strchr(pattern,'*')))  len += strlen(input);

  if( (expanded= AllocBuffer(len,"filename")))
  { if( star)
    {	*star= 0;  strcpy(expanded,pattern);	*star= '*';
	strcat(expanded,input);
	if( (dot= strrchr(expanded,'.')))  *dot= 0;	/** remove extension **/
	strcat(expanded,star+1);
    }
    else strcpy(expanded,pattern);
  }
  return expanded;
}

static short ParaRangeC( int offs, char *start)
{
	int num;

  while( *start && *start>'@')  ++start;

  if((num= atol(start))) --num;	/** companion number given **/
	else num=0;		/** no comp no => asume 1. **/

  if( num<0 || num>=MAX_COMP)
  {	fprintf(stderr," bad companion number %d\n",num);  return 8;	}

  if( N_comp<=num)
  {	N_comp= num+1;	N_param= ANGLE1+3*N_comp;	}

  num *= 3;
  Ranges[offs+num]= Ranges[offs];	/** cp prec etc. **/
  return ParaRange( &Ranges[offs+num],start);
}

short ReadParaFile( char *fname)
{
	FILE *fp;
	short fp_remote;
	char line[260], *str;

  if( !(fp= rfopen(fname,"r")))
  {	fprintf(stderr," Can't read %s!\a\n",fname);	return 1;	}
  fp_remote= rfopen_pipe;

  if( verbose_level >= 1)  printf("\n Reading parameter file %s...\n\n",fname);

  while( !feof(fp))
  {	fgets(line,256,fp);
	if( (str= strchr(line,'\n')))  *str=0;
	if( line[0]==0 || line[0]=='#')	continue;	/** comment line **/

	if( !strncmp(line,"visib",5))	VisFName = DupParaValue(line);
   else if( !strncmp(line,"dvisib",6))	DVisFName= DupParaValue(line);
   else if( !strncmp(line,"phase",5))	PhaFName = DupParaValue(line);
   else if( !strncmp(line,"dphase",6))	DPhaFName= DupParaValue(line);
   else if( !strncmp(line,"mask",4))	MaskFName= DupParaValue(line);
   else if( !strncmp(line,"titl",4))	Title	 = DupParaValue(line);
   else if( !strncmp(line,"ce_a",4))	ParaRange( &Ranges[CE_ANGLE],line);
   else if( !strncmp(line,"ce_d",4))	ParaRange( &Ranges[CE_DIST],line);
   else if( !strncmp(line,"angle",5))	ParaRangeC( ANGLE1,line);
   else if( !strncmp(line,"dist", 4))	ParaRangeC( DIST1 ,line);
   else if( !strncmp(line,"brightn",7))	ParaRangeC( BRGTN1,line);
   else if( !strncmp(line,"rad",3) && (str= FindParaValue(line))) {
	radius = atoi(str);
	if((str= strchr(str,' '))) {	/** da kommt noch was, oder? **/
	  while( *str==' ')  str++;
	  if( *str) { rad_min= radius;	radius = atoi(str); }
	}
   }
   else if( !strncmp(line,"ftol",4))	amoeba_ftol= atof(FindParaValue(line));
   else if( !strncmp(line,"RefIsMult",9)) RefIsMult= 1;
   else if( !strncmp(line,"fit_out",7))  fitten_out= ExpandFName(fname,FindParaValue(line));
   else if( !strncmp(line,"bff_out",7))  bff_out   = ExpandFName(fname,FindParaValue(line));
   else if( !strncmp(line,"err_vis",7))  EVisFName = ExpandFName(fname,FindParaValue(line));
   else if( !strncmp(line,"err_pha",7))  EPhaFName = ExpandFName(fname,FindParaValue(line));
   else if( !strncmp(line,"csub_vis",8)) CSVisFNm  = ExpandFName(fname,FindParaValue(line));
   else if( !strncmp(line,"csub_pha",8)) CSPhaFNm  = ExpandFName(fname,FindParaValue(line));
   else if( !strncmp(line,"csub_dvis",9)) CSDVisFNm= ExpandFName(fname,FindParaValue(line));
   else if( !strncmp(line,"csub_dpha",9)) CSDPhaFNm= ExpandFName(fname,FindParaValue(line));
   else if( !strncmp(line,"dvisscl",7))
	{	if( (DVisScale= atof(FindParaValue(line)))==0.0)  DVisScale= 1.0; }
   else if( !strncmp(line,"dphascl",7))
	{	if( (DPhaScale= atof(FindParaValue(line)))==0.0)  DPhaScale= 1.0; }
   else fprintf(stderr," unknown parameter: %s\a\n",line);

	if( ferror(fp))
	{	fp_remote? pclose(fp):fclose(fp);	return 2;	}
  }
  fp_remote? pclose(fp):fclose(fp);
  if( !VisFName && !PhaFName) {	fprintf(stderr," No data given!\a\n");	return 5; }
  return 0;
}

/******************************************************************************/

static char *ce_range_name[2]= { "ce_ang ", "ce_dist" },
	    *co_range_name[3]= { "angle", "dist", "brightn" };

static void PrintFileLine(FILE *fp, char *what, char *fname)
{
	if(fname) fprintf(fp,"%-8s= %s\n",what,fname );
	else	  fprintf(fp,"#%-8s= none\n",what );
}

void WriteParaFile(short all_done)
{
	FILE *outfp;
	long  i;
	short fp_remote;

  if( !(outfp= rfopen(bff_out,"w")))
  {	fprintf(stderr," Can't open %s\a\n",bff_out);	}
  else
  {	fp_remote= rfopen_pipe;
	if( verbose_level >= 1)
		fprintf(stderr," Writing bff-file %s...\n",bff_out);

	fprintf(outfp,"#\n# %s	created by bff on "__DATE__"\n#\n",bff_out);

	PrintFileLine(outfp,"visib" ,VisFName );
	PrintFileLine(outfp,"dvisib",DVisFName);
	if( DVisScale!=1.0)
			fprintf(outfp,"dvisscl= %f\n", DVisScale);

	PrintFileLine(outfp,"phase" ,PhaFName );
	PrintFileLine(outfp,"dphase",DPhaFName);
	if( DPhaScale!=1.0)
			fprintf(outfp,"dphascl= %f\n",DPhaScale);

	PrintFileLine(outfp,"mask", MaskFName);
	PrintFileLine(outfp,"title",Title);

	fprintf(outfp,"\n");
	for( i=0;  i<=CE_DIST;  ++i)
		fprintf(outfp,"%-7s= %f %f %d %f\n", ce_range_name[i],
			Ranges[i].min, Ranges[i].max, Ranges[i].npnts, Ranges[i].prec);

	for( i=ANGLE1;  i<N_param;  ++i)
		fprintf(outfp,"%-7s%1ld = %f %f %d %f\n",
			co_range_name[(i-ANGLE1)%3], (i-ANGLE1+3)/3,
			Ranges[i].min, Ranges[i].max, Ranges[i].npnts, Ranges[i].prec);

	fprintf(outfp,"radius = %ld %ld\n\n",rad_min,radius);
	if(amoeba_ftol > 0.0)
			fprintf(outfp,"ftol   = %g\n\n", amoeba_ftol);
	if(RefIsMult)	fprintf(outfp,"RefIsMult\n\n");

	fprintf(outfp,"bff_out= %s+\n", bff_out);
	if(fitten_out)	fprintf(outfp,"fit_out= %s\n",fitten_out);
	if( EVisFName)	fprintf(outfp,"err_vis= %s\n",EVisFName);
	if( EPhaFName)	fprintf(outfp,"err_pha= %s\n",EPhaFName);
	if( CSVisFNm)	fprintf(outfp,"csub_vis= %s\n",CSVisFNm);
	if( CSPhaFNm)	fprintf(outfp,"csub_pha= %s\n",CSPhaFNm);
	if( CSDVisFNm)	fprintf(outfp,"csub_dvis= %s\n",CSDVisFNm);
	if( CSDPhaFNm)	fprintf(outfp,"csub_dpha= %s\n",CSDPhaFNm);
	if(all_done)	fprintf(outfp,"\n# Fit converged\n");
	fp_remote? pclose(outfp) : fclose(outfp);
  }
}
