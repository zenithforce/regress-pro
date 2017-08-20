#include <algorithm>
#include <cmath>

#include <gsl/gsl_interp.h>

#include "dispers_sampling_optim.h"

template <typename T, unsigned N>
struct tab_array {
    tab_array(int size): m_size(size), m_array(N * size) { }

    int size() const { return m_size; }

    const double *row(int k) const { return m_array.data() + k * m_size; }
          double *row(int k)       { return m_array.data() + k * m_size; }

    const T& at(int i, int j) const { return m_array[i + m_size * j]; }
          T& at(int i, int j)       { return m_array[i + m_size * j]; }

private:
    int m_size;
    pod_array<double> m_array;
};

/* Provide accessors for elements from rc_matrix.
   We assume that data are stored in column-major form. */
class tab_matrix {
public:
    tab_matrix(const rc_matrix *m): m_matrix(m) { }

    int size() const { return m_matrix->view.matrix.size2; }
    double operator()(int i, int j) const { return m_matrix->view.matrix.data[j * m_matrix->view.matrix.tda + i]; }
private:
    const rc_matrix *m_matrix;
};

template <typename T>
void insert_at(pod_vector<T>& vec, int index, const T value) {
    int required_capacity = vec.size() + 1;
    if (vec.capacity() < required_capacity) {
        vec.resize(required_capacity * 3 / 2);
    }
    vec.insert_at(index, value);
}

class cspline_array_interp {
public:
    using double_pair = std::pair<double, double>;

    cspline_array_interp(const tab_array<double, 3>& array)
    : m_table(array) {
        const int length = m_table.size();
        m_interp_n = gsl_interp_alloc(gsl_interp_cspline, length);
        m_interp_k = gsl_interp_alloc(gsl_interp_cspline, length);
        m_accel = gsl_interp_accel_alloc();
        compute_interpolation();
    }

    ~cspline_array_interp() {
        gsl_interp_free(m_interp_n);
        gsl_interp_free(m_interp_k);
        gsl_interp_accel_free(m_accel);
    }

    void compute_interpolation() {
        gsl_interp_init(m_interp_n, m_table.row(0), m_table.row(1), m_table.size());
        gsl_interp_init(m_interp_k, m_table.row(0), m_table.row(2), m_table.size());
    }

    double_pair eval(const double wavelength) const {
        const double n = gsl_interp_eval(m_interp_n, m_table.row(0), m_table.row(1), wavelength, m_accel);
        const double k = gsl_interp_eval(m_interp_k, m_table.row(0), m_table.row(2), wavelength, m_accel);
        return double_pair(n, k);
    }
private:
    const tab_array<double, 3>& m_table;

    gsl_interp *m_interp_n;
    gsl_interp *m_interp_k;
    gsl_interp_accel *m_accel;
};

tab_array<double, 3> get_array(const pod_vector<int>& ipoints, const tab_matrix& ms) {
    tab_array<double, 3> table(ipoints.size());
    for (int i = 0; i < ipoints.size(); i++) {
        const int k = ipoints[i];
        table.at(i, 0) = ms(k, 0);
        table.at(i, 1) = ms(k, 1);
        table.at(i, 2) = ms(k, 2);
    }
    return table;
}

int subsampling_eval(const tab_matrix& ms, const pod_vector<int>& ipoints, const double tol) {
    tab_array<double, 3> table = get_array(ipoints, ms);
    cspline_array_interp interp(table);
    for (int i = 0; i < ms.size(); i++) {
        auto nki = interp.eval(ms(i, 0));
        auto nkr = std::pair<double, double>(ms(i, 1), ms(i, 2));
        if (std::abs(nki.first  - nkr.first ) > tol ||
            std::abs(nki.second - nkr.second) > tol) {
            return i;
        }
    }
    return -1;
}

template <typename T> T square(T a) { return a * a; }

