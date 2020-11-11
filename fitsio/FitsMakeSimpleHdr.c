/* $Id$
 * $Log$
 * Created:     Sat Jun 28 17:01:38 1997 by Koehler@Lisa
 * Last change: Sat Jun 28 17:01:49 1997
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

/****************************************************************
 * FitsMakeSimpleHeader(FitsFile)				*
 * Uses values in "FitsFile" to create a header suitable to be	*
 * written to disk.  Any old header will be discarded		*
 * Returns 1 upon succes or 0 if no memory for header		*
 ****************************************************************/

short FitsMakeSimpleHeader(struct FitsFile *ff)
{
	char *Header;
	long i;

  if( !(Header= AllocBuffer(2880,"new header")))	return 0;

  if( ff->Header)	free(ff->Header);
  ff->Header= Header;
  ff->HeaderSize= 2880;

  sprintf( Header    ,"SIMPLE  = %20s   / %-45s","T","FITS-File");
  sprintf( Header+ 80,"BITPIX  = %20d   / %-45s",ff->bitpix,"Data type");
  sprintf( Header+160,"NAXIS   = %20d   / %-45s",ff->naxis,"number of axes");
  sprintf( Header+240,"NAXIS1  = %20d   / %-45s",ff->n1, "number of pixels along 1.axis");
  sprintf( Header+320,"NAXIS2  = %20d   / %-45s",ff->n2, "number of pixels along 2.axis");
  if(ff->naxis>=3)
	sprintf(Header+400,"NAXIS3  = %20d   / %-45s",ff->n3, "number of pixels along 3.axis");
  else
	sprintf(Header+400,"%-80s","COMMENT = no NAXIS3");
  sprintf( Header+480,"OBJECT  = %20s   / %-45s",ff->object, "source name");
  sprintf( Header+560,"BSCALE  = %20g   / %-45s", 1.0, "Real = Tape * BSCALE + BZERO");
  sprintf( Header+640,"BZERO   = %20g   / %-45s", 0.0, "Real = Tape * BSCALE + BZERO");

  for( i=720;  i<2800;  i+=80)	sprintf(Header+i,"%-80s","COMMENT");
  sprintf(Header+2800,"%-79s","END");	/** die terminating 0 steht im letzten byte vom Header **/
  Header[2879]= ' ';			/** jetzt stehen lauter Spaces am Ende **/
  return 1;
}
