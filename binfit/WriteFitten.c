/* $Id$
 * $Log$
 * Created:     Thu Aug  6 18:33:59 1998 by Koehler@cathy
 * Last change: Thu Aug 27 10:22:22 1998
 *
 * This file is Hellware!
 * It may send you and your data straight to hell without further notice.
 */

#include "fit.h"
#include "fitsio.h"

extern char *EVisFName, *EPhaFName, *CSVisFNm, *CSPhaFNm,
	    *fitten_out, *bff_out, *CSDVisFNm, *CSDPhaFNm;
extern char *VisFName, *DVisFName, *PhaFName, *DPhaFName, *MaskFName, *Title;
extern double BestFit[MAX_PARAM], BestVisF;
extern long radius;

void WriteFitten(void)
{
	FILE *outfp;
	long c;
	short fp_remote;

  if( !(outfp= rfopen(fitten_out,"w")))
  {	fprintf(stderr," Can't open %s\a\n",fitten_out);	}
  else
  {	fp_remote= rfopen_pipe;
	if( verbose_level >= 1)
		fprintf(stderr," Writing fit-file %s...\n",fitten_out);

	fprintf(outfp,"         :  Number of companions\n");
	fprintf(outfp,"ANZAHL   :  %d\n", N_comp);
	fprintf(outfp,"         :  Primary at origin, flux 1.0\n");
	fprintf(outfp,"         :  Pos.-Angle  Distance  Flux\n");
      for( c=0;  c<3*N_comp;  c+=3)
	fprintf(outfp,"BEGLEITER:\t%10.5f  %10.5f  %10.5f\n",
				BestFit[ANGLE1+c], BestFit[DIST1+c], BestFit[BRGTN1+c]);
	fprintf(outfp,"         :  Position of center of light\n");
	fprintf(outfp,"         :  Pos.-Angle  Distance\n");
	fprintf(outfp,"SCHWERPUN:  %10.5f  %10.5f  0  0\n",
				BestFit[CE_ANGLE], BestFit[CE_DIST] );
	fprintf(outfp,"         :  Visibilityfactor\n");
	fprintf(outfp,"VISIBFAKT:  %10.5f  0\n",BestVisF );
	fprintf(outfp,"RADIUS   :  %ld\n",radius);
	if( VisFName)	fprintf(outfp,"VISIBILITY: %s\n",VisFName );
	if( DVisFName)	fprintf(outfp,"DELTA VISI: %s\n",DVisFName);
	if( PhaFName)	fprintf(outfp,"PHASE     : %s\n",PhaFName );
	if( DPhaFName)	fprintf(outfp,"DELTA PHAS: %s\n",DPhaFName);
	if( MaskFName)	fprintf(outfp,"MASK      : %s\n",MaskFName);
	if( Title)	fprintf(outfp,"TITLE    : %s\n", Title	);
	fp_remote? pclose(outfp) : fclose(outfp);
  }
}
