/* Created:     Sun Nov 26 16:54:55 2000 by Koehler@cathy
 * Last change: Tue Dec  5 18:01:33 2000
 *
 * Routines...err...borrowed from ptcl
 *
 * Some of these routines follow not even the simplest rules to avoid crashes.
 * I'll have to fix this some time.  Sigh...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "fitsio.h"
#include "tcl.h"
#include "cpgplot.h"


#define NUM_BIN_PGPLOT 10000   /* maximum number of bins permitted by PGPLOT */
#define NUM_DATA_PGPLOT 100000 /* maximum number of data points permitted by PGPLOT */


/*****************************************************************************/

int Tcl_GetFloat( Tcl_Interp *tclip, char *str, float *pflt )
{
	int res;
	double dbl;

  res= Tcl_GetDouble( tclip, str, &dbl);
  *pflt= (res == TCL_OK) ? (float)dbl : 0.0;
  return res;
}

/******************************************************************************/
/* Given a tcl-list, converts all elements to float and returns them as array */

int Tcl_GetFloatList( Tcl_Interp *tclip, char *liststr, int *sizep, float **arrayp)
{
	int  i, res;
	char **tcllist;
	float *array;

  if((res= Tcl_SplitList( tclip, liststr, sizep, &tcllist)) != TCL_OK)  return res;

  if((array= malloc( (*sizep) * sizeof(float))) == NULL)
  {	Tcl_Free((char *)tcllist);
	Tcl_SetResult(tclip,"Out of memory",TCL_STATIC);
	return TCL_ERROR;
  }
  *arrayp = array;

  for( i=0;  i<*sizep;  ++i)
  {
	res= Tcl_GetFloat( tclip, tcllist[i], &array[i]);
	if( res != TCL_OK)
	{	*arrayp= NULL;	free(array);	break;	}
  }
  Tcl_Free((char*)tcllist);
  return res;
}


/*****************************************************************************/
/* returns minimum (cdata==NULL) or maximum (cdata!=NULL) of list	*/

int Tcl_minmax( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int i, numdata, res;
	double val,minmax;
	char **listp, buf[42];

  if(argc != 2)
  {	Tcl_AppendResult(tclip,"Wrong number of arguments - should be \'",
			 argv[0], " list\'", NULL);
	return TCL_ERROR;
  }

  if((res= Tcl_SplitList( tclip, argv[1], &numdata, &listp)) != TCL_OK)
	return TCL_ERROR;
  /** Tcl_SplitList set result if an error occured **/

  if( (res= Tcl_GetDouble( tclip, listp[0], &minmax)) == TCL_OK)
  { for( i= 0;  i<numdata;  ++i)
    {
	if((res= Tcl_GetDouble( tclip, listp[i], &val)) != TCL_OK)  break;
	if(cdata)
	{	if( minmax < val)  minmax= val; 	}
	else
	{	if( minmax > val)  minmax= val; 	}
    }
  }
  Tcl_Free((char *)listp);

  if( res==TCL_OK)
  {	sprintf(buf,"%f",minmax);	Tcl_SetResult(tclip,buf,TCL_VOLATILE);	}
  else
  {	Tcl_AppendResult(tclip,argv[0],": Invalid element in data vector",NULL); }

  return res;
}


/*****************************************************************************/

int Tcl_pgbin( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res, numbins, sizex, sizey;
	float *array_x, *array_y;
	Logical center;		/** typedef'ed to int in cpgplot.h **/


  if(argc != 5)
  {	Tcl_AppendResult(tclip,"Wrong number of arguments - should be \'",
			 argv[0], " nbin x data center\'",NULL);
	return TCL_ERROR;
  }
  /*
   * 1.arg: number of bins
   */
  if((res= Tcl_GetInt( tclip, argv[1], &numbins)) != TCL_OK)  return res;
  if( (numbins < 1) || (numbins > NUM_BIN_PGPLOT))
  {	Tcl_AppendResult(tclip,argv[0],": Invalid number of bins",NULL);
	return TCL_ERROR;
  }
  /*
   * 2.arg: list of x values
   */
  if((res= Tcl_GetFloatList( tclip, argv[2], &sizex, &array_x)) != TCL_OK)  return res;
  if( numbins != sizex )
  {	Tcl_AppendResult(tclip,
		     argv[0],": number of bins not equal to number of x values",NULL);
	free(array_x);
	return TCL_ERROR;
  }
  /*
   * 3.arg: list of y values
   */
  if((res= Tcl_GetFloatList( tclip, argv[3], &sizey, &array_y)) != TCL_OK)
  {	free(array_x);	return res;	}

  if( numbins != sizey )
  {	Tcl_AppendResult(tclip,
		     argv[0],": number if bins not equal to number of data points",NULL);
	free(array_x);
	free(array_y);
	return TCL_ERROR;
  }
  /*
   * 4.arg: center or not
   */
  if((res= Tcl_GetBoolean( tclip, argv[4], &center)) != TCL_OK)
  {	free(array_x);	free(array_y);	return res;	}

  cpgbin(numbins, array_x, array_y, center);

  free(array_x);
  free(array_y);
  return TCL_OK;
}