double interp_delta_score(const cspline_array_interp& interp, const tab_matrix& ms, const pod_vector<int>& ipoints, const int i_min, const int i_max) {
    double del_n = 0, del_k = 0;
    for (int i = ipoints[i_min]; i < ipoints[i_max]; i++) {
        auto nki = interp.eval(ms(i, 0));
        auto nkr = std::pair<double, double>(ms(i, 1), ms(i, 2));
        del_n += square(nki.first  - nkr.first );
        del_k += square(nki.second - nkr.second);
    }
    int points_number = ipoints[i_max] - ipoints[i_min];
    return sqrt(del_n / points_number) + sqrt(del_k / points_number);
}

std::pair<int, double> find_delta_optimal(const tab_matrix& ms, pod_vector<int>& ipoints, int i, int ka, int kb) {
    int kbest = ka;
    double del = 1000.0;

    tab_array<double, 3> table = get_array(ipoints, ms);
    cspline_array_interp interp(table);

    for (int k = ka + 1; k <= kb - 1; k++) {
        table.row(0)[i] = ms(k, 0);
        table.row(1)[i] = ms(k, 1);
        table.row(2)[i] = ms(k, 2);
        interp.compute_interpolation();
        double kdel = interp_delta_score(interp, ms, ipoints, i - 1, i + 1);
        if (kdel < del) {
            kbest = k;
            del = kdel;
        }
    }
    return std::pair<int, double>(kbest, del);
}

void insert_opt_point(const tab_matrix& ms, pod_vector<int>& ipoints, int i) {
    int ka = ipoints[i], kb = ipoints[i+1];
    insert_at(ipoints, i + 1, ka + 1);
    auto best = find_delta_optimal(ms, ipoints, i + 1, ka, kb);
    ipoints[i+1] = best.first;
}

void add_new_point(const tab_matrix& ms, pod_vector<int>& ipoints, int i_nok) {
    const double wi = ms(i_nok, 0);
    for (int i = 0; i < ipoints.size(); i++) {
        const double wa = ms(ipoints[i    ], 0);
        const double wb = ms(ipoints[i + 1], 0);
        if (wa < wi && wb > wi) {
            insert_opt_point(ms, ipoints, i);
            break;
        }
    }
}

pod_vector<int> optimize_sampling_points(const disp_sample_table *disp, const double approx_tolerance) {
    const tab_matrix ms(disp->table);

    pod_vector<int> ipoints(16);
    ipoints.push_back(0);
    ipoints.push_back(ms.size() - 1);

    add_new_point(ms, ipoints, ms.size() / 2);

    while (true) {
        int i_nok = subsampling_eval(ms, ipoints, approx_tolerance);
        if (i_nok < 0) break;
        add_new_point(ms, ipoints, i_nok);
    }

    return ipoints;
}

tab_array<double, 3> get_array_c(const pod_vector<double>& sampling_points, const disp_t *disp) {
    tab_array<double, 3> table(sampling_points.size());
    for (int i = 0; i < sampling_points.size(); i++) {
        const double wavelength = sampling_points[i];
        const cmpl n = n_value(disp, wavelength);
        table.at(i, 0) = wavelength;
        table.at(i, 1) =  std::real(n);
        table.at(i, 2) = -std::imag(n);
    }
    return table;
}

double interp_delta_score_c(const cspline_array_interp& interp, const disp_t *disp, double wavelength_inf, double wavelength_sup) {
    const int test_points_number = 32;
    double del_n = 0, del_k = 0;
    for (int k = 1; k < test_points_number; k++) {
        const double wavelength = wavelength_inf + (wavelength_sup - wavelength_inf) * k / test_points_number;
        auto nki = interp.eval(wavelength);
        cmpl nkr = n_value(disp, wavelength);
        del_n = std::max(del_n, std::abs(nki.first  - std::real(nkr)));
        del_k = std::max(del_k, std::abs(nki.second + std::imag(nkr)));
    }
    return std::max(del_n, del_k);
}

