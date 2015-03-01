#include <stdio.h>
#include <string.h>
#include "fit-params.h"
#include "dispers.h"

static void get_disp_param_name(const fit_param_t *fp, str_ptr buf);


void
set_model_param(fit_param_t *fpres, int lyr, enum disp_model_id model_id,
                int param_nb)
{
    fpres->id = PID_LAYER_N;
    fpres->layer_nb = lyr;
    fpres->model_id = model_id;
    fpres->param_nb = param_nb;
}

void
set_thick_param(fit_param_t *fpres, int lyr)
{
    fpres->id = PID_THICKNESS;
    fpres->layer_nb = lyr;
    fpres->model_id = 0;
    fpres->param_nb = 0;
}

void
get_param_name(const fit_param_t *fp, str_t name)
{
    switch(fp->id) {
    case PID_THICKNESS:
        str_printf(name, "T%i", fp->layer_nb);
        break;
    case PID_FIRSTMUL:
        str_printf(name, "1stmult");
        break;
    case PID_LAYER_N:
        get_disp_param_name(fp, name);
        break;
    default:
        str_printf(name, "###");
    }
}

void
get_full_param_name(const fit_param_t *fp, str_t name)
{
    switch(fp->id) {
    case PID_THICKNESS:
        str_printf(name, "T%i", fp->layer_nb);
        break;
    case PID_FIRSTMUL:
        str_printf(name, "1stmult");
        break;
    case PID_LAYER_N:
    {
        str_t dname;
        str_init(dname, 16);
        get_disp_param_name(fp, dname);
        str_printf(name, "Layer%d / %s", fp->layer_nb, CSTR(dname));
        str_free(dname);
        break;
    }
    default:
        str_printf(name, "###");
    }
}

void
get_disp_param_name(const fit_param_t *fp, str_ptr buf)
{
    struct disp_class *dclass;
    void *iter;

    if(fp->id != PID_LAYER_N) {
        str_copy_c(buf, "###");
        return;
    }

    for(iter = disp_class_next(NULL); iter; iter =  disp_class_next(iter)) {
        dclass = disp_class_from_iter(iter);
        if(dclass->model_id == fp->model_id) {
            break;
        }
    }

    if(iter) {
        dclass->encode_param(buf, fp);
    } else {
        str_copy_c(buf, "###");
    }
}

int
parse_fit_string(const char *s, seed_t *seed)
{
    int n;

    n = sscanf(s, "%lf-%lf,%lf", & seed->min, & seed->max, & seed->step);

    if(n == 3) {
        seed->type = SEED_RANGE;
        seed->seed = (seed->min + seed->max) / 2.0;
        return 0;
    }

    return 1;
}

struct fit_parameters *
fit_parameters_new(void) {
    return (struct fit_parameters *) ARRAY_NEW(fit_param_t);
}

void
fit_parameters_free(struct fit_parameters *s)
{
    free(s->values);
    free(s);
}

void
fit_parameters_clear(struct fit_parameters *s)
{
    s->number = 0;
}

void
fit_parameters_add(struct fit_parameters *lst, fit_param_t const * fp)
{
    size_t idx = lst->number;

    ARRAY_CHECK_ALLOC(lst, fit_param_t, idx);

    memcpy(lst->values + idx, fp, sizeof(fit_param_t));

    lst->number ++;
}

void
fit_parameters_remove(struct fit_parameters *lst, int index)
{
    size_t i;
    for (i = index; i + 1 < lst->number; i++) {
        lst->values[i] = lst->values[i + 1];
    }
    lst->number--;
}

struct seeds *
seed_list_new(void) {
    return (struct seeds *) ARRAY_NEW(seed_t);
}

void
seed_list_free(struct seeds *s)
{
    free(s->values);
    free(s);
}

void
seed_list_add_simple(struct seeds *s, double v)
{
    size_t idx = s->number;

    ARRAY_CHECK_ALLOC(s, seed_t, idx);

    s->values[idx].type = SEED_SIMPLE;
    s->values[idx].seed = v;

    s->number ++;
}

void
seed_list_add(struct seeds *s, const seed_t *v)
{
    size_t idx = s->number;
    ARRAY_CHECK_ALLOC(s, seed_t, idx);
    memcpy(s->values + idx, v, sizeof(seed_t));
    s->number ++;
}

void
seed_list_remove(struct seeds *lst, int index)
{
    size_t i;
    for (i = index; i + 1 < lst->number; i++) {
        lst->values[i] = lst->values[i + 1];
    }
    lst->number--;
}

