/* $Id$
 * $Log$
 * Created:     Sat Jun 28 16:44:26 1997 by Koehler@Lisa
 * Last change: Fri Sep 14 15:26:33 2012
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fitsio.h"

#define REMSH "rsh"	/** for anything but HPUX **/

/************************************************************************
 * rfopen(filename,type) - Remote File Open				*
 * Opens file on local or remote machines				*
 * "filename" may be...							*
 * - NULL or ""	      => return stdin or stdout				*
 * - a plain filename => just calls fopen				*
 * - host:filename    => calls popen to start rsh "host" cat "filename"	*
 * - filename.gz      => calls popen to gzip/gzcat file			*
 * .rhosts on the remote machine must be set properly!			*
 * "type" must start with "r" for read or "w" for write access		*
 *	for calls to popen, type must be "r" or "w", nothing more!	*
 ************************************************************************/

short rfopen_pipe;	/** 1 if latest rfopen was using popen **/

FILE *rfopen( char *filename, const char *type)
{
	FILE *fp;
	char *colonptr, *cmd, *myhost;
	long len;

  rfopen_pipe=0;
  if( filename==NULL || *filename==0)
  {	return( type[0]=='w'? stdout : stdin);	}
  else
  if( (filename[1] != ':') &&			/** no windows-drive ("C:" etc.) **/
      (colonptr= strchr(filename,':')))		/** but there is a colon **/
  {
    if( !(myhost= getenv("HOST")) ||		/** don't know my name **/
	  strncasecmp(filename,myhost,strlen(myhost)))	/** file IS remote **/
    {
	rfopen_pipe=1;
	len= 4 + (colonptr-filename) + 8 + strlen(colonptr+1) + 4;
	if( !(cmd= AllocBuffer(len,"remote cmd line")))	 return NULL;
	*colonptr= 0;
	sprintf(cmd,"rsh %s 'cat %s\"%s\"'\n",
			filename, type[0]=='w'? ">! ":"", colonptr+1);
	*colonptr= ':';
	fprintf(stderr,"calling: %s",cmd);
	fp= popen(cmd,type);
	free(cmd);
	return fp;
    }
    else filename= colonptr+1;	/** skip hostname **/
  }
  len= strlen(filename);
  if( !strcmp( filename+len-3, ".gz") || !strcmp( filename+len-2, ".Z"))		/** compressed file? **/
  {
	rfopen_pipe=1;
	len += 8;	/** a bit more than needed **/
	if( !(cmd= AllocBuffer(len,"gzip cmd line")))	 return NULL;
	sprintf(cmd,"gzip %s %s\n", type[0]=='w'? ">":"-cd", filename);
//	fprintf(stderr,"rfopen calling: %s",cmd);
	fp= popen(cmd,type);
	free(cmd);
	return fp;
  }
  return fopen(filename,type);
}
