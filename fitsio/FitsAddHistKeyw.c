/* $Id$
 * $Log$
 * Created:     Sat Jun 28 17:03:57 1997 by Koehler@Lisa
 * Last change: Sat Nov  3 22:30:44 2001
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
#include <time.h>

/****************************************************************
 * FitsAddHistKeyw(FistFile,what_was_done)			*
 * Adds a line of the form					*
 * "HISTORY = '<what was done> on <date-string>'	"	*
 * into the Fits-header.					*
 ****************************************************************/

long FitsCp2Keyw( char *keyw, char *src, long i)
{
	while( i<79 && *src)	keyw[i++]= *src++;
	return i;
}

char *FitsAddHistKeyw( struct FitsFile *ff, char *action)
{
	static char Month[12][4]= { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	time_t Time;
	struct tm *tm;
	char *histkeyw, line[24];
	long i;

  if( !(histkeyw= FitsFindEmptyHdrLine(ff)))
  {
    if( !(histkeyw= findkeyword(ff,"END       ")))
    {	fprintf(stderr," No END - I'm afraid your FitsHeader is broken\a\n");
	return NULL;		/** this should never happen!!! **/
    }
    if( histkeyw < ff->Header + ff->HeaderSize - 80)
    {	/** hinter END is Platz, also schieb eine Zeile um eine Zeile weiter **/
	memcpy( histkeyw+70, histkeyw-10, 80);
    }
    else
    {	/** realloc dumps core here **/
	if( !(histkeyw= AllocBuffer(ff->HeaderSize+2890,"FitsHeader")))
	{ fprintf(stderr,
		    " couldn't allocate more space for Header - Sorry, You loose!\a\n");
	  return NULL;
	}
	memcpy( histkeyw,ff->Header,ff->HeaderSize);
	free(ff->Header);
	ff->Header= histkeyw;
	histkeyw += ff->HeaderSize;
	ff->HeaderSize += 2880;
	memset(histkeyw, ' ', 2880);	memcpy(histkeyw,"END",3);
	histkeyw= findkeyword(ff,"END       ");	/** da sollte jetzt Platz sein **/
    }
    histkeyw -= 10;	/** findkeyword() liefert ptr auf value! **/
  }
  time(&Time);
  tm= localtime(&Time);
  sprintf(line,"%4d %s %02d %02d:%02d:%02d", tm->tm_year+1900,
		Month[tm->tm_mon], tm->tm_mday,	tm->tm_hour, tm->tm_min, tm->tm_sec );
 /** Length: "1234 Abc 12 12:34:56" = 20 **/

  strcpy( histkeyw,"HISTORY = '");	/** 11 chars **/
  i= strlen(histkeyw);
  i= FitsCp2Keyw( histkeyw, action, i);
  i= FitsCp2Keyw( histkeyw, " on ", i);	/**  4 chars **/
  i= FitsCp2Keyw( histkeyw, line, i);	/** 20 chars **/

  histkeyw[i]= '\'';	/** i points to first char after string or position 79 **/
  while( ++i < 80)  histkeyw[i]= ' ';
  return histkeyw;
}