static int
seed_write(writer_t *w, const seed_t *s)
{
    if (s->type == SEED_SIMPLE) {
        writer_printf(w, "value %g", s->seed);
    } else if (s->type == SEED_RANGE) {
        writer_printf(w, "range %g %g %g", s->min, s->max, s->step);
    } else {
        writer_printf(w, "?");
    }
    return 0;
}

int
seed_list_write(writer_t *w, const struct seeds *s)
{
    writer_printf(w, "seed-list %d", s->number);
    writer_newline_enter(w);
    int i;
    for (i = 0; i < s->number; i++) {
        if (i > 0) {
            writer_newline(w);
        }
        seed_write(w, s->values + i);
    }
    writer_newline_exit(w);
    return 1;
}

struct strategy *
strategy_new() {
    struct strategy *r;

    r = emalloc(sizeof(struct strategy));

    r->parameters = fit_parameters_new();
    r->seeds = seed_list_new();

    return r;
}

void
strategy_free(struct strategy *s)
{
    fit_parameters_free(s->parameters);
    seed_list_free(s->seeds);
    free(s);
}

int
fit_parameters_are_RI_fixed(struct fit_parameters *f)
{
    int j;

    for(j = 0; j < f->number; j++)
        if(f->values[j].id == PID_LAYER_N) {
            break;
        }

    return (j >= f->number);
}

int
fit_param_compare(const fit_param_t *a, const fit_param_t *b)
{
    if (a->id != b->id) return 1;
    if (a->layer_nb != b->layer_nb) return 1;
    if (a->id == PID_LAYER_N) {
        if (a->model_id != b->model_id) return 1;
        if (a->param_nb != b->param_nb) return 1;
    }
    return 0;
}

int
fit_parameters_find(const struct fit_parameters *lst, const fit_param_t *fp)
{
    int j;
    for(j = 0; j < lst->number; j++) {
        const fit_param_t *lfp = &lst->values[j];
        if (fit_param_compare(lfp, fp) == 0) {
            return j;
        }
    }
    return -1;
}

static void
fix_delete_layer(struct fit_parameters *lst, int index)
{
    size_t i;
    for (i = 0; i < lst->number; i++) {
        fit_param_t *fp = &lst->values[i];
        if (fp->id < PID_LAYER_INDIPENDENT && fp->layer_nb >= index) {
            if (fp->layer_nb > index) {
                fp->layer_nb --;
            } else {
                fp->id = PID_INVALID;
            }
        }
    }
}

static void
fix_insert_layer(struct fit_parameters *lst, int index)
{
    size_t i;
    for (i = 0; i < lst->number; i++) {
        fit_param_t *fp = &lst->values[i];
        if (fp->id < PID_LAYER_INDIPENDENT && fp->layer_nb >= index) {
            fp->layer_nb ++;
        }
    }
}

void
fit_parameters_fix_layer_shift(struct fit_parameters *lst, struct shift_info shift)
{
    if (shift.event == SHIFT_DELETE_LAYER) {
        fix_delete_layer(lst, shift.index);
    } else if (shift.event == SHIFT_INSERT_LAYER) {
        fix_insert_layer(lst, shift.index);
    }
}

static int
fit_param_write(writer_t *w, const fit_param_t *fp)
{
    static const char *id_name[] = {"thickness", "n", "firstmul"};
    static const char *model_name[] = {"ho", "cauchy", "lookup", "bruggeman"};
    int id = fp->id;
    writer_printf(w, "%s", (id >= PID_THICKNESS && id < PID_INVALID) ? id_name[id-1] : "invalid");
    if (id < PID_LAYER_INDIPENDENT) {
        writer_printf(w, " %d", fp->layer_nb);
        if (id == PID_LAYER_N) {
            const int mid = fp->model_id;
            const char *mname = (mid >= 1 && mid < MODEL_NONE) ? model_name[mid - 1] : "unknown";
            writer_printf(w, " %s %d", mname, fp->param_nb);
        }
    }
    return 0;
}

int
fit_parameters_write(writer_t *w, const struct fit_parameters *s)
{
    writer_printf(w, "fit-parameters %d", s->number);
    writer_newline_enter(w);
    int i;
    for (i = 0; i < s->number; i++) {
        if (i > 0) {
            writer_newline(w);
        }
        fit_param_write(w, s->values + i);
    }
    writer_newline_exit(w);
    return 1;
}
