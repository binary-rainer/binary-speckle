/* $Id: binplotGUI.c,v 1.2 2002/05/12 08:11:04 koehler Exp koehler $
 * $Log: binplotGUI.c,v $
 * Revision 1.2  2002/05/12 08:11:04  koehler
 * Change tcl-C interaction to "official" method:
 * The C-program is an extended wish that gets the name of the
 * script as first argument.
 *
 * Revision 1.1  1997/08/02  13:03:22  koehler
 * Initial revision
 *
 * Created:     Sat May 24 17:29:22 1997 by Koehler@Lisa
 * Last change: Sun Aug  4 15:19:44 2002
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include "binplot.h"
#ifdef __sun
# include <ieeefp.h>	/** for finite() **/
#endif

/*#define DEBUG 1*/

extern int SaveCompSub(int, char **), SaveResiduals(int, char **);
extern double FindVisFak(double *VisBuf, double *DVisBuf, double *MVisBuf,
			 short *MaskBuf, long naxis1, long naxis2, long rmin, long rmax);
void FindBestCenter(double *PhaBuf, double *DPhaBuf,
		    double *MPhaBuf, short *MaskBuf, long sizex, long sizey, long rmax);

Tcl_Interp *tclip;

short Initializing;	/** To prevent plotting when scales are initialized **/

/*****************************************************************************/

char *SetDoubleVar(char *fmt, double var, char *tcl_name)
{
	char line[42];

  sprintf(line,fmt,var);
#ifdef DEBUG
  fprintf(stderr,"Setting %s to %s\n",tcl_name,line);
#endif
  return Tcl_SetVar(tclip,tcl_name,line,TCL_GLOBAL_ONLY);
}

char *GetDoubleVar(char *tcl_name, int flags, double *varp)
{
	char *ptr;

  if( (ptr= Tcl_GetVar(tclip,tcl_name,flags)))  *varp= atof(ptr);
  return ptr;
}


void UpdateTclVars(void)
{
	char line[88];

  Tcl_SetVar(tclip,"title",Title? Title:"", TCL_GLOBAL_ONLY);
  Tcl_SetVar(tclip,"visib", VisFits?  VisFits->filename:"", TCL_GLOBAL_ONLY);
  Tcl_SetVar(tclip,"dvisi",DVisFits? DVisFits->filename:"", TCL_GLOBAL_ONLY);
  Tcl_SetVar(tclip,"phase", PhaFits?  PhaFits->filename:"", TCL_GLOBAL_ONLY);
  Tcl_SetVar(tclip,"dphas",DPhaFits? DPhaFits->filename:"", TCL_GLOBAL_ONLY);
  Tcl_SetVar(tclip,"mask", MaskFits? MaskFits->filename:"", TCL_GLOBAL_ONLY);

  sprintf(line,"%.0f",pangle);	Tcl_SetVar(tclip,"proja",line,TCL_GLOBAL_ONLY);
  sprintf(line,"%d",rmin);	Tcl_SetVar(tclip,"rmin", line,TCL_GLOBAL_ONLY);
  sprintf(line,"%d",rmax);	Tcl_SetVar(tclip,"rmax", line,TCL_GLOBAL_ONLY);
  sprintf(line,"%d",rfit);	Tcl_SetVar(tclip,"fitrad",line,TCL_GLOBAL_ONLY);
  sprintf(line,"%d",stripw);	Tcl_SetVar(tclip,"stripw",line,TCL_GLOBAL_ONLY);

  SetDoubleVar("%.2f",Modell[ANGLE1],"co1a");
  SetDoubleVar("%.2f",Modell[DIST1] ,"co1s");
  SetDoubleVar("%f",  Modell[BRGTN1],"co1r");

  SetDoubleVar("%.2f",Modell[ANGLE1+3],"co2a");
  SetDoubleVar("%.2f",Modell[DIST1 +3],"co2s");
  SetDoubleVar("%f",  Modell[BRGTN1+3],"co2r");

  SetDoubleVar("%.2f",Modell[CE_ANGLE],"cnta");
  SetDoubleVar("%.2f",Modell[CE_DIST], "cntd");
  SetDoubleVar("%f",  visfak, "visfak");
}


