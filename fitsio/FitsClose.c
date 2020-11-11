/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:50:12 1997 by Koehler@Lisa
 * Last change: Wed Oct 24 17:27:50 2001
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsio.h"

/********************************************************
 * FitsClose(FitsFile)					*
 * Closes file associated with FitsFile, free()s Buffer,*
 * Header and FitsFile itself (all only if necessary)	*
 ********************************************************/

void FitsClose(struct FitsFile *ff)
{
  if( !ff)	return;		/** war wohl nix **/

  if( ff->fp && !(ff->flags & FITS_FLG_DONT_CLOSE))
  { if(ff->flags & FITS_FLG_PIPE)
	pclose(ff->fp);		/** ignore errors, probably just "broken pipe" **/
    else
      if( fclose(ff->fp))
	fprintf(stderr,"Error while closing file %s!\a\n",ff->filename);
  }

  if( ff->Buffer.c) free(ff->Buffer.c);
  if( ff->Header) free(ff->Header);
  free(ff);
}
