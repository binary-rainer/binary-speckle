/* $Id: movish.c,v 1.1 1998/08/25 19:32:05 koehler Exp koehler $
 * $Log: movish.c,v $
 * Revision 1.1  1998/08/25 19:32:05  koehler
 * Initial revision
 *
 * Created:     Mon May  4 16:31:56 1998 by Koehler@hex
 * Last change: Tue Oct  6 15:07:31 2009
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <unistd.h>
#include <tk.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "tkpgplot.h" /* Needed solely for tkpgplot_Init() */
#include "fitsio.h"

#include <time.h>

#ifdef __sun
/**# include <sys/byteorder.h>**/
#define htonl(x)	(x)
#define htons(x)	(x)
#else
# include <netinet/in.h>
#endif

#undef DEBUG


/** in pgtcl.c **/

extern void pgtcl_init(Tcl_Interp *);


/** in color.c **/

long AllocColors(Display *display, Colormap colormap,
		 unsigned long *colors, int min_colors, int max_colors, char *lutname);
void FreeColors(Display *display, Colormap colormap,
		unsigned long *colors, int num_colors);

/********************************* tkpg stuff *********************************/
typedef struct FitsImage FitsImage;

struct FitsImage {
				/* Widget context, i.e. tcl/tk stuff */
	Tk_Window tkwin;		/* Tk's window object */
	Tcl_Interp *interp;		/* The application's TCL interpreter */
	unsigned long event_mask;	/* Event mask registered to FitsImgEventHandler() */

				/** X stuff **/
	Display *display;		/* The X display of the window */
	int screen;			/* The screen of the display (from pgx) */
	Visual *visual;
	int depth;
	Colormap colormap;		/* the colormap used for the image */
	unsigned long colors[256];
	long ActiveColors;
	GC gc;
	XImage *image;

				/* Public resource attributes */
	char *filename;			/* the name of the FITS file */
	int max_colors;			/* The max number of colors needed */
	int min_colors;			/* The min number of colors needed */
	int req_width;			/* The requested widget width (pixels) */
	int req_height;			/* The requested widget height (pixels) */
	int highlight_thickness;	/* The width of the highlight border */
	XColor *highlightBgColor;	/* The inactive traversal highlight color */
	XColor *highlightColor;		/* The active traversal highlight color */
	XColor *normalFg;		/* Normal foreground color (color index 1) */
	Tk_3DBorder border;		/* 3D border structure */
	int borderWidth;		/* The width of the 3D border */
	int relief;			/* Relief of the 3D border */
	char *takeFocus;		/* "1" to allow focus traversal, "0" to disallow */
	Cursor cursor;			/* The active cursor of the window */
	Tk_Anchor anchor;		/* how to position the image within the widget */
	int CurFrame;			/* the frame currently displayed */
	int ColorBarWd;			/* width of color (0 for none) */
	int Zoom;			/* + enlarge, - scale down */
	int WrapColors;			/* wrap count for colormap */
	char *lutname;			/* name of color lookup table */
	char *ScaleType;		/* linear/logarithmic	*/
	char *StrMin, *StrMax;		/* Strings for CutMin, CutMax */
	char *framenovar;		/* Tcl-variable to store current frame no */

	                      /* Private attributes */
	struct FitsFile *Fits;
	float **frameptr;		/* array of pointers to the frames */
	double *framemin, *framemax;	/* min/max values of individual frames */
	double DataMin, DataMax, DataMean, DataSigma;
	double CutMin, CutMax, Scale;
	short ScaleMode;		/* linear/logarithmic/histequal scaling    */
	float HistEqTbl[256];		/* color lookup-table for histequal scaling */
	short updatePending;		/* flag to avoid drawing multiple times	   */
	short draw_border;		/* set if we have to redraw border and bg  */
	short draw_ok;			/* flag to avoid drawing unconsistent data */
	int off_x, off_y;
};

#define SCALE_LIN  0
#define SCALE_LOG  1
#define SCALE_HIST 2

#define HIST_EQ_SIZE 25000	/** Size of Histogram used for equalisation **/

static int  CreateFitsImg(ClientData context, Tcl_Interp *interp, int argc, char *argv[]);
static void DestroyFitsImg(char *context);
static int  ConfigureFitsImg(FitsImage *fimg, int argc, char **argv, int flags);
void DisplayFitsImg(ClientData cdata);

static int FitsImgCmd(ClientData context, Tcl_Interp *interp,	int argc, char *argv[]);
static int FitsImgCmd_return(ClientData context, int iret);
static void FitsImgEventHandler(ClientData context, XEvent *event);

static short SetCurFrame(FitsImage *fimg, int CurFrame);
static void PutFrame( FitsImage *fimg);
static struct FitsFile *LoadFits( FitsImage *fimg);
static void FindDataMinMax( FitsImage *fimg);
static unsigned long *CreateHist(FitsImage *fimg, int numbins);
static short WriteFits( char *name, struct FitsImage *fimg);
static void SetAnchorPos(FitsImage *fimg);

/*
 * Describe all recognized widget resources.
 */
