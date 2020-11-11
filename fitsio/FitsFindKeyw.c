/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:48:57 1997 by Koehler@Lisa
 * Last change: Thu Oct 31 22:41:51 2002
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsio.h"

/****************************************************************
 * FitsFindKeyw(FitsFile,Keyword)				*
 * returns pointer to VALUE of line starting with "Keyword"	*
 * in header of "FitsFile" or NULL if "Keyword" not in header	*
 * NOTE: "Keyword" MUST be 10 chars long!!!			*
 ****************************************************************/

char *FitsFindKeyw( struct FitsFile *ff, char *keyw)
{
	long line;

#ifdef DEBUG
	fprintf(stderr," looking for %s\n",keyw);
#endif

  for( line=80;  line<ff->HeaderSize;  line+=80)	/** 1.line ist "SIMPLE" **/
  {
    /** check search key first, in case we search for END **/
	if( !strncmp(ff->Header+line,keyw,10))	break;
	if( !strncmp(ff->Header+line,"END       ",10))
		return NULL;
  }
  return( line<ff->HeaderSize? ff->Header+line+10 : NULL);
}