/*****************************************************************************
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
  if(argc < 2) {
	Tcl_AppendResult(interp, "Usage: pgopen device [nxsub nysub]",NULL);
	return TCL_ERROR;
  }
  id = cpgopen(argv[1]);  /* Attempt to open the PGPLOT device specified in argv[1]. */
  if(id <= 0) {
    Tcl_AppendResult(interp, "Unable to open device: ", argv[1], NULL);
    return TCL_ERROR;
  };
  if(argc > 2) {
    int nxsub= atoi(argv[2]);
    int nysub= (argc > 3) ? atoi(argv[3]) : 1;

    cpgsubp( nxsub, nysub);	/** should we barf if n?sub == 0? **/
  }
  cpgask(0);	/** Turn off new-page prompting. **/
  cpgscf(1);	/** set character font (1=normal)**/
 /*
  * Return the PGPLOT id of the device via the Tcl result string.
  */
  sprintf(result, "%d", id);
  Tcl_AppendResult(interp, result, NULL);
  return TCL_OK;
}


/********************************* Custom Commands *********************************/

static void NewFitsFile(char *tcl_name, char **namep, struct FitsFile **ffp)
{	char *new;

  if( (new= Tcl_GetVar(tclip,tcl_name,TCL_GLOBAL_ONLY)) && (new= FilenamePlus(new)))
  {
#ifdef DEBUG
	fprintf(stderr,"new filename is %s\n",new);
#endif
	if( *new==0)	new= NULL;
	if( ReadFrame(new, ffp)==0)
	{	if( *namep)	free(*namep);
		*namep= new;
	}
  }
}


int NewData(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  if(argc<2)	return TCL_ERROR;

  if(strcmp(argv[1],"visib")==0)  NewFitsFile("visib",&VisName, &VisFits );
  if(strcmp(argv[1],"dvisi")==0)  NewFitsFile("dvisi",&DVisName,&DVisFits);
  if(strcmp(argv[1],"phase")==0)  NewFitsFile("phase",&PhaName, &PhaFits );
  if(strcmp(argv[1],"dphas")==0)  NewFitsFile("dphas",&DPhaName,&DPhaFits);
  if(strcmp(argv[1],"mask" )==0)  NewFitsFile("mask" ,&MaskName,&MaskFits);

  UpdateTclVars();
  FT_Modell( VisModel.Buffer.d, PhaModel.Buffer.d, naxis1,naxis2,rmax);
  Plot();
  return TCL_OK;
}