static Tk_ConfigSpec configSpecs[] = {

  {TK_CONFIG_BORDER,
	"-background", "background", "Background", "Black", Tk_Offset(FitsImage, border), 0},
  {TK_CONFIG_SYNONYM,
	"-bg", "background", (char *) NULL, NULL, 0, 0},

  {TK_CONFIG_COLOR,
	"-foreground", "foreground", "Foreground", "White", Tk_Offset(FitsImage, normalFg), 0},
  {TK_CONFIG_SYNONYM,
	"-fg", "foreground", (char *) NULL, NULL, 0, 0},

  {TK_CONFIG_ACTIVE_CURSOR,
	"-cursor", "cursor", "Cursor", "", Tk_Offset(FitsImage, cursor), TK_CONFIG_NULL_OK},

  {TK_CONFIG_PIXELS,
	"-borderwidth", "borderWidth", "BorderWidth", "0", Tk_Offset(FitsImage, borderWidth), 0},
  {TK_CONFIG_SYNONYM,
	"-bd", "borderWidth", (char *) NULL, NULL, 0, 0},

  {TK_CONFIG_RELIEF,
	"-relief", "relief", "Relief","raised", Tk_Offset(FitsImage, relief), 0},

  {TK_CONFIG_PIXELS, "-height","height","Height","256", Tk_Offset(FitsImage, req_height),0},
  {TK_CONFIG_PIXELS, "-width", "width", "Width", "256", Tk_Offset(FitsImage, req_width), 0},

  {TK_CONFIG_COLOR,
	"-highlightbackground","highlightBackground","HighlightBackground","grey",Tk_Offset(FitsImage, highlightBgColor), 0},
  {TK_CONFIG_COLOR,
	"-highlightcolor",     "highlightColor",     "HighlightColor",  "White",Tk_Offset(FitsImage, highlightColor), 0},
  {TK_CONFIG_PIXELS,
	"-highlightthickness", "highlightThickness", "HighlightThickness", "2", Tk_Offset(FitsImage, highlight_thickness), 0},

  {TK_CONFIG_STRING,
	"-takefocus", "takeFocus", "TakeFocus","0", Tk_Offset(FitsImage, takeFocus), TK_CONFIG_NULL_OK},

  {TK_CONFIG_ANCHOR,
	"-anchor", "anchor", "Anchor", "center", Tk_Offset(FitsImage,anchor), 0},

  {TK_CONFIG_STRING,
	"-file", "file", "File", NULL, Tk_Offset(FitsImage,filename), TK_CONFIG_NULL_OK},

  {TK_CONFIG_INT,
	"-mincolors", "minColors", "MinColors",  "2", Tk_Offset(FitsImage, min_colors), 0},
  {TK_CONFIG_INT,
	"-maxcolors", "maxColors", "MaxColors","200", Tk_Offset(FitsImage, max_colors), 0},

  {TK_CONFIG_INT, "-colorbar","colorbar","ColorBar","0",Tk_Offset(FitsImage,ColorBarWd),0},
  {TK_CONFIG_INT, "-frameno","frameno","Frameno","0",Tk_Offset(FitsImage,CurFrame),0},
  {TK_CONFIG_INT, "-zoom", "zoom", "Zoom", "1", Tk_Offset(FitsImage, Zoom), 0},
  {TK_CONFIG_INT, "-wrap", "wrap", "Wrap", "1", Tk_Offset(FitsImage, WrapColors), 0},

  {TK_CONFIG_STRING,
	"-lut", "lut", "Lut", "grayscale", Tk_Offset(FitsImage, lutname), 0},
  {TK_CONFIG_STRING,
	"-scale", "scale", "Scale", "linear", Tk_Offset(FitsImage,ScaleType), 0},
  {TK_CONFIG_STRING,
	"-min", "min", "Min", NULL, Tk_Offset(FitsImage,StrMin), TK_CONFIG_NULL_OK},
  {TK_CONFIG_STRING,
	"-max", "max", "Max", NULL, Tk_Offset(FitsImage,StrMax), TK_CONFIG_NULL_OK},
  {TK_CONFIG_STRING,
	"-framenovar","framenovar","FramenoVar",NULL,Tk_Offset(FitsImage,framenovar),TK_CONFIG_NULL_OK},

  {TK_CONFIG_END,
	(char *) NULL, (char *) NULL, (char *) NULL, (char *) NULL, 0, 0}
};

/*
 * The following macro defines the event mask normally used by the widget.
 */
#define NORMAL_EVENT_MASK ((unsigned long)(StructureNotifyMask | \
					   ExposureMask | FocusChangeMask))

/******************************************************************************
 * Provide a package initialization procedure. This creates the Tcl
 * "fitsimg" widget creation command.
 *
 * Input:
 *	interp  Tcl_Interp *  The TCL interpreter of the application.
 * Output:
 *	return  int	TCL_OK    - Success.
 *                      TCL_ERROR - Failure.
 */
int FitsImage_Init(Tcl_Interp *interp)
{
 /** Get the main window of the application. **/
  Tk_Window main_w = Tk_MainWindow(interp);

 /* If Tk_Init() hasn't been called, then there won't be a main window
  * yet. In such cases, Tk_MainWindow() places a suitable error message
  * in interp->result.
  */
  if(!main_w)  return TCL_ERROR;

 /** Create the TCL command that is to be used for creating PGPLOT widgets **/
  Tcl_CreateCommand(interp, "fitsimg", CreateFitsImg, (ClientData) main_w, 0);
  return TCL_OK;
}


/******************************************************************************
 * This function provides the TCL command that creates a FitsImg widget.
 *
 * Input:
 *  context ClientData	The client_data argument specified in
 *			FitsImage_Init() when CreateFitsImg was registered.
 *                      This is the main window cast to (ClientData).
 *  interp Tcl_Interp * The TCL intrepreter.
 *  argc   int		The number of command arguments.
 *  argv   char **	The array of 'argc' command arguments.
 *                        argv[0] = "fitsimg"
 *                        argv[1] = the name to give the new widget.
 *                        argv[2..argc-1] = attribute settings.
 * Output:
 *  return int		TCL_OK    - Success.
 *                      TCL_ERROR - Failure.
 */
static int CreateFitsImg(ClientData context, Tcl_Interp *interp, int argc, char *argv[])
{
	Tk_Window main_w= (Tk_Window)context;	/* The application main window */
	FitsImage *fimg;			/* The new widget instance object */
	char *name;				/* the name of the widget */

 /** Make sure that a name for the new widget has been provided **/
  if(argc < 2) {
	Tcl_AppendResult(interp,"Wrong number of arguments - should be \'",
				argv[0], " pathName \?options\?\'", NULL);
	return TCL_ERROR;
  }
  name= argv[1];

 /** Allocate the widget-instance object **/
  if( !(fimg= malloc(sizeof(FitsImage)))) {
	Tcl_AppendResult(interp, "Insufficient memory to create ", argv[1], NULL);
	return TCL_ERROR;
  }
 /*
  * Before attempting any operation that might fail, initialize the container
  * at least up to the point at which it can safely be passed to
  * DestroyFitsImg().
  */
  fimg->tkwin	= NULL;
  fimg->interp	= interp;
  fimg->event_mask= NoEventMask;

  fimg->display	= Tk_Display(main_w);
  fimg->screen	= Tk_ScreenNumber(main_w);
  fimg->visual  = None;
  fimg->depth	= 0;
  fimg->colormap= None;
  fimg->ActiveColors= 0;
  fimg->gc	= None;
  fimg->image	= NULL;

  fimg->filename = NULL;
  fimg->max_colors= 200;
  fimg->min_colors=   0;  /** used as flag that Tk_ConfigureWidget has been called **/
  fimg->req_width = 256;
  fimg->req_height= 256;
  fimg->highlight_thickness = 2;
  fimg->highlightBgColor = NULL;
  fimg->highlightColor = NULL;
  fimg->normalFg= NULL;
  fimg->border	= NULL;
  fimg->borderWidth = 0;
  fimg->relief	= TK_RELIEF_RAISED;
  fimg->takeFocus = NULL;
  fimg->ColorBarWd= 0;
  fimg->cursor	= None;
  fimg->lutname	= NULL;
  fimg->ScaleType= NULL;
  fimg->StrMin= fimg->StrMax= NULL;
  fimg->framenovar= NULL;

  fimg->Fits	= NULL;
  fimg->frameptr= NULL;
  fimg->framemin= fimg->framemax= NULL;
  fimg->updatePending= 0;
  fimg->draw_ok	= 0;

 /** Create the widget window from the specified path **/
  if( !(fimg->tkwin = Tk_CreateWindowFromPath(interp, main_w, name, NULL)))
  {	DestroyFitsImg((char *)fimg);	return TCL_ERROR;	}

 /** Give the widget a class name **/
  Tk_SetClass(fimg->tkwin, "Fitsimg");

 /** Register an event handler **/
  fimg->event_mask = NORMAL_EVENT_MASK;
  Tk_CreateEventHandler(fimg->tkwin, fimg->event_mask, FitsImgEventHandler, (ClientData)fimg);

 /** Create the TCL command that will allow users to configure the widget **/
  Tcl_CreateCommand(interp, name, FitsImgCmd, (ClientData)fimg, NULL);

 /** Get the application colormap and visual **/
  {	XWindowAttributes attr;		/* The attributes of the main window */

   /** Ensure that the main window of the application exists **/
    Tk_MakeWindowExist(main_w);

   /** Acquire the attributes of the main window **/
    if(!XGetWindowAttributes(fimg->display, Tk_WindowId(main_w), &attr))
    {
	Tcl_AppendResult(interp, "Unable to get attributes of main window",NULL);
	DestroyFitsImg((char *)fimg);
	return TCL_ERROR;
    }
    fimg->visual  = attr.visual;
    fimg->depth	  = attr.depth;
    fimg->colormap= attr.colormap;
  }
 /*
  * Parse command line defaults into fimg so that fimg->min_colors and
  * fimg->max_colors are known.
  */
  if( ConfigureFitsImg(fimg, argc-2, argv+2, 0) != TCL_OK)
  {	DestroyFitsImg((char *)fimg);	return TCL_ERROR;	}

 /** Make sure the window exists so we can try to draw in it **/
  Tk_MakeWindowExist(fimg->tkwin);

 /** Create a Graphics Context (we have to change the fg to draw the colorbar) **/
  {	XGCValues xgcv;
	xgcv.foreground= 0;
	fimg->gc = XCreateGC(fimg->display,Tk_WindowId(fimg->tkwin),GCForeground,&xgcv);
  }
 /** Return the widget name **/
  Tcl_SetResult(interp, Tk_PathName(fimg->tkwin), TCL_STATIC);
  return TCL_OK;
}


