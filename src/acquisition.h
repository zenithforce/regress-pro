#ifndef ACQUISITION_H
#define ACQUISITION_H

#include "defs.h"
#include "str.h"

struct fit_parameters;

// TODO: rename below AB to RPE. Optionally rename ELLISS and REFLECTOMETER to SE / SR.
enum system_kind {
    SYSTEM_UNDEFINED = 0,
    SYSTEM_REFLECTOMETER,
    SYSTEM_ELLISS_AB,
    SYSTEM_ELLISS_PSIDEL,
    SYSTEM_EXCEED_VALUE
};

#define SYSTEM_CHANNELS_NUMBER(k) ((k) == SYSTEM_UNDEFINED ? 0 : ((k) == SYSTEM_REFLECTOMETER ? 1 : 2))
#define SYSTEM_ACQUISITION_PARAMS_NUMBER(k) ((k) == SYSTEM_UNDEFINED ? 0 : ((k) == SYSTEM_REFLECTOMETER ? SR_ACQ_PARAMETERS_NB : ((k) == SYSTEM_ELLISS_AB ? 3 : 2)))

/* Ellipsometry parameters: */
struct rpe_acquisition_parameters {
    double aoi; /* Angle in degrees. */
    double analyzer;  /* Angle in degrees. */
};

/* Ellipsometry parameters: */
struct se_acquisition_parameters {
    double aoi; /* Angle in degrees. */
};

/* Reflectometry parameters */
struct sr_acquisition_parameters {
    double rmult;
};

struct acquisition_parameters {
    enum system_kind type;
    double bandwidth;
    double numap;
    union {
        struct rpe_acquisition_parameters rpe;
        struct se_acquisition_parameters  se;
        struct sr_acquisition_parameters  sr;
    } parameters;
};

extern void     acquisition_set_zero(struct acquisition_parameters *acq);
extern int      acquisition_apply_param(struct acquisition_parameters *acq, int param_id, double val);
extern void     acquisition_get_all_parameters(const struct acquisition_parameters *acq, struct fit_parameters *fps);
extern double   acquisition_get_parameter(const struct acquisition_parameters *acq, int param_id);
extern double * acquisition_parameter_pointer(struct acquisition_parameters *acq, int param_id);
extern void     acquisition_set_default(struct acquisition_parameters *acq);
extern void     acquisition_set_default_rpe(struct acquisition_parameters *acq);
extern void     acquisition_set_default_se(struct acquisition_parameters *acq);
extern void     acquisition_parameter_to_string(str_t name, int param_id);
extern double   acquisition_get_se_aoi(const struct acquisition_parameters *acquisition);
extern double   acquisition_get_se_analyzer(const struct acquisition_parameters *acquisition);

extern const char * acquisition_parameter_name(int param_id);

#endif