/*****************************************************************************/

int Tcl_pgbox( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res, nxsub, nysub;
	float xtick, ytick;


  if(argc != 7)
  {	Tcl_AppendResult(tclip,"Wrong number of arguments - should be \'",
			 argv[0]," xopt xtick nxsub yopt ytick nysub\'",NULL);
	return TCL_ERROR;
  }
  /*
   * 1.arg: char *xopt - no need to convert
   * 2.arg: float xtick
   */
  if(((res= Tcl_GetFloat( tclip, argv[2], &xtick)) != TCL_OK) || xtick < 0.0)
  {
	Tcl_AppendResult(tclip,argv[0],": Invalid x-axis tick interval",NULL);
	return TCL_ERROR;
  }
  /*
   * 3.arg: int nxsub
   */
  if(((res= Tcl_GetInt( tclip, argv[3], &nxsub)) != TCL_OK) || nxsub < 0)
  {
	Tcl_AppendResult(tclip,argv[0],": Invalid x-axis number of subintervals",NULL);
	return TCL_ERROR;
  }
  /*
   * 4.arg: char *yopt - nothing to convert
   * 5.arg: float ytick
   */
  if(((res= Tcl_GetFloat( tclip, argv[5], &ytick)) != TCL_OK) || ytick < 0.0)
  {
	Tcl_AppendResult(tclip,argv[0],": Invalid y-axis tick interval",NULL);
	return TCL_ERROR;
  }
  /*
   * 6.arg: int nysub
   */
  if(((res= Tcl_GetInt( tclip, argv[6], &nysub)) != TCL_OK) || nysub < 0)
  {
	Tcl_AppendResult(tclip,argv[0],": Invalid y-axis number of subintervals",NULL);
	return TCL_ERROR;
  }

  cpgbox( argv[1], xtick, nxsub, argv[4], ytick, nysub);
  return( TCL_OK );
}


/*****************************************************************************/

int Tcl_pgclos( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
  if( argc != 1 )
  {	Tcl_AppendResult(tclip,
			 "Wrong number of arguments - should be \'",argv[0],"\'",NULL);
	return TCL_ERROR;
  }
  cpgclos();
  return TCL_OK;
}


/*****************************************************************************/

int Tcl_pgeras( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
  if( argc != 1 )
  {	Tcl_AppendResult(tclip,
			 "Wrong number of arguments - should be \'",argv[0],"\'",NULL);
	return TCL_ERROR;
  }
  cpgeras( );
  return TCL_OK;
}


/*****************************************************************************/

int Tcl_pglab( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
  if( argc != 4 )
  {	Tcl_AppendResult(tclip,"Wrong number of arguments - should be \'",
			 argv[0]," xlabel ylabel toplabel\'",NULL);
	return TCL_ERROR;
  }
  cpglab( argv[1], argv[2], argv[3] );
  return TCL_OK;
}


/*****************************************************************************/

int Tcl_pgline( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res, num, sizex, sizey;
	float *array_x, *array_y;

  if( argc != 4 )
  {	Tcl_AppendResult(tclip,"Wrong number of arguments - should be \'",
			 argv[0]," n xpoints ypoints\'",NULL);
	return TCL_ERROR;
  }
  /*
   * 1.arg: number of points
   */
  if((res= Tcl_GetInt( tclip, argv[1], &num)) != TCL_OK)  return res;
  if( num < 1 || num > NUM_DATA_PGPLOT)
  {
	Tcl_AppendResult(tclip,argv[0],": Invalid number of data points",NULL);
	return TCL_ERROR;
  }
  /*
   * 2.arg: list of x data
   */
  if((res= Tcl_GetFloatList( tclip, argv[2], &sizex, &array_x)) != TCL_OK)  return res;
  if( num != sizex )
  {	Tcl_AppendResult(tclip,
		     argv[0],": number of points not equal to number of x values",NULL);
	free(array_x);
	return TCL_ERROR;
  }
  /*
   * 3.arg: list of y values
   */
  if((res= Tcl_GetFloatList( tclip, argv[3], &sizey, &array_y)) != TCL_OK)
  {	free(array_x);	return res;	}

  if( num != sizey )
  {	Tcl_AppendResult(tclip,
		     argv[0],": number of points not equal to number of y values",NULL);
	free(array_x);
	free(array_y);
	return TCL_ERROR;
  }

  cpgline( num, array_x, array_y);
  free(array_x);
  free(array_y);
  return TCL_OK;
}


