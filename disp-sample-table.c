#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dispers.h"
#include "disp-sample-table.h"
#include "error-messages.h"

static void     disp_sample_table_free          (disp_t *d);
static disp_t * disp_sample_table_copy          (const disp_t *d);

static cmpl disp_sample_table_n_value       (const disp_t *disp, double lam);

struct disp_class disp_sample_table_class = {
  .disp_class_id       = DISP_SAMPLE_TABLE,
  .model_id            = MODEL_NONE,

  .short_id            = "SampleTable",
  .full_id             = "SampleTableDispersion",

  .free                = disp_sample_table_free,
  .copy                = disp_sample_table_copy,

  .n_value             = disp_sample_table_n_value,
  .fp_number           = disp_base_fp_number,
  .n_value_deriv       = NULL,
  .apply_param         = NULL,

  .decode_param_string = disp_base_decode_param_string,
  .encode_param        = NULL,
};

static void
disp_sample_table_init (struct disp_sample_table dt[], int nb)
{
  dt->nb = nb;
  dt->table_ref = data_table_new (nb, 3 /* columns */);
}

static void
disp_sample_table_clear (struct disp_sample_table *dt)
{
  dt->nb = 0;
}

void
disp_sample_table_free (disp_t *d)
{
  struct disp_sample_table *dt = &d->disp.sample_table;
  if (dt->nb > 0)
    data_table_unref (dt->table_ref);
  disp_base_free (d);
}

disp_t *
disp_sample_table_copy (const disp_t *src)
{
  disp_t *res = disp_base_copy (src);
  struct disp_sample_table *dt = &res->disp.sample_table;
  if (dt->nb > 0)
    data_table_ref (dt->table_ref);
  return res;
}

cmpl
disp_sample_table_n_value (const disp_t *disp, double _lam)
{
  const struct disp_sample_table *dt = & disp->disp.sample_table;
  struct data_table *t = dt->table_ref;
  float lam = _lam;
  int j, nb = dt->nb;
  
  for (j = 0; j < nb-1; j++)
    {
      double jlam = data_table_get (t, j+1, 0);
      if (jlam >= lam)
	break;
    }

  if (j >= nb-1)
    return data_table_get (t, j, 1) + data_table_get (t, j, 2) * I;

  float lam1 = data_table_get (t, j, 0), lam2 = data_table_get (t, j+1, 0);
  int j1 = j, j2 = j+1;

  float a = (lam - lam1) / (lam2 - lam1);
  float complex n1 = data_table_get (t, j1, 1) - data_table_get (t, j1, 2) * I;
  float complex n2 = data_table_get (t, j2, 1) - data_table_get (t, j2, 2) * I;

  return n1 + (n2 - n1) * a;
}

disp_t *
disp_sample_table_new_from_mat_file (const char * filename)
{
  FILE * f;
  str_t row;
  disp_t *disp = NULL;
  double unit_factor = 1.0;
  enum disp_type dtype;

  f = fopen (filename, "r");

  if (f == NULL)
    {
      notify_error_msg (LOADING_FILE_ERROR, "Cannot open %s", filename);
      return NULL;
    }

  str_init (row, 64);

  str_getline (row, f);
  str_getline (row, f);

  if (strncasecmp (CSTR(row), "CAUCHY", 6) == 0)
    {
      dtype = DISP_CAUCHY;
    }
  else if (strncasecmp (CSTR(row), "angstroms", 9) == 0)
    {
      str_getline (row, f);
      unit_factor = 0.1;
      if (strncasecmp (CSTR(row), "NK", 2) == 0)
	dtype = DISP_SAMPLE_TABLE;
    }
  else if (strncasecmp (CSTR(row), "nm", 2) == 0)
    {
      str_getline (row, f);
      if (strncasecmp (CSTR(row), "NK", 2) == 0)
	dtype = DISP_SAMPLE_TABLE;
    }
    
  switch (dtype)
    {
    case DISP_SAMPLE_TABLE:
      {
	struct disp_sample_table *dt;
	struct data_table *table;
	long start_pos = ftell (f);
	int j, lines;

	disp = disp_new (DISP_SAMPLE_TABLE);
	dt = & disp->disp.sample_table;
	disp_sample_table_clear (dt);

	start_pos = ftell (f);

	for (lines = 0; ; )
	  {
	    float xd[3];
	    int read_status = fscanf (f, "%f %f %f\n", xd, xd+1, xd+2);
	    if (read_status == 3)
	      lines ++;
	    if (read_status == EOF)
	      break;
	  }

	if (lines < 2)
	  {
	    disp_free (disp);
	    disp = NULL;
	    break;
	  }

	fseek (f, start_pos, SEEK_SET);
	
	disp_sample_table_init (dt, lines);
	
	table = dt->table_ref;

	for (j = 0; j < lines; j++)
	  {
	    float *dptr = table->heap + 3 * j;
	    int read_status;
	    do
	      read_status = fscanf (f, "%f %f %f\n", dptr, dptr+1, dptr+2);
	    while (read_status < 3 && read_status != EOF);

	    if (read_status == EOF)
	      break;
	  }

	break;
      }	  
    case DISP_CAUCHY:
      notify_error_msg (LOADING_FILE_ERROR, "cauchy MAT files");
      break;
#if 0
      cn = disp->disp.cauchy.n;
      ck = disp->disp.cauchy.k;
      fscanf (f, "%lf %lf %lf %*f %*f %*f\n", cn, cn+1, cn+2);
      cn[1] *= 1e3;
      cn[2] *= 1e6;
      ck[0] = ck[1] = ck[2] = 0.0;
      break;
#endif
    default:
      notify_error_msg (LOADING_FILE_ERROR, "corrupted material card");
      break;
    }

  fclose (f);
  str_free (row);
  return disp;
}