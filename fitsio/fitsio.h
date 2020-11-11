/* $Id: fitsio.h,v 1.5 1995/03/05 17:09:36 koehler Exp koehler $
 *
 * Last Change: Thu Aug  2 17:59:09 2012
 *
 * $Log: fitsio.h,v $
 * Revision 1.5  1995/03/05  17:09:36  koehler
 *
 * Revision 1.4  1995/02/17  14:59:52  koehler
 * FitWrite() etc. eingefuehrt
 *
 * Revision 1.3  1995/01/17  11:04:47  koehler
 * letzte Version vor Calar Alto
 *
 * Revision 1.2  1995/01/02  21:25:15  koehler
 * added "anypntr"
 *
 * Revision 1.1  1994/12/29  17:06:18  RK
 * Initial revision
 */

union anypntr			/** pointer to some fits image data **/
{	unsigned char *c;
	short *s;
	long  *l;
	float *f;
	double *d;
};

union anyvar			/** variable containing some fits image data **/
{	unsigned char c;
	unsigned char byte[8];
	short s;
	int   l;	/** long on 64-bit machines is 8 bytes, int is always 32 bytes (?) **/
	float f;
	double d;
};


struct FitsFile
{	long flags;
	char *filename;
	FILE *fp;
	short bitpix;
	short naxis, n1, n2, n3;	/** mehr brauchen wir nicht **/
	char object[22];
	double bscale,bzero;
	char *Header;
	long HeaderSize;
	union anypntr Buffer;
	char *SA_hdrline;		/** SAVEAREA in Header (ptr to '[xmin:...]') **/
	short SA_xmin, SA_xmax;		/** SAVEAREA **/
	short SA_ymin, SA_ymax;
	double (*ReadDblPixel)(struct FitsFile *);
};

#define FITS_FLG_DONT_CLOSE	1	/** set if fp is stdin/out/err **/
#define FITS_FLG_EOF		2	/** at End-of-File **/
#define FITS_FLG_ERROR		4	/** IO-Error	**/
#define FITS_FLG_PIPE		8	/** got FILE from popen() **/
#define FITS_FLG_SHARP		16	/** File is in SHARP format **/
#define FITS_FLG_NEW_SHARP	32	/** File is FITS, but unsigned short **/

#define BITPIX_UBYTE	8
#define BITPIX_SHORT	16
#define BITPIX_LONG	32
#define BITPIX_FLOAT	-32
#define BITPIX_DOUBLE	-64


extern short	rfopen_pipe;	/** TRUE if latest rfopen was a popen() **/
extern FILE	*rfopen( char *filename, const char *type);
extern void	*AllocBuffer(unsigned long size, char *zweck);
extern short	CheckPhysRAM(float limit);
extern short	FitsWordSize(struct FitsFile *Fits);
extern char	*FitsFindKeyw( struct FitsFile *ff, char *keyw);  /** keyw muss 10 chars lang sein!! **/
extern struct FitsFile	*CopyFits(struct FitsFile *);
extern struct FitsFile	*FitsOpenR( char *filename);
extern struct FitsFile *LoadFitsFile(
			char *filename, short bufbitpix,
			short (*FrameCallBack)(struct FitsFile *, long fr, void *userdata),
			void *userdata	);
extern short	FitsCheckError(struct FitsFile *ff);
extern void	FitsClose(struct FitsFile *ff);
extern short	FitsReadShort(struct FitsFile *ff);
extern double	FitsReadDouble(struct FitsFile *ff);
extern long	FitsReadDoubleBuf(struct FitsFile *ff,double *buf,long cnt);
extern double	DoubleOfAnyp( union anypntr ptr, short bitpix, long offs);
extern double	DoubleOfFitsFile( long offs, struct FitsFile *ff);
extern short	FitsMakeSimpleHeader(struct FitsFile *ff);
extern short	FitsSetHeaderLine(struct FitsFile *ff, char *keyw, char *new_line);
extern char *	FitsFindEmptyHdrLine(struct FitsFile *ff);
extern char *	FitsAddHistKeyw( struct FitsFile *ff, char *action);
extern void	FindMinMax(union anypntr buff,short bitpix,long size,double *min,double *max);
extern short	FitsWriteAny(char *name, struct FitsFile *ff, void *buff,short bufbitpix);
extern short	FitsWrite( char *name, struct FitsFile *ff, double *buff);


#define FitsWrite(nm,ff,bf)	FitsWriteAny((nm),(ff),(bf),-64)


/** for old code...**/

#define findkeyword(f,k)	FitsFindKeyw(f,k)
#define fits_close(f)		FitsClose(f)
#define fits_open_r(n)		FitsOpenR(n)
