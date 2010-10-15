
/*
  $Id: refl-kernel.h,v 1.1 2006/04/07 11:59:52 af27 Exp $
*/

#ifndef REFL_KERNEL_H
#define REFL_KERNEL_H

#include "common.h"
#include "cmpl.h"
#include "spectra.h"

#include <gsl/gsl_vector.h>


/* reflectivity for normal incidence with multi-layer film */
double mult_layer_refl_ni (size_t nb /*nb of mediums */,
			   const cmpl ns[], const double ds[],
			   double lambda,
			   gsl_vector *rjacob_th, gsl_vector *rjacob_n);

#endif