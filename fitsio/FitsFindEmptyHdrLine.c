/* $Id$
 * $Log$
 * Created:     Sat Jun 28 17:03:08 1997 by Koehler@Lisa
 * Last change: Sat Jun 28 17:03:16 1997
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
 * FitsFindEmptyHdrLine(FitsFile)				*
 * Searches for an empty line within header of FitsFile.	*
 * A line is empty if it contains only "COMMENT" or is past	*
 * the "END" keyword but within the header record(s)		*
 * Retuns pointer to empty header line or NULL if none found	*
 ****************************************************************/

	/* 80 chars:   123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789. **/
#define EMPTY_COMMENT "COMMENT                                                                         "

char *FitsFindEmptyHdrLine(struct FitsFile *ff)
{
	long line;

  /** 1.line ist "SIMPLE", letzte Zeile ist "END" **/
  for( line=80;  line<ff->HeaderSize-80;  line+=80)
  {
	if( !strncmp( ff->Header+line,EMPTY_COMMENT,80))	break;
	if( !strncmp( ff->Header+line,"END       ",10))
	{	memcpy(ff->Header+line+80,ff->Header+line,80);	break;	}
  }
  return( line<ff->HeaderSize-80? ff->Header+line : NULL);
}
