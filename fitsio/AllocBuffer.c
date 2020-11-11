/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:46:44 1997 by Koehler@Lisa
 * Last change: Mon Apr 16 13:31:26 2012
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>	/* for sleep() */
#include "fitsio.h"

/************************************************************************
 * AllocBuffer(size,zweck)						*
 * malloc()s buffer of size "size" bytes, complains if size <= 0	*
 * Tells user "no memory for 'zweck'" if buffer couldn't be alloced	*
 * Result: ptr to buffer or NULL if no memory				*
 ************************************************************************/

void *AllocBuffer(unsigned long size, char *zweck)
{	char *ptr;
	unsigned int cnt;

  if(size <= 0)
  {	fprintf(stderr,"AllocBuffer: negative size requested for %s\a\n",zweck);  return NULL;	}

  size += 20;

#ifdef DEBUG
	fprintf(stderr," Going to alloc %d bytes..",size);	fflush(stderr);
#endif

  for( cnt=1;  cnt<10;  cnt++)
  {
    if( (ptr= malloc(size)))	break;
    sleep(cnt);
  }
  if( ptr==NULL)
  {	fprintf(stderr," No memory for %s!\n",zweck);	return NULL;	}

#ifdef DEBUG
	fprintf(stderr,"..ok\n");
#endif
  return ptr;
}
