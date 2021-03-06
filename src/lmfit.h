#ifndef __LMFIT_H
#define __LMFIT_H

#include <gsl/gsl_vector.h>
#include <gsl/gsl_multifit_nlin.h>

#include "fit-engine.h"
#include "str.h"

__BEGIN_DECLS

enum {
    LMFIT_GET_RESULTING_STACK = 0,
    LMFIT_PRESERVE_STACK = 1
};

typedef int (*gui_hook_func_t)(void *, float, const char *);


extern int  lmfit_iter(gsl_vector *x, gsl_multifit_function_fdf *f,
                       gsl_multifit_fdfsolver *s, const int max_iter,
                       double epsabs, double epsrel, int *nb_iter,
                       gui_hook_func_t hfun, void *hdata, int *user_stop);

__END_DECLS

#endif /* __LMFIT_H */