int VisRange(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{	char *minp, *maxp;

  minp= GetDoubleVar("vismin",TCL_GLOBAL_ONLY,&vismin);
  maxp= GetDoubleVar("vismax",TCL_GLOBAL_ONLY,&vismax);

  if( minp==NULL || *minp==0)  vismin= -DBL_MAX;
  if( maxp==NULL || *maxp==0)  vismax=  DBL_MAX;

  Plot();
  return TCL_OK;
}

int PhaRange(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{	char *minp, *maxp;

  minp= GetDoubleVar("phamin",TCL_GLOBAL_ONLY,&phamin);
  maxp= GetDoubleVar("phamax",TCL_GLOBAL_ONLY,&phamax);

  if( minp==NULL || *minp==0)  phamin= -DBL_MAX;
  if( maxp==NULL || *maxp==0)  phamax=  DBL_MAX;

  Plot();
  return TCL_OK;
}


int NewProj(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{	char *ptr;

  if( !Initializing)
  {
    if( (ptr= Tcl_GetVar(tclipp,"proja",TCL_GLOBAL_ONLY)))  pangle= atoangle(ptr);
    if( (ptr= Tcl_GetVar(tclipp,"rmin", TCL_GLOBAL_ONLY)))  rmin = atol(ptr);
    if( (ptr= Tcl_GetVar(tclipp,"rmax", TCL_GLOBAL_ONLY)))
    {	rmax = atol(ptr);
	if( rmax > naxis1 * naxis2 / 4)
	{	char line[42];

	  rmax= naxis1 * naxis2 / 4;
	  fprintf(stderr," Warning: rmax reset to %d\a\n",rmax);
	  sprintf(line,"%d",rmax);	Tcl_SetVar(tclip,"rmax",line,TCL_GLOBAL_ONLY);
	}
    }
    if( (ptr= Tcl_GetVar(tclipp,"stripw",TCL_GLOBAL_ONLY)))  stripw= atol(ptr);
    if( (ptr= Tcl_GetVar(tclipp,"visfak",TCL_GLOBAL_ONLY)))  visfak= atof(ptr);

    Tcl_Eval(tclipp,"adj_proj_dials");
    Plot();
  }
  return TCL_OK;
}


static void GetPAdVars(char *PA_name, double *PA_var, char *d_name, double *d_var)
{
	char *ptr;

  if( (ptr= Tcl_GetVar(tclip,PA_name,TCL_GLOBAL_ONLY)))  *PA_var= atoangle(ptr);
  if( GetDoubleVar( d_name,TCL_GLOBAL_ONLY, d_var))
  { if( *d_var < 0.0)
    {	*d_var = -*d_var;
	if( (*PA_var += 180.0) >= 360.0)  *PA_var -= 360.0;
	SetDoubleVar("%.2f",*PA_var,PA_name);
	SetDoubleVar("%.2f", *d_var, d_name);
    }
  }
}


int NewModel(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
	char line[42], *ptr;

  if( !Initializing)
  {
    GetPAdVars( "co1a",&Modell[ANGLE1], "co1s",&Modell[DIST1]);
    GetDoubleVar("co1r",TCL_GLOBAL_ONLY, &Modell[BRGTN1]);

    GetPAdVars( "co2a",&Modell[ANGLE1+3],"co2s",&Modell[DIST1+3]);
    GetDoubleVar("co2r",TCL_GLOBAL_ONLY, &Modell[BRGTN1+3]);
    N_comp= (Modell[BRGTN1+3] > 0.0)? 2 : 1;

    GetPAdVars( "cnta",&Modell[CE_ANGLE],"cntd",&Modell[CE_DIST]);

    if( (ptr= Tcl_GetVar(tclipp,"RefIsMult",TCL_GLOBAL_ONLY)))	RefIsMult= atoi(ptr);

    FT_Modell( VisModel.Buffer.d, PhaModel.Buffer.d, naxis1,naxis2,rmax);
    Plot();

    sprintf(line,"%d",rmax);	Tcl_SetVar(tclip,"rmodel", line,TCL_GLOBAL_ONLY);
  }
  return TCL_OK;
}


int ReadFitCmd(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  if(argc<2)	return TCL_ERROR;
  ReadFitFile(argv[1]);
  UpdateTclVars();
  FT_Modell( VisModel.Buffer.d, PhaModel.Buffer.d, naxis1,naxis2,rmax);
  Plot();
  return TCL_OK;
}


int WriteBffCmd(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  if(argc<2)	return TCL_ERROR;
  WriteBff(argv[1]);
  return TCL_OK;
}


int HardcopyCmd(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  if(argc<2)	return TCL_ERROR;
  Hardcopy(argv[1]);
  Plot();
  return TCL_OK;
}

int WriteDataCmd(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  WriteDatafiles();
  return TCL_OK;
}

int SaveCSCmd(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  if(argc<2)	return TCL_ERROR;
  SaveCompSub(argc,argv);
  return TCL_OK;
}

int SaveResCmd(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  if(argc<2)	return TCL_ERROR;
  SaveResiduals(argc,argv);
  return TCL_OK;
}

int Replot(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  cpgpage();
  Plot();
  return TCL_OK;
}


int Vis2PGM(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
	FILE	*fp;
	double	*buffy;
	long	x,y, rmaxq= rmax*rmax;
	long	halfx= VisFits->n1/2, halfy= VisFits->n2/2;
	double	val,min,max;

  if(argc<2)	return TCL_ERROR;

  if( !(fp= fopen(argv[1],"w")))  return TCL_ERROR;

  /** default is to write visibility data, but we can write the model, too **/

  buffy = (argc > 2) ? VisModel.Buffer.d : VisFits->Buffer.d;

  min= DBL_MAX;	 max= -DBL_MAX;
  for( y= -halfy;  y<halfy;  ++y)
    for( x= -halfx;  x<halfx;  ++x)
      if( x*x + y*y < rmaxq)
      {	val= buffy[x+halfx + VisFits->n1*(y+halfy)];
	if( finite(val))
	{	if( min > val)	 min= val;
		if( max < val)	 max= val;
	}
      }
  if(vismin >-DBL_MAX)	min= vismin;
  if(vismax < DBL_MAX)	max= vismax;

  fprintf(fp,"P5\n%d %d 255\n",VisFits->n1,VisFits->n2);

  for( y=VisFits->n2-1;  y>=0;  --y)
    for( x=0;  x<VisFits->n1;  ++x)
	fprintf(fp,"%c",
	    (int)((buffy[x+VisFits->n1*y]-min) / (max-min)*255));

  fclose(fp);
  return TCL_OK;
}


int Estimate(ClientData cd, Tcl_Interp *tclipp, int argc, char *argv[])
{
  if(argc<2)	return TCL_ERROR;

  if(strcmp(argv[1],"center")==0)
  {
	if( PhaFits == NULL)
	{	Tcl_AppendResult(tclipp,"cannot estimate center without phase-data",NULL);
		return TCL_ERROR;
	}
#if 0
	Modell[CE_DIST]= 0.0;	/** erstmal in die Mitte **/
	FT_Modell( VisModel.Buffer.d, PhaModel.Buffer.d, naxis1,naxis2,rmax);
#endif
	FindBestCenter( PhaFits->Buffer.d, DPhaFits? DPhaFits->Buffer.d : NULL,
			PhaModel.Buffer.d, MaskFits? MaskFits->Buffer.s : NULL,
			naxis1, naxis2, rmax);
	SetDoubleVar("%.2f",Modell[CE_ANGLE],"cnta");
	SetDoubleVar("%.2f",Modell[CE_DIST], "cntd");
  }
  else if( VisFits == NULL)
  {
	Tcl_AppendResult(tclipp,"cannot estimate anything without visibility-data",NULL);
	return TCL_ERROR;
  }
  else if(strcmp(argv[1],"visfak")==0)
  {	visfak=
	    FindVisFak( VisFits->Buffer.d, DVisFits? DVisFits->Buffer.d : NULL,
			VisModel.Buffer.d, MaskFits? MaskFits->Buffer.s : NULL,
			naxis1, naxis2, rmin, rmax);
	SetDoubleVar("%f",  visfak, "visfak");
  }
  else if(strcmp(argv[1],"angle")==0)
  {
		double Dx, Dy, SumZ, SumN;
		long x,y,xm,ym, rmaxq= rmax*rmax;

	SumZ= SumN= 0.0;
	for( x=0;  x<naxis1;  ++x)
	{	xm= x-naxis1/2;

	    for( y=0;  y<naxis2;  ++y)
	    {	ym= y-naxis2/2;
		if( xm*xm+ym*ym<rmaxq)
		{
		  Dx= VisFits->Buffer.d[x+1 + naxis1*y] - VisFits->Buffer.d[x-1 + naxis1*y];
		  Dy= VisFits->Buffer.d[x+naxis1*(y+1)] - VisFits->Buffer.d[x+naxis1*(y-1)];

		  SumZ += Dx * Dy;
		  SumN += Dx*Dx - Dy*Dy;
		}
	    }
	}
	if((Modell[ANGLE1]= DEG(atan2( 2*SumZ,SumN) / 2.0)) < 0.0)  Modell[ANGLE1] += 360.0;
	SetDoubleVar("%.2f",Modell[ANGLE1],"co1a");
  }
  else if(strcmp(argv[1],"bratio")==0)
  {	extern void EstimSep_BRatio(void);

	EstimSep_BRatio();
	SetDoubleVar("%.2f",Modell[DIST1] ,"co1s");
	SetDoubleVar("%f",  Modell[BRGTN1],"co1r");
  }
  else
  {	Tcl_AppendResult(tclipp,"don't know how to estimate ",argv[1],NULL);
	return TCL_ERROR;
  }
  FT_Modell( VisModel.Buffer.d, PhaModel.Buffer.d, naxis1,naxis2,rmax);
  Plot();
  return TCL_OK;
}


/********************************* main Init *********************************/

short InitGUI(void)
{
  Initializing= 1;

  /********************** Commands **********************/
  Tcl_CreateCommand(tclip,"pgopen",   tcl_pgopen,NULL, 0);
  Tcl_CreateCommand(tclip,"new_data", NewData,	NULL,NULL);
  Tcl_CreateCommand(tclip,"visrange", VisRange,	NULL,NULL);
  Tcl_CreateCommand(tclip,"pharange", PhaRange,	NULL,NULL);
  Tcl_CreateCommand(tclip,"new_proj", NewProj,	NULL,NULL);
  Tcl_CreateCommand(tclip,"mk_new_model",NewModel,NULL,NULL);
  Tcl_CreateCommand(tclip,"fitfile",  ReadFitCmd,NULL,NULL);
  Tcl_CreateCommand(tclip,"write_bff",WriteBffCmd,NULL,NULL);
  Tcl_CreateCommand(tclip,"hardcopy", HardcopyCmd,NULL,NULL);
  Tcl_CreateCommand(tclip,"writedata",WriteDataCmd,NULL,NULL);
  Tcl_CreateCommand(tclip,"save_cs",  SaveCSCmd, NULL,NULL);
  Tcl_CreateCommand(tclip,"save_resi",SaveResCmd,NULL,NULL);
  Tcl_CreateCommand(tclip,"replot",   Replot,	NULL,NULL);
  Tcl_CreateCommand(tclip,"vistopgm", Vis2PGM,	NULL,NULL);
  Tcl_CreateCommand(tclip,"estimate", Estimate,	NULL,NULL);
  UpdateTclVars();

  Initializing= 0;
  return 0;
}
