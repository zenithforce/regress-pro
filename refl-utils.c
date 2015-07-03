#include <stdio.h>
#include <string.h>

#include "refl-utils.h"
#include "error-messages.h"
#include "data-table.h"

#define NORMALIZE(c) ((float) c / 32020.0)

static double
get_lambda(double w[], int k)
{
    return w[0] + k * w[1] + k*k * w[2];
}

struct data_table *
read_nova_spectrum(FILE *f, str_ptr ln) {
    struct data_table *table;
    unsigned int c, c_save;
    int na, j, lc0 = 0, lcount;
    long dpos;
    int starting_zeroes = 1;
    int ezcount = 0; /* Number of zeroes at the end of file. */
    double w[5];

    for(;;) {
        char tc;
        if(str_getline(ln, f) < 0) {
            break;
        }
        na = sscanf(CSTR(ln), "%u %c", &c, &tc);

        if(na == 1) {
            if(c > 0 && starting_zeroes) {
                c_save = c;
                dpos = ftell(f);
                starting_zeroes = 0;
                lcount = 1;
                continue;
            } else if (c > 0 && ezcount > 0) {
                lcount += ezcount;
                ezcount = 0;
            }

            if(c == 0) {
                if(!starting_zeroes) {
                    ezcount ++;
                } else {
                    lc0++;
                }
            }

            if(!starting_zeroes && ezcount == 0) {
                lcount++;
            }
            continue;
        } else if(na == 2 && lcount > 0) {
            const char *p = CSTR(ln);
            if(sscanf(p, "%lf %lf %lf %lf %lf \n", w, w+1, w+2, w+3, w+4) == 5) {
                break;
            }
        }

        return NULL;
    }

    if(fseek(f, dpos, SEEK_SET) < 0) {
        return NULL;
    }

    table = data_table_new(lcount, 2);
    data_table_set(table, 0, 0, get_lambda(w, lc0+1));
    data_table_set(table, 0, 1, NORMALIZE(c_save));

    for(j = 1; j < lcount; j++) {
        fscanf(f, " %u", &c);

        data_table_set(table, j, 0, get_lambda(w, lc0+j+1));
        data_table_set(table, j, 1, NORMALIZE(c));
    }

    return table;
}

struct spectrum *
load_refl_data(const char *filename, str_ptr *error_msg) {
    struct spectrum *s;
    struct data_table *table;
    str_t ln;
    FILE *f;

    f = fopen(filename, "r");

    if(f == NULL) {
        *error_msg = new_error_message(LOADING_FILE_ERROR, "File \"%s\" does not exists or cannot be opened", filename);
        return NULL;
    }

    str_init(ln, 64);

    s = emalloc(sizeof(struct spectrum));
    s->config.system = SYSTEM_REFLECTOMETER;

    str_getline(ln, f);

    if(strstr(CSTR(ln), ";Experimental Spectrum") || \
            strstr(CSTR(ln), ";Theoretical Spectrum")) {
        table = read_nova_spectrum(f, ln);
    } else {
        str_getline(ln, f);
        table = data_table_read_lines(f, "%*s %f %*f %f %*f\n", 0, 2);
    }

    if(table == NULL) {
        goto invalid_s;
    }

    data_view_init(s->table, table);

    str_free(ln);
    fclose(f);

    return s;
invalid_s:
    *error_msg = new_error_message(LOADING_FILE_ERROR, "Format of spectra \"%s\" is incorrect", filename);
    free(s);
    str_free(ln);
    fclose(f);
    return NULL;
}