/******************************************************************************
 * Delete a FitsImage widget.
 *
 * Input:
 *	char *context	The widget to be deleted
 * Output:
 *	none
 */
static void DestroyFitsImg(char *context)
{
	FitsImage *fimg= (FitsImage *) context;

  if(fimg)
  {	fimg->draw_ok= 0;

	/** Unload FitsFile **/
	if(fimg->Fits)
	{	FitsClose(fimg->Fits);	fimg->Fits= NULL;	}

	/** free XImage **/
	if(fimg->image != NULL)
	{
	  if(fimg->image->data) { free(fimg->image->data); fimg->image->data= NULL; }
	  XDestroyImage(fimg->image);	fimg->image= NULL;
	}

	/** Delete Graphics Context **/
	if( fimg->gc != None)
	{	XFreeGC(fimg->display,fimg->gc);	fimg->gc= None; }

	/** free Colors **/
	if( fimg->ActiveColors > 0)
	{	FreeColors(fimg->display,fimg->colormap,fimg->colors,fimg->ActiveColors);
		fimg->ActiveColors= 0;
	}

	/** Delete resource values **/
	if( fimg->min_colors > 0)
	{	Tk_FreeOptions(configSpecs, (char *)fimg, fimg->display, 0);
		fimg->min_colors= 0;
	}

	/** Remove the DestroyNotify event handler before destroying the
	 ** window. Otherwise this function would call itself recursively
	 **/
	if(fimg->event_mask != NoEventMask)
	{ Tk_DeleteEventHandler(fimg->tkwin, fimg->event_mask,
				FitsImgEventHandler, (ClientData) fimg);
	  fimg->event_mask = NoEventMask;
	}

	/** Zap the window **/
	if(fimg->tkwin)
	{	Tk_DestroyWindow(fimg->tkwin);	fimg->tkwin = NULL;	}

	/** Delete the container **/
	free(fimg);
  }
}


/******************************************************************************
 * Configure a FitsImage widget
 *
 * Input:
 *	FitsImage *fimg		the widget to be configured
 *	int	argc		the number of option arguments
 *	char	*argv[]		the options
 *	int	flags		flags for Tk_ConfigureWidget
 * Output:
 *	return	int		TCL_OK or TCL_ERROR
 */
static int  ConfigureFitsImg(FitsImage *fimg, int argc, char **argv, int flags)
{
	Tcl_Interp *interp= fimg->interp;
	char *widget= Tk_PathName(fimg->tkwin);
	char *old_lut= fimg->lutname;

  fimg->draw_ok= 0;

  if(Tk_ConfigureWidget(interp, fimg->tkwin, configSpecs,
			argc, argv, (char *) fimg, flags) != TCL_OK) {
	printf("Configure Widget failed: %s\n",interp->result);
	return TCL_ERROR;
  }

	if(fimg->min_colors <   2)	fimg->min_colors =   2;
   else if(fimg->min_colors > 256)	fimg->min_colors = 256;
	if(fimg->max_colors <   2)	fimg->max_colors =   2;
   else if(fimg->max_colors > 256)	fimg->max_colors = 256;

  Tk_SetWindowBackground(fimg->tkwin,Tk_3DBorderColor(fimg->border)->pixel);
  Tk_SetInternalBorder(fimg->tkwin, fimg->borderWidth);
  Tk_GeometryRequest(fimg->tkwin, fimg->req_width, fimg->req_height);

  if( (fimg->ActiveColors != fimg->max_colors)	/** user changed # of colors **/
   || (old_lut != fimg->lutname))		/** user changed name of lut **/
  {
    if( fimg->ActiveColors > 0)
	FreeColors(fimg->display,fimg->colormap,fimg->colors,fimg->ActiveColors);

    fimg->ActiveColors= AllocColors(fimg->display, fimg->colormap, fimg->colors,
					fimg->min_colors, fimg->max_colors, fimg->lutname);
    if( fimg->ActiveColors < fimg->min_colors)
    {	Tcl_AppendResult(interp,"Couldn't allocate minimum number of colors for ",widget,NULL);
	return TCL_ERROR;
    }
    fimg->max_colors= fimg->ActiveColors;
    /*fprintf(stderr," %ld color cells reserved\n",fimg->ActiveColors);*/
  }

  /** check if the filename was changed **/
  if( fimg->filename
   && ((fimg->Fits == NULL) || (fimg->filename != fimg->Fits->filename)))
  {
    if( LoadFits(fimg) == NULL)
    {	Tcl_AppendResult(interp, widget, ": Can't load ",fimg->filename,NULL);
	return TCL_ERROR;
    }
    fimg->CurFrame= 0;
    /** Find default Cuts **/
    FindDataMinMax(fimg);
  }
  if(fimg->Fits)			/** allocate XImage **/
  {	int Enlarge= fimg->Zoom;

    if( Enlarge<=0)  Enlarge= 1;

    if( (fimg->image == NULL)
     ||	(Enlarge * fimg->Fits->n1 != fimg->image->width)
     || (Enlarge * fimg->Fits->n2 != fimg->image->height))
    {
	if( fimg->image != NULL)	XDestroyImage(fimg->image);
	if( !(fimg->image= XCreateImage(fimg->display,fimg->visual,fimg->depth,
				ZPixmap /*format*/, 0 /*offset*/, NULL/*data*/,
				Enlarge*fimg->Fits->n1, Enlarge*fimg->Fits->n2,
				8 /*bitmap_pad*/, 0	)))
	{	Tcl_AppendResult(interp, widget, ": Can't create image",NULL);
		return TCL_ERROR;
	}
	if( !(fimg->image->data= malloc(fimg->image->bytes_per_line*Enlarge*fimg->Fits->n2)))
	{	Tcl_AppendResult(interp, widget, ": No memory for image data",NULL);
		return TCL_ERROR;
	}
    }
    /** Set position within widget **/
    SetAnchorPos(fimg);
  }
  /** check for User-given Cuts **/
  fimg->CutMin= fimg->StrMin ? atof(fimg->StrMin) : fimg->DataMin;
  fimg->CutMax= fimg->StrMax ? atof(fimg->StrMax) : fimg->DataMax;

  fimg->Scale = (double)(fimg->ActiveColors*fimg->WrapColors - 1);

  if( strncmp(fimg->ScaleType,"hist",4)==0)		/** histequal scaling **/
  {	unsigned long *histogram;
	long  step, sum, i,c;
	float hlp;

	fimg->ScaleMode= SCALE_HIST;
	if( !(histogram= CreateHist(fimg,HIST_EQ_SIZE)))
	{	Tcl_AppendResult(interp,"Not enough memory for histogram of ",widget,NULL);
		return TCL_ERROR;
	}
	step= fimg->Fits->n1 * fimg->Fits->n2 * fimg->Fits->n3 / (fimg->ActiveColors - 1);

	fimg->HistEqTbl[0]= 0.;
	i = sum = 0;
	for (c=1;  c <= fimg->ActiveColors;  c++)
	{	while( i<HIST_EQ_SIZE && (sum <= c * step))  { sum += histogram[i];  ++i; }

		/** too high:  sum		 +	**/
		/** should be: c*step		/	**/
		/** too low: sum-histogram[i]  +	**/
		/**			  (i-1)^ ^i	**/
		hlp= (float)i;
		if (histogram[i]!=0) { hlp -= (float)(sum-c*step) / (float)histogram[i]; }
		fimg->HistEqTbl[c]= hlp * (float)fimg->CutMax / (float)HIST_EQ_SIZE;
		/*fprintf(stderr,"HistEqTbl[%ld] = %f\n",c,fimg->HistEqTbl[c]);*/
	}
	free(histogram);
  }
  else if( strncmp(fimg->ScaleType,"log",3)==0)		/** logarithmic scaling **/
  {	fimg->ScaleMode= SCALE_LOG;
	fimg->Scale /= 10.0;				/** In PutImg() wird dieser Faktor gebraucht **/
  } else {
	fimg->ScaleMode= SCALE_LIN;
	fimg->Scale /= fimg->CutMax - fimg->CutMin;	/** scale factor: max -> CMapSize*wrap-1 **/
  }
  /** set tcl variable to CurFrame **/
  SetCurFrame(fimg,fimg->CurFrame);

  fimg->draw_ok= 1;
  if( !fimg->updatePending)
  {
	Tk_DoWhenIdle( DisplayFitsImg, (ClientData)fimg);
	fimg->updatePending = 1;
  }
  return TCL_OK;
}


