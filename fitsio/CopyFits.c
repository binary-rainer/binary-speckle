/* $Id$
 * $Log$
 * Created:     Sat Jul 26 17:51:46 1997 by Koehler@Lisa
 * Last change: Sat Jul 26 18:15:23 1997
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
 * CopyFits(srcFits)						*
 * creates a copy of srcFits, including a copy of the Header	*
 * flags, fp, and Buffer will be set to NULL			*
 ****************************************************************/

struct FitsFile *CopyFits(struct FitsFile *src)
{
	struct FitsFile *dst;

  if( !(dst= AllocBuffer(sizeof(struct FitsFile),"copy of FitsFile")))
	return NULL;

  *dst = *src;
  dst->flags= 0;
  dst->fp   = NULL;
  dst->Buffer.c= NULL;

  if( !(dst->Header= AllocBuffer(dst->HeaderSize+10,"FitsHeader")))
  {	FitsClose(dst);  return NULL;	}

  memcpy(dst->Header,src->Header,src->HeaderSize);
  return dst;
}