/*****************************************************************************/

int Tcl_pgopen( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res;
	char buff[20];

  if(argc != 2)
  {	Tcl_AppendResult(tclip,"Invalid number of arguments - should be \'",
			 argv[0]," device\'",NULL);
	return TCL_ERROR;
  }
  if((res= cpgopen(argv[1])) == 0)
  {	Tcl_AppendResult(tclip,argv[0],": could not open device ",argv[1],NULL);
	return TCL_ERROR;
  }
  sprintf(buff, "%d", res);
  Tcl_SetResult(tclip,buff,TCL_VOLATILE);
  return TCL_OK;
}


/*****************************************************************************/

int Tcl_pgpt( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res, num, sizex, sizey, symbol;
	float *array_x, *array_y;

  if( argc != 5 )
  {	Tcl_AppendResult(tclip,"Wrong number of arguments - should be \'",
			 argv[0]," n xpoints ypoints symbol\'",NULL);
	return TCL_ERROR;
  }
  /*
   * 1.arg: number of points
   */
  if((res= Tcl_GetInt( tclip, argv[1], &num)) != TCL_OK)  return res;
  if( num < 1 || num > NUM_DATA_PGPLOT)
  {
	Tcl_AppendResult(tclip,argv[0],": Invalid number of data points",NULL);
	return TCL_ERROR;
  }
  /*
   * 2.arg: list of x data
   */
  if((res= Tcl_GetFloatList( tclip, argv[2], &sizex, &array_x)) != TCL_OK)  return res;
  if( num != sizex )
  {	Tcl_AppendResult(tclip,
		     argv[0],": number of points not equal to number of x values",NULL);
	free(array_x);
	return TCL_ERROR;
  }
  /*
   * 3.arg: list of y values
   */
  if((res= Tcl_GetFloatList( tclip, argv[3], &sizey, &array_y)) != TCL_OK)
  {	free(array_x);	return res;	}

  if( num != sizey )
  {	Tcl_AppendResult(tclip,
		     argv[0],": number of points not equal to number of y values",NULL);
	free(array_x);
	free(array_y);
	return TCL_ERROR;
  }
  /*
   * 4.arg: symbol
   */
  if((res= Tcl_GetInt( tclip, argv[4], &symbol)) == TCL_OK)
  { if( symbol < -1 || symbol > 127)
    {	Tcl_AppendResult(tclip,argv[0],": Invalid PGPLOT symbol",NULL);
	res= TCL_ERROR;
    }
    else
    {	cpgpt(num, array_x, array_y, symbol);	res= TCL_OK;	}
  }
  free(array_x);
  free(array_y);
  return res;
}


/*****************************************************************************/

int Tcl_pgscf( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res, font;

  if(argc != 2)
  {	Tcl_AppendResult(tclip,
		     "Wrong number of arguments - should be \'",argv[0]," font\'",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetInt( tclip, argv[1], &font)) != TCL_OK || font < 1 || font > 4)
  {
	Tcl_AppendResult(tclip,argv[0],": Invalid font index",NULL);
	return TCL_ERROR;
  }
  cpgscf(font);
  return TCL_OK;
}


/*****************************************************************************/

int Tcl_pgsch( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res;
	float hgt;

  if(argc != 2)
  {	Tcl_AppendResult(tclip,
		     "Wrong number of arguments - should be \'",argv[0]," char-height\'",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetFloat( tclip, argv[1], &hgt)) != TCL_OK || hgt < 0.)
  {
	Tcl_AppendResult(tclip,argv[0],": Invalid height",NULL);
	return TCL_ERROR;
  }
  cpgsch(hgt);
  return TCL_OK;

}

/*****************************************************************************/

int Tcl_pgsci( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res, ci;

  if(argc != 2)
  {	Tcl_AppendResult(tclip,
		     "Wrong number of arguments - should be \'",argv[0]," color\'",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetInt( tclip, argv[1], &ci)) != TCL_OK)  return res;

  cpgsci(ci);
  return TCL_OK;
}


