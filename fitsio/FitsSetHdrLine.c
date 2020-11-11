/* $Id$
 * $Log$
 * Created:     Sat Jun 28 17:02:30 1997 by Koehler@Lisa
 * Last change: Wed Mar  7 16:33:01 2012
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

/************************************************************************
 * FitsSetHeaderLine(FitsFile,Keyword,Line)				*
 * Searches for "Keyword" within header of "FitsFile" and replaces the	*
 * first matching line by "line" or a comment line if "line" is NULL	*
 * If keyword is not found, checks for ...				*
 *  1. an empty line (keyword = spaces)					*
 *  2. room behind END-keyword						*
 *  3. en empty COMMENT-line						*
 * If all else fails, the header will be enlarged			*
 * Returns 0 if successful or 1 of "Keyword" couldn't be found		*
 ************************************************************************/

short FitsSetHeaderLine(struct FitsFile *ff, char *keyw, char *new_line)
{
	char *keyval;


  if( !(keyval= FitsFindKeyw(ff,keyw)))
  { if( !new_line)  return 1;

    //fprintf(stderr,"FitsSetHeaderLine: '%s' '%s'\n",keyw,new_line);
    //    fprintf(stderr," Keyword '%s' not found\n",keyw);

    if( !(keyval= FitsFindKeyw(ff,"          ")))	/** empty line somewhere? **/
    {
      //fprintf(stderr," no empty line\n");

      if( !(keyval= FitsFindKeyw(ff,"END       ")))
      {	fprintf(stderr," No END - I'm afraid your FitsHeader is broken\a\n");
	return 42;		/** this should never happen!!! **/
      }
      if( keyval < ff->Header + ff->HeaderSize - 80)	/** Space behind END? **/
      {	/** Shift END-line by one line **/
	//fprintf(stderr," Shifting END to make room for keyword %s\n",keyw);
	memcpy( keyval+70, keyval-10, 80);
      }
      else
      if( !(keyval= FitsFindKeyw(ff,"COMMENT   ")))	/** empty comment line **/
      {	/** realloc dumps core here **/
	if( !(keyval= AllocBuffer(ff->HeaderSize+2890,"FitsHeader")))
	{ fprintf(stderr,
		" couldn't allocate more space for Header - Sorry, You loose!\a\n");
	  return 2;
	}
	//fprintf(stderr," Enlarging header to make room for keyword %s\n",keyw);
	//fprintf(stderr," old header: %ld, size %d\n",ff->Header,ff->HeaderSize);
	memcpy( keyval,ff->Header,ff->HeaderSize);
	free(ff->Header);
	ff->Header= keyval;
	keyval += ff->HeaderSize;
	memset(keyval, ' ', 2880);	memcpy(keyval,"END",3);
	ff->HeaderSize += 2880;
	keyval= FitsFindKeyw(ff,"END       ");	/** da sollte jetzt Platz sein **/
	//fprintf(stderr," new header: %ld, size %d\n",ff->Header,ff->HeaderSize);
      }
    }
  }
  if(new_line)	memcpy( keyval-10,new_line,80);
	else	memcpy( keyval-10,"COMMENT = ",10);
  return 0;
}