void add_new_point_c(const disp_t *disp, pod_vector<double>& sampling_points, int index) {
    const int search_points_number = 16;

    const double wavelength_inf = sampling_points[index];
    const double wavelength_sup = sampling_points[index + 1];

    insert_at(sampling_points, index + 1, (wavelength_sup + wavelength_inf) / 2);

    double wavelength_best;
    double del = -1;

    tab_array<double, 3> table = get_array_c(sampling_points, disp);
    cspline_array_interp interp(table);

    for (int k = 1; k < search_points_number; k++) {
        const double wavelength = wavelength_inf + (wavelength_sup - wavelength_inf) * k / search_points_number;
        const cmpl n = n_value(disp, wavelength);
        table.row(0)[index + 1] = wavelength;
        table.row(1)[index + 1] =  std::real(n);
        table.row(2)[index + 1] = -std::imag(n);
        interp.compute_interpolation();
        double kdel = interp_delta_score_c(interp, disp, wavelength_inf, wavelength_sup);
        if (del < 0 || kdel < del) {
            wavelength_best = wavelength;
            del = kdel;
        }
    }
    sampling_points[index + 1] = wavelength_best;
}

int subsampling_eval_c(const disp_t *disp, const pod_vector<double>& sampling_points, const double tol) {
    const int test_points_number = 32;
    tab_array<double, 3> table = get_array_c(sampling_points, disp);
    cspline_array_interp interp(table);
    for (int i = 0; i < sampling_points.size() - 1; i++) {
        const double wavelength_inf = sampling_points[i];
        const double wavelength_sup = sampling_points[i + 1];
        for (int k = 0; k < test_points_number; k++) {
            const double wavelength = wavelength_inf + (wavelength_sup - wavelength_inf) * k / test_points_number;
            auto nki = interp.eval(wavelength);
            const cmpl nkr = n_value(disp, wavelength);
            if (std::abs(nki.first  - std::real(nkr)) > tol ||
                std::abs(nki.second + std::imag(nkr)) > tol) {
                return i;
            }
        }
    }
    return -1;
}

pod_vector<double> optimize_sampling_points_c(const disp_t *disp, const double approx_tolerance)
{
    double wavelength_start, wavelength_end;
    int unused;
    disp_get_wavelength_range(disp, &wavelength_start, &wavelength_end, &unused);
    pod_vector<double> sampling_points(16);
    sampling_points.push_back(wavelength_start);
    sampling_points.push_back(wavelength_end);

    add_new_point_c(disp, sampling_points, 0);

    while (true) {
        int i_nok = subsampling_eval_c(disp, sampling_points, approx_tolerance);
        if (i_nok < 0) break;
        add_new_point_c(disp, sampling_points, i_nok);
    }

    return sampling_points;
}

disp_t *dispersion_from_sampling_points(const disp_sample_table *disp, const pod_vector<int>& ipoints, const char *name) {
    const tab_matrix src(disp->table);
    disp_t *new_disp = disp_new(DISP_SAMPLE_TABLE);
    disp_set_name(new_disp, name);
    disp_sample_table * const st_disp = &new_disp->disp.sample_table;
    const int sampling_points = ipoints.size();
    disp_sample_table_init(st_disp, sampling_points);
    gsl_matrix * const st_matrix = &st_disp->table->view.matrix;
    for (int i = 0; i < sampling_points; i++) {
        st_matrix->data[i                     ] = src(ipoints[i], 0);
        st_matrix->data[i + 1 * st_matrix->tda] = src(ipoints[i], 1);
        st_matrix->data[i + 2 * st_matrix->tda] = src(ipoints[i], 2);
    }
    disp_sample_table_prepare_interp(st_disp);
    return new_disp;
}

disp_t *dispersion_from_sampling_points(const disp_t *disp, const pod_vector<double>& sampling_points, const char *name) {
    disp_t *new_disp = disp_new(DISP_SAMPLE_TABLE);
    disp_set_name(new_disp, name);
    disp_sample_table * const st_disp = &new_disp->disp.sample_table;
    const int points_number = sampling_points.size();
    disp_sample_table_init(st_disp, points_number);
    gsl_matrix * const st_matrix = &st_disp->table->view.matrix;
    for (int i = 0; i < points_number; i++) {
        const double wavelength = sampling_points[i];
        const cmpl n = n_value(disp, wavelength);
        st_matrix->data[i                     ] = wavelength;
        st_matrix->data[i + 1 * st_matrix->tda] =  std::real(n);
        st_matrix->data[i + 2 * st_matrix->tda] = -std::imag(n);
    }
    disp_sample_table_prepare_interp(st_disp);
    return new_disp;
}