/*****************************************************************************/

int Tcl_pgslct( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res, id;

  if(argc != 2)
  {	Tcl_AppendResult(tclip,
		     argv[0],": Invalid number of arguments",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetInt( tclip, argv[1], &id)) != TCL_OK || id < 1)
  {	Tcl_AppendResult(tclip,
		     argv[0],": Invalid graphics device ID number",NULL);
	return TCL_ERROR;
  }

  cpgslct(id);
  return TCL_OK;
}

/*****************************************************************************/

int Tcl_pgsvp( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res;
	float xleft, xright, ybot, ytop;

  if( argc != 5 )
  {	Tcl_AppendResult(tclip,"Wrong number of arguments - should be \'",
			 argv[0]," xleft xright ybot ytop\'",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetFloat( tclip, argv[1], &xleft)) != TCL_OK || xleft < 0.0 || xleft > 1.0)
  {
	Tcl_AppendResult(tclip, argv[0],": Invalid left x coordinate",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetFloat( tclip, argv[2], &xright)) != TCL_OK || xright < 0.0 || xright > 1.0)
  {
	Tcl_AppendResult(tclip, argv[0],": Invalid right x coordinate",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetFloat( tclip, argv[3], &ybot)) != TCL_OK || ybot < 0.0 || ybot > 1.0)
  {
	Tcl_AppendResult(tclip, argv[0],": Invalid bottom y coordinate",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetFloat( tclip, argv[4], &ytop)) != TCL_OK || ytop < 0.0 || ytop > 1.0)
  {
	Tcl_AppendResult(tclip, argv[0],": Invalid top y coordinate",NULL);
	return TCL_ERROR;
  }
  cpgsvp(xleft,xright,ybot,ytop);
  return TCL_OK;
}


/*****************************************************************************/

int Tcl_pgswin( ClientData cdata, Tcl_Interp *tclip, int argc, char **argv)
{
	int res;
	float xleft, xright, ybot, ytop;

  if( argc != 5 )
  {	Tcl_AppendResult(tclip,"Wrong number of arguments - should be \'",
			 argv[0]," xleft xright ybot ytop\'",NULL);
	return TCL_ERROR;
  }
  if((res = Tcl_GetFloat( tclip, argv[1], &xleft )) != TCL_OK)  return res;
  if((res = Tcl_GetFloat( tclip, argv[2], &xright)) != TCL_OK)  return res;
  if((res = Tcl_GetFloat( tclip, argv[3], &ybot  )) != TCL_OK)  return res;
  if((res = Tcl_GetFloat( tclip, argv[4], &ytop  )) != TCL_OK)  return res;

  cpgswin(xleft,xright,ybot,ytop);
  return TCL_OK;
}


/*****************************************************************************/

void pgtcl_init(Tcl_Interp *interp)
{
  Tcl_CreateCommand( interp, "maxdata",Tcl_minmax, (ClientData)1, NULL);
  Tcl_CreateCommand( interp, "mindata",Tcl_minmax,	NULL	, NULL);
  Tcl_CreateCommand( interp, "pgbin",  Tcl_pgbin,  NULL,NULL);
  Tcl_CreateCommand( interp, "pgbox",  Tcl_pgbox,  NULL,NULL);
  Tcl_CreateCommand( interp, "pgclos", Tcl_pgclos, NULL,NULL);
  Tcl_CreateCommand( interp, "pgeras", Tcl_pgeras, NULL,NULL);
  Tcl_CreateCommand( interp, "pglabel",Tcl_pglab,  NULL,NULL);
  Tcl_CreateCommand( interp, "pgline", Tcl_pgline, NULL,NULL);
  Tcl_CreateCommand( interp, "pgopen", Tcl_pgopen, NULL,NULL);
  Tcl_CreateCommand( interp, "pgpt",   Tcl_pgpt,   NULL,NULL);
  Tcl_CreateCommand( interp, "pgscf",  Tcl_pgscf,  NULL,NULL);
  Tcl_CreateCommand( interp, "pgsch",  Tcl_pgsch,  NULL,NULL);
  Tcl_CreateCommand( interp, "pgsci",  Tcl_pgsci,  NULL,NULL);
  Tcl_CreateCommand( interp, "pgslct", Tcl_pgslct, NULL,NULL);
  Tcl_CreateCommand( interp, "pgsvp",  Tcl_pgsvp,  NULL,NULL);
  Tcl_CreateCommand( interp, "pgswin", Tcl_pgswin, NULL,NULL);
}