/******************************************************************************
 * This function services TCL commands for a given widget.
 *
 * Input:
 *  context   ClientData    The fimg widget cast to (ClientData).
 *  interp    Tcl_Interp *  The TCL intrepreter.
 *  argc             int    The number of command arguments.
 *  argv            char ** The array of 'argc' command arguments.
 * Output:
 *  return           int    TCL_OK    - Success.
 *                          TCL_ERROR - Failure.
 */
static int FitsImgCmd(ClientData context, Tcl_Interp *interp, int argc, char *argv[])
{
	FitsImage *fimg = (FitsImage *) context;
	char *widget= argv[0];  /* The name of the widget */
	char *command;		/* The name of the command */

  if(argc < 2)
  {	Tcl_AppendResult(interp, "Missing arguments to ", widget, " command.",NULL);
	return TCL_ERROR;
  }
  command = argv[1];
 /*
  * Prevent untimely deletion of the widget while this function runs.
  * Note that following this statement you must return via
  * FitsImgCmd_return() to ensure that Tk_Release() gets called.
  */
  Tk_Preserve(context);

 /** Check for recognized command names **/
  if(strcmp(command, "configure") == 0)		/** Configure widget **/
  {
    switch(argc - 2) {
      case 0:			/** Return the values of all config options **/
	return FitsImgCmd_return(context,
			Tk_ConfigureInfo(interp, fimg->tkwin, configSpecs,
					 (char *) fimg, NULL, 0));
	break;
      case 1:			/** Return the value of a single given option **/
	return FitsImgCmd_return(context,
		        Tk_ConfigureInfo(interp, fimg->tkwin, configSpecs,
					 (char *) fimg, argv[2], 0));
	break;
      default:			/** Change one of more of the config options **/
	return FitsImgCmd_return(context,
			ConfigureFitsImg(fimg,argc-2,argv+2,TK_CONFIG_ARGV_ONLY));
	break;
    }
  }
  else if(strcmp(command, "cget") == 0)		/** Get a configuration value **/
  {
    if(argc != 3) {
	Tcl_AppendResult(interp,"Wrong number of arguments to \"", widget,
				" cget\" command", NULL);
	return FitsImgCmd_return(context, TCL_ERROR);
    } else {
	return FitsImgCmd_return(context,
			Tk_ConfigureValue(interp, fimg->tkwin, configSpecs,
					  (char *) fimg, argv[2], 0));
    }
  }
  else if(strcmp(command, "about") == 0)	/** Who are you, anyway? **/
  {
	Tcl_AppendResult(interp,"movish by Rainer Köhler, compiled on "__DATE__, NULL);
	return FitsImgCmd_return(context, TCL_OK);
  }
  else if(fimg->Fits == NULL)			/** the following cmds need data **/
  {
	Tcl_AppendResult(interp,"No file loaded in ", widget, NULL);
	return FitsImgCmd_return(context, TCL_ERROR);
  }
  else
  { if(strcmp(command, "forward") == 0)		/** one frame forward **/
    {
	if( SetCurFrame(fimg,fimg->CurFrame+1))
		Tcl_AppendResult(interp,"No more frames in ", widget, NULL);
	else	PutFrame(fimg);
	return FitsImgCmd_return(context, TCL_OK);
    }
    else if(strcmp(command, "backward") == 0)	/** one frame backward **/
    {
	if( SetCurFrame(fimg,fimg->CurFrame-1))
		Tcl_AppendResult(interp,"No more frames in ", widget, NULL);
	else	PutFrame(fimg);
	return FitsImgCmd_return(context, TCL_OK);
    }
    else if(strcmp(command, "play") == 0)	/** play movie **/
    {
	for( ;  SetCurFrame(fimg,fimg->CurFrame)==0;  ++fimg->CurFrame)
		PutFrame(fimg);
	return FitsImgCmd_return(context,TCL_OK);
    }
    else if(strcmp(command,"datamin") == 0
	 || strcmp(command,"datamax") == 0
	 || strcmp(command,"datamean") == 0
	 || strcmp(command,"datasigma") == 0)	/** min, max... of data **/
    {
		char line[42];

	sprintf(line,"%g",
		command[4]=='s' ? fimg->DataSigma :
		(command[5]=='e' ? fimg->DataMean :
		 (command[5]=='i'? fimg->DataMin : fimg->DataMax)));
	Tcl_AppendResult(interp,line,NULL);
	return TCL_OK;
    }
    else if(strcmp(command,"header") == 0)	/** get fitsheader **/
    {
		char line[82], *headln;
		int offs;

	if( argc>2)		/** only one keyword **/
	{
	  sprintf(line,"%-8s= ",argv[2]);
	  if( !(headln= FitsFindKeyw(fimg->Fits,line)))
	  {	Tcl_AppendResult(interp,argv[2]," not found in header",NULL);
		return TCL_ERROR;
	  }
	  else
	  {	memcpy(line,headln-10,80);	line[80]= 0;
		Tcl_AppendResult(interp,line,NULL);
		return TCL_OK;
	  }
	}
	for( offs=0, headln=fimg->Fits->Header;
		offs < fimg->Fits->HeaderSize;  offs += 80, headln += 80)
	{
		memcpy(line,headln,80);		line[80]= 0;
		Tcl_AppendElement(interp,line);
		if( strncmp(line,"END     ",8)==0)	break;
	}
	return TCL_OK;
    }
    else if(strcmp(command,"histogram") == 0)	/** histogram of pixel values **/
    {
		unsigned long *histogram;
		int  numbins=200, i;
		char line[42];

	if( argc>2)	numbins= atoi(argv[2]);
	if( !(histogram= CreateHist(fimg,numbins)))
	{	Tcl_AppendResult(interp,"Not enough memory for histogram of ",widget,NULL);
		return TCL_ERROR;
	}
	for( i=0;  i<numbins;  ++i)
	{	sprintf(line,"%lu",histogram[i]);
		Tcl_AppendElement(fimg->interp,line);
	}
	free(histogram);
	return TCL_OK;
    }
    else if(strcmp(command,"sumlist") == 0
	 || strcmp(command,"meanlist") == 0)	/** sum/mean of pixel values in one frame **/
    {
		long framesz= fimg->Fits->n1 * fimg->Fits->n2;
		int fr, i;
		float sum;
		char line[42];

	for( fr=0;  fr<fimg->Fits->n3;  ++fr)
	{ sum= 0.0;
	  for( i=0;  i<framesz;  ++i)
		sum += fimg->frameptr[fr][i];

	  sprintf(line,"%g",command[0]=='s'? sum:sum/(float)framesz);
	  Tcl_AppendElement(fimg->interp,line);
	}
	return TCL_OK;
    }
    else if(strcmp(command,"minlist") == 0
	 || strcmp(command,"maxlist") == 0)	/** min/max of pixel values in one frame **/
    {
		int fr;
		double *mmptr= command[1]=='i'? fimg->framemin : fimg->framemax;
		char line[42];

	for( fr=0;  fr<fimg->Fits->n3;  ++fr)
	{ sprintf(line,"%g", mmptr[fr]);
	  Tcl_AppendElement(fimg->interp,line);
	}
	return TCL_OK;
    }
    else if(strcmp(command,"pixel") == 0)	/** get pixel no. from position **/
    {
		int pixel, piyel;
		short enlarge= fimg->Zoom;

	if( enlarge<=0)  enlarge=1;
	if( argc<4)
	{	Tcl_AppendResult(interp,"Wrong number of arguments to ",widget,
					", should be \"pixel x|y|v pos",NULL);
		return TCL_ERROR;
	}
	if( argv[2][0]=='v')
	{ if( argc<5)
	  {	Tcl_AppendResult(interp,"Wrong number of arguments to ",widget,
					", should be \"pixel v posx posy",NULL);
		return TCL_ERROR;
	  }
	  pixel= (atoi(argv[3])-fimg->off_x) / enlarge;
	  piyel= fimg->Fits->n2 - (atoi(argv[4])-fimg->off_y) / enlarge;

	  /** out of frame => no result, just empty string **/
	  if( pixel>=0 && pixel<fimg->Fits->n1 && piyel>=0 && piyel<fimg->Fits->n2)
		sprintf(interp->result,"%f",
			fimg->frameptr[fimg->CurFrame][pixel + piyel*fimg->Fits->n1]);
	  return TCL_OK;
	}
	else if( argv[2][0]=='x')
	{	pixel= (atoi(argv[3])-fimg->off_x) / enlarge;
		if( pixel >= fimg->Fits->n1)	pixel= -1;
	}
	else if( argv[2][0]=='y')
	{	pixel= fimg->Fits->n2 - (atoi(argv[3])-fimg->off_y) / enlarge;
		if( pixel >= fimg->Fits->n2)	pixel= -1;
	}
	if( pixel>=0)	sprintf(interp->result,"%d",pixel+1);	/** saoimage-convention **/
	return TCL_OK;
    }
    else if(strcmp(command,"delete") == 0)	/** delete frame **/
    {
		long frame;
		char line[100];

	frame= (argc>2)? atoi(argv[2]) : fimg->CurFrame;
	if( frame >= fimg->Fits->n3)
	{	Tcl_AppendResult(interp,"frame number out of range",NULL);
		return TCL_ERROR;
	}
	fimg->Fits->n3--;
	memmove( &(fimg->frameptr[frame]), &(fimg->frameptr[frame+1]),
					(fimg->Fits->n3-frame) * sizeof(float *));
	memmove( &(fimg->framemin[frame]), &(fimg->framemin[frame+1]),
					(fimg->Fits->n3-frame) * sizeof(double));
	memmove( &(fimg->framemax[frame]), &(fimg->framemax[frame+1]),
					(fimg->Fits->n3-frame) * sizeof(double));

	if( fimg->Fits->naxis > 2)
	{	sprintf(line,"NAXIS3  = %20d   / %-45s",
				fimg->Fits->n3,"number of pixels along 3.axis");
		FitsSetHeaderLine(fimg->Fits, "NAXIS3  = ", line);
	}
	sprintf(line,"movish: removed frame %ld",frame);
	FitsAddHistKeyw(fimg->Fits,line);
	FindDataMinMax(fimg);

	SetCurFrame(fimg,fimg->CurFrame);	/** might be > n3 now **/
	if( !fimg->updatePending)
	{	Tk_DoWhenIdle( DisplayFitsImg, (ClientData)fimg);
		fimg->updatePending = 1;
	}
	return TCL_OK;
    }
    else if(strcmp(command,"save") == 0)	/** save FITSfile **/
    {
		char *fname= fimg->Fits->filename;

	if( argc>2)	fname= argv[2];
	if( WriteFits(fname,fimg))
	{	Tcl_AppendResult(interp,"Couldn't write ",fname,NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
    }
  }
/*
 * Unknown command name.
 */
  Tcl_AppendResult(interp, "Unknown command \"", widget, " ", command, "\"",
		   NULL);
  return FitsImgCmd_return(context, TCL_ERROR);
}

/******************************************************************************
 * This is a private cleanup-return function of FitsImgCmd().
 * It should be used to return from said function after Tk_Preserve() has
 * been called. It calls Tk_Release() on the widget to unblock deletion
 * and returns the specified error code.
 *
 * Input:
 *  context   ClientData    The tkpg widget cast to (ClientData).
 *  iret             int    TCL_OK or TCL_ERROR.
 * Output:
 *  return           int    The value of iret.
 */
static int FitsImgCmd_return(ClientData context, int iret)
{
	Tk_Release(context);
	return iret;
}


/******************************************************************************
 * This is the main X event callback for Pgplot widgets.
 *
 * Input:
 *  context   ClientData    The fimg widget cast to (ClientData).
 *  event         XEvent *  The event that triggered the callback.
 */
static void FitsImgEventHandler(ClientData context, XEvent *event)
{
	FitsImage *fimg = (FitsImage *) context;

 /** Determine what type of event triggered this call **/
  switch(event->type) {
    case ConfigureNotify:	/* The window has been resized */
	SetAnchorPos(fimg);
	/* drop through */

    case Expose:		/* Redraw the specified area */
	fimg->draw_border= 1;
	if( !fimg->updatePending)
	{
		Tk_DoWhenIdle( DisplayFitsImg, (ClientData)fimg);
		fimg->updatePending = 1;
	}
	break;

    case DestroyNotify:		/* The window has been destroyed */

	/** Delete the main event handler to prevent prolonged use **/
	Tk_DeleteEventHandler(fimg->tkwin, fimg->event_mask,
				FitsImgEventHandler, (ClientData) fimg);

	/** Tell DestroyFitsImg() that we have already deleted the event mask **/
	fimg->event_mask = NoEventMask;

	/** Queue deletion of FitsImg until all references have been completed **/
	Tk_EventuallyFree(context, DestroyFitsImg);
	break;

    case FocusIn:		/* Keyboard-input focus has been acquired */
	break;
    case FocusOut:		/* Keyboard-input focus has been lost */
	break;
  }
  return;
}


/*****************************************************************************
 * Routine to set fimg->CurFrame
 * takes care of size of Fits-cube, and sets tcl-var given by -framenovar
 *
 * returns 1 if CurFrame is out of allowed range
 */

static short SetCurFrame(FitsImage *fimg, int CurFrame)
{
	short res=0;

  if( fimg->Fits == NULL)
  {	if(CurFrame)	res= 1;		/** only 0 allowed if no fits loaded **/
	CurFrame= 0;
  }
  else if( CurFrame >= fimg->Fits->n3)	{ res= 1;  CurFrame= fimg->Fits->n3-1; }
  else if( CurFrame < 0)		{ res= 1;  CurFrame= 0;		}

  fimg->CurFrame= CurFrame;

  if( fimg->framenovar)
  {	char line[10];

	sprintf(line,"%d", CurFrame);
	Tcl_SetVar(fimg->interp,fimg->framenovar,line,TCL_GLOBAL_ONLY);
  }
  return res;
}


/*************************** from putimg.c *********************************/

void DisplayFitsImg(ClientData cdata)
{
	FitsImage *fimg= (FitsImage *)cdata;

  fimg->updatePending = 0;
  if (!Tk_IsMapped(fimg->tkwin)) return;

  if (fimg->draw_border)
  {	Tk_Fill3DRectangle(fimg->tkwin, Tk_WindowId(fimg->tkwin), fimg->border,
		0, 0, Tk_Width(fimg->tkwin), Tk_Height(fimg->tkwin),
		fimg->borderWidth, fimg->relief);
	fimg->draw_border= 0;
  }
  if( fimg->ColorBarWd > 0)
  {		int x, width= Tk_Width(fimg->tkwin) - 2*fimg->borderWidth,
			bot = Tk_Height(fimg->tkwin)- 1-fimg->borderWidth;

	for( x=0;  x<width;  ++x)
	{ XSetForeground(fimg->display, fimg->gc,
			 fimg->colors[ x * fimg->ActiveColors/width ]);
	  XDrawLine(fimg->display, Tk_WindowId(fimg->tkwin), fimg->gc,
			x+fimg->borderWidth, bot,
			x+fimg->borderWidth, bot+1-fimg->ColorBarWd);
	}
  }
  if( fimg->draw_ok && fimg->Fits)
  {	if( fimg->CurFrame >= fimg->Fits->n3)	fimg->CurFrame= 0;
	else if( fimg->CurFrame < 0)		fimg->CurFrame= fimg->Fits->n3-1;

	PutFrame(fimg);
  }
}

static short FindHistEqCol( FitsImage *fimg, float v)
{
  short Lo=0, Hi= fimg->ActiveColors, mid;

  while( v!=fimg->HistEqTbl[Lo] && (Hi-Lo > 1))
  {	mid= (Lo+Hi)/2;
	if( v >= fimg->HistEqTbl[mid]) { Lo = mid; }
				 else  { Hi = mid; }
  }
  /*
  for( Lo=0;  Lo<fimg->ActiveColors;  Lo++)  if( v<fimg->HistEqTbl[Lo]) break;
   */
  return Lo-1;
}

static void PutFrame( FitsImage *fimg)
{
	struct FitsFile *Src= fimg->Fits;
	long  frame= fimg->CurFrame;
	float *buff= fimg->frameptr[frame];
	float max= fimg->CutMax, min= fimg->CutMin, scale= fimg->Scale;
	short enlarge= fimg->Zoom;
	float val, prelogscl;
	short rx,ry,c;
	long x,y, xenl,yenl;
	/*long yoff, xsize= Src->n1 * enlarge; */

  if( enlarge<=0)  enlarge=1;
  prelogscl= (exp(10.)-1.0) / (max-min);

				   /** Zeilen von oben nach unten, aber nachher andersrum **/
  for( y=Src->n2-1, yenl=0;  y>=0;  --y, yenl+=enlarge)
    for( x= xenl= 0;  x<Src->n1;  ++x, xenl+=enlarge)	/** Spalten von links nach rechts **/
    {
	val= buff[x + Src->n1*y];
	if( val > max)	val= max;
	if( val <=min)	val= 0.0;
		else	val -= min;			/** min<= val <=max **/
	if(fimg->ScaleMode == SCALE_LOG)
	{		val= log( val*prelogscl + 1.0);	/**  0 <= val <= 10 **/
		/** believe it or not, this is the same as saoimage's log-scale **/
	}
	if(fimg->ScaleMode == SCALE_HIST)
		c= FindHistEqCol(fimg, val);	/** 0<=val<=fimg->CutMax **/
	else	c= (short)(val * scale) % fimg->ActiveColors;

	for( ry=0;  ry<enlarge;  ++ry)		/** repeat lines **/
	{  /*yoff= xsize*(yenl+ry);*/
	   for( rx=0;  rx<enlarge;  ++rx)	/** repeat columns **/
		XPutPixel(fimg->image, xenl+rx, yenl+ry, fimg->colors[c]);
		/*fimg->image->data[(xenl+rx) + yoff]= fimg->colors[c];*/
	}
    }
  XPutImage( fimg->display,Tk_WindowId(fimg->tkwin), fimg->gc,fimg->image,
		0,0, fimg->off_x, fimg->off_y,
		enlarge*fimg->Fits->n1, enlarge*fimg->Fits->n2);
}


/*****************************************************************************/

static short FrameCallBack(struct FitsFile *ff, long fr, void *dummy)
{
	FitsImage *fimg= dummy;
	long framesz= ff->n1 * ff->n2;

  if( !(fimg->frameptr))
     if( !(fimg->frameptr= AllocBuffer(ff->n3 * sizeof(float *),"frame pointer array")))
	return 1;

  if( !(fimg->framemin))
  {  if( !(fimg->framemin= AllocBuffer(ff->n3 * 2 * sizeof(double),"min/max array")))
	return 2;
     fimg->framemax= fimg->framemin + ff->n3;
  }

  fimg->frameptr[fr]= ff->Buffer.f + fr*framesz;

  /** Find min/max **/
  FindMinMax( (union anypntr)(fimg->frameptr[fr]), BITPIX_FLOAT, framesz,
			&(fimg->framemin[fr]), &(fimg->framemax[fr]));

  if( (fr&3) == 0)
  {	fimg->Fits= ff;		/** SetCurFrame() needs this! **/
	SetCurFrame(fimg,fr);
	while( Tcl_DoOneEvent(TK_ALL_EVENTS|TCL_DONT_WAIT)) ;
  }
  return 0;
}

struct FitsFile *LoadFits(FitsImage *fimg)
{
	time_t tim;

  if( fimg->Fits)
  {	FitsClose(fimg->Fits);	fimg->Fits= NULL;	}

  if( fimg->frameptr)
  {	free(fimg->frameptr);	fimg->frameptr= NULL;	}

  if( fimg->framemin)
  {	free(fimg->framemin);	fimg->framemin= fimg->framemax= NULL;	}

  tim= time(NULL);

  if( !(fimg->Fits = LoadFitsFile(fimg->filename,BITPIX_FLOAT,FrameCallBack,fimg)))
	return NULL;

#ifdef DEBUG
  printf("elapsed time %ld secs\n",time(NULL)-tim);
#endif
  return fimg->Fits;
}

/*****************************************************************************/

static void FindDataMinMax(struct FitsImage *fimg)
{
	long fr, i;
	long framesz= fimg->Fits->n1 * fimg->Fits->n2;
	double sum, sum2, N;

  fimg->DataMin= fimg->framemin[0];
  fimg->DataMax= fimg->framemax[0];
  sum = sum2 = 0.0;

  for( fr=0;  fr<fimg->Fits->n3;  ++fr)
  {	if(fimg->DataMin > fimg->framemin[fr])	fimg->DataMin= fimg->framemin[fr];
	if(fimg->DataMax < fimg->framemax[fr])	fimg->DataMax= fimg->framemax[fr];
	for( i=0;  i<framesz;  ++i)
	{	sum  += fimg->frameptr[fr][i];
		sum2 += fimg->frameptr[fr][i] * fimg->frameptr[fr][i];
	}
  }
  N = (double)(framesz * fimg->Fits->n3);
  fimg->DataMean = sum / N;
  fimg->DataSigma= sqrt((sum2 - sum*sum/N) / N );
}

/*****************************************************************************/

static unsigned long *CreateHist(FitsImage *fimg, int numbins)
{
	unsigned long *histogram, fr, i, framesz;
	float step;

  if( !(histogram= malloc((numbins+2)*sizeof(unsigned long))))	return NULL;

  for( i=0;  i<numbins;  ++i)	histogram[i]= 0;

  framesz= fimg->Fits->n1 * fimg->Fits->n2;
  step= (fimg->DataMax - fimg->DataMin) / (float)(numbins-1);

  for( fr=0;  fr<fimg->Fits->n3;  ++fr)
    for( i=0;  i<framesz;  ++i)
	++histogram[(int)((fimg->frameptr[fr][i] - fimg->DataMin) / step)];

  return histogram;
}

/*****************************************************************************/

static void SetAnchorPos(FitsImage *fimg)
{
	int fits_w, fits_h, widg_w, widg_h;

  if( fimg->Fits == NULL)	return;

  fits_w= fimg->Fits->n1;	widg_w= Tk_Width(fimg->tkwin);
  fits_h= fimg->Fits->n2;	widg_h= Tk_Height(fimg->tkwin) - fimg->ColorBarWd;

  if( fimg->Zoom > 1)
  {	fits_w *= fimg->Zoom;	fits_h *= fimg->Zoom;	}

  switch(fimg->anchor)
  {	case TK_ANCHOR_N:	fimg->off_x= (widg_w - fits_w) / 2;
				fimg->off_y= fimg->borderWidth;
				break;
	case TK_ANCHOR_NE:	fimg->off_x= widg_w - fits_w - fimg->borderWidth;
				fimg->off_y= fimg->borderWidth;
				break;
	case TK_ANCHOR_E:	fimg->off_x=  widg_w - fits_w - fimg->borderWidth;
				fimg->off_y= (widg_h - fits_h) / 2;
				break;
	case TK_ANCHOR_SE:	fimg->off_x= widg_w - fits_w - fimg->borderWidth;
				fimg->off_y= widg_h - fits_h - fimg->borderWidth;
				break;
	case TK_ANCHOR_S:	fimg->off_x= (widg_w - fits_w) / 2;
				fimg->off_y= widg_h - fits_h - fimg->borderWidth;
				break;
	case TK_ANCHOR_SW:	fimg->off_x= fimg->borderWidth;
				fimg->off_y= widg_h - fits_h - fimg->borderWidth;
				break;
	case TK_ANCHOR_W:	fimg->off_x= fimg->borderWidth;
				fimg->off_y= (widg_h - fits_h) / 2;
				break;
	case TK_ANCHOR_NW:	fimg->off_x= fimg->borderWidth;
				fimg->off_y= fimg->borderWidth;
				break;
	default:
	case TK_ANCHOR_CENTER:	fimg->off_x= (widg_w - fits_w) / 2;
				fimg->off_y= (widg_h - fits_h) / 2;
  }
}

/************************************************************************/
/* based on FitsWriteAny()
 * Return values:
 * 0 - all right
 * 1 - Can't open output file
 * 2 - Error while writing
 */

static short WriteFits( char *name, struct FitsImage *fimg)
{
	struct FitsFile *ff= fimg->Fits;
	char line[88];
	union anypntr buff;
	double min, max;
	short err;
	long  fr, i, framesz, filesz, curframe= fimg->CurFrame;

  if( ff->naxis<3)	ff->n3= 1;
  framesz= ff->n1 * ff->n2;

  if( !ff->Header)	FitsMakeSimpleHeader(ff);

  ff->filename= name? name : "(stdout)";	err=0;
  if( !(ff->fp= rfopen( name,"wb")))	return 1;

  printf(" Writing %s (%d x %d x %d pixels)\n",ff->filename, ff->n1,ff->n2,ff->n3);

  sprintf(line,"BITPIX  = %20d   / %-45s",ff->bitpix,"Data type");
  FitsSetHeaderLine( ff, "BITPIX  = ", line);

  sprintf(line,"NAXIS   = %20d   / %-45s",ff->naxis,"Number of axes");
  FitsSetHeaderLine( ff, "NAXIS   = ", line);
  sprintf(line,"NAXIS1  = %20d   / %-45s",ff->n1,"number of pixels along 1.axis");
  FitsSetHeaderLine( ff, "NAXIS1  = ", line);
  sprintf(line,"NAXIS2  = %20d   / %-45s",ff->n2,"number of pixels along 2.axis");
  FitsSetHeaderLine( ff, "NAXIS2  = ", line);
  if( ff->naxis > 2)
  {	sprintf(line,"NAXIS3  = %20d   / %-45s",ff->n3,"number of pixels along 3.axis");
	FitsSetHeaderLine( ff, "NAXIS3  = ", line);
  }else	FitsSetHeaderLine( ff, "NAXIS3  = ", NULL);

  if( ff->bitpix > 0)	/** smaller range than buffer data **/
  {	min= fimg->DataMin;	max= fimg->DataMax;
	printf("	min is %g, max is %g\n",min,max);

	ff->bzero = (min+max)/2.0;
	ff->bscale= (max-min);
	if( ff->bitpix==8)			/** tape= 0...255 **/
	{	ff->bzero= min;		ff->bscale /= 255.0;  }
	else if( ff->bitpix==16)	ff->bscale /= 65536.0 - 2.0;
	else if( ff->bitpix==32)	ff->bscale /= 65536.0*65536.0 - 2.0;
	else if( ff->bitpix==-32)	ff->bscale /= FLT_MAX * 0.99;
		/** We don't use the full float-range, just to be on the safe side **/

	if( ff->bscale > 1.0)
		fprintf(stderr," Warning: BSCALE = %g in %s!\n", ff->bscale, ff->filename);
	/*printf("	BSCALE = %g, BZERO = %g\n",ff->bscale,ff->bzero);*/
  }
  else			/** fitsfile has range >= buffer **/
  {	ff->bzero = 0.0;
	ff->bscale= 1.0;
  }
  sprintf(line,"BSCALE  = %20g   / %-45s",ff->bscale,"Real = Tape * BSCALE + BZERO");
  FitsSetHeaderLine( ff, "BSCALE  = ", line);

  sprintf(line,"BZERO   = %20g   / %-45s",ff->bzero, "Real = Tape * BSCALE + BZERO");
  FitsSetHeaderLine( ff, "BZERO   = ", line);

  fwrite( ff->Header,1,ff->HeaderSize,ff->fp);

  for( fr=0;  fr<ff->n3;  ++fr)
  { buff.f= fimg->frameptr[fr];

    switch(ff->bitpix)
    {	case 8:	for( i=0;  i<framesz;  ++i)
		{ unsigned char data= (unsigned char)
				      ((DoubleOfAnyp(buff,BITPIX_FLOAT,i)-ff->bzero) / ff->bscale);
		  if( fwrite( &data,sizeof(unsigned char),1,ff->fp)<1) break;
		}
		break;
	case 16:
		for( i=0;  i<framesz;  ++i)
		{
		  union
		  {	short s;
			unsigned char b[2];
		  } data;
		  data.s= (short)((DoubleOfAnyp(buff,BITPIX_FLOAT,i)-ff->bzero) / ff->bscale);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  {
			unsigned char hlp;
			hlp= data.b[0];  data.b[0]= data.b[1];  data.b[1]= hlp;
		  }
#endif
		  if( fwrite( &data.s,sizeof(short),1,ff->fp) < 1)  break;
		}
		break;
	case 32:
		for( i=0;  i<framesz;  ++i)
		{
		  union
		  {	int l;
			unsigned char b[4];
		  } data;
		  data.l= (long)
			((DoubleOfAnyp(buff,BITPIX_FLOAT,i)-ff->bzero) / ff->bscale);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  {
			unsigned char hlp;
			hlp= data.b[0];  data.b[0]= data.b[3];  data.b[3]= hlp;
			hlp= data.b[1];  data.b[1]= data.b[2];  data.b[2]= hlp;
		  }
#endif
		  /** FITS expects 4 bytes, so let's write 4 bytes and not sizeof(int) **/
		  if( fwrite( &data.l,4,1,ff->fp) < 1)  break;
		}
		break;
	case -32:
		for( i=0;  i<framesz;  ++i)
		{
		  union
		  {	float f;
			unsigned char b[4];
		  } data;
		  data.f= (float)
			  ((DoubleOfAnyp(buff,BITPIX_FLOAT,i)-ff->bzero) / ff->bscale);
#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  {
			unsigned char hlp;
			hlp= data.b[0];  data.b[0]= data.b[3];  data.b[3]= hlp;
			hlp= data.b[1];  data.b[1]= data.b[2];  data.b[2]= hlp;
		  }
#endif
		  if( fwrite( &data.f,sizeof(float),1,ff->fp) < 1)  break;
		}
		break;
	default:
	case -64:
		for( i=0;  i<framesz;  ++i)
		{
		  union
		  {	double d;
			unsigned char b[8];
		  } data;
		  data.d= ((DoubleOfAnyp(buff,BITPIX_FLOAT,i)-ff->bzero) / ff->bscale);

#if defined(i386) || defined(__x86_64) || defined(__alpha)
		  {
			unsigned char hlp;
			hlp= data.b[0];  data.b[0]= data.b[7];  data.b[7]= hlp;
			hlp= data.b[1];  data.b[1]= data.b[6];  data.b[6]= hlp;
			hlp= data.b[2];  data.b[2]= data.b[5];  data.b[5]= hlp;
			hlp= data.b[3];  data.b[3]= data.b[4];  data.b[4]= hlp;
		  }
#endif
		  if( fwrite( &data.d,sizeof(double),1,ff->fp) < 1)  break;
		}
		break;
    }
    if( i<framesz)  {	err=2;	break;	}

    SetCurFrame(fimg,fr);
    while( Tcl_DoOneEvent(TK_ALL_EVENTS|TCL_DONT_WAIT)) ;
  } /** loop over frames **/

  SetCurFrame(fimg,curframe);

  switch(ff->bitpix)
  {	case   8: filesz= sizeof(unsigned char); break;
	case  16: filesz= sizeof(short);	break;
	case  32: filesz= sizeof(long);		break;
	case -32: filesz= sizeof(float);	break;
	default:
	case -64: filesz= sizeof(double);	break;
  }
  filesz *= framesz * ff->n3;

  if( err==0 && (i= filesz%2880)!=0)	/** no multiple of 2880 **/
  {
	filesz += ff->HeaderSize;
	filesz += 2880 - i;		/** round up **/
	if( 0==fseek(ff->fp,filesz-sizeof(unsigned char),SEEK_SET))
	{	unsigned char data= 42;
		/*printf(" Padding with %ld bytes\n",2880-i);*/
		fwrite( &data,sizeof(unsigned char),1,ff->fp);
	} /** NOTE: this won't work for remote writes! **/
  }
  if(name)				/** don't close stdout!!! **/
	if( rfopen_pipe? pclose(ff->fp) : fclose(ff->fp))
	{	fprintf(stderr," Error while closing file!\a\n");  err=1;	}

  ff->fp= NULL;
  return err;
}

/********************************* MAIN *********************************/

static int Movie_AppInit(Tcl_Interp *interp)
{
  if(Tcl_Init(interp)	 == TCL_ERROR ||
     Tk_Init(interp)	  == TCL_ERROR ||
     Tkpgplot_Init(interp) == TCL_ERROR ||
     FitsImage_Init(interp) == TCL_ERROR)
    return TCL_ERROR;

  pgtcl_init(interp);
  return TCL_OK;
}

int main( int argc, char **argv)
{
  Tk_Main(argc, argv, Movie_AppInit);
  return 0;
}
