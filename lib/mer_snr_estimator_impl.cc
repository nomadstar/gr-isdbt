/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mer_snr_estimator_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/isdbt/metrics_board.h>

#include <algorithm>
#include <cmath>

namespace gr {
namespace isdbt {

mer_snr_estimator::sptr mer_snr_estimator::make(const std::string& constellation)
{
    return gnuradio::get_initial_sptr(new mer_snr_estimator_impl(constellation));
}

mer_snr_estimator_impl::mer_snr_estimator_impl(const std::string& constellation)
    : gr::sync_block("mer_snr_estimator",
                      gr::io_signature::make(1, 1, sizeof(gr_complex)),
                      gr::io_signature::make(0, 0, 0)),
      d_constellation(constellation)
{
    init_luts();
    d_last_calc_time = std::chrono::steady_clock::now();
}

mer_snr_estimator_impl::~mer_snr_estimator_impl() {}

void mer_snr_estimator_impl::init_luts()
{
    const float sqrt2 = std::sqrt(2.0f);
    d_qpsk_lut = { gr_complex(1, 1) / sqrt2, gr_complex(1, -1) / sqrt2,
                   gr_complex(-1, 1) / sqrt2, gr_complex(-1, -1) / sqrt2 };

    const float v16[4] = { -3, -1, 1, 3 };
    const float sqrt10 = std::sqrt(10.0f);
    d_qam16_lut.clear();
    for (float x : v16)
        for (float y : v16) d_qam16_lut.push_back(gr_complex(x, y) / sqrt10);

    const float v64[8] = { -7, -5, -3, -1, 1, 3, 5, 7 };
    const float sqrt42 = std::sqrt(42.0f);
    d_qam64_lut.clear();
    for (float x : v64)
        for (float y : v64) d_qam64_lut.push_back(gr_complex(x, y) / sqrt42);
}

float mer_snr_estimator_impl::calc_mer_candidate(const std::vector<gr_complex>& rx_symbols_norm,
                                                  const std::vector<gr_complex>& lut) const
{
    std::size_t test_n = std::min<std::size_t>(500, rx_symbols_norm.size());

    float best_angle = 0.0f;
    float min_error = std::numeric_limits<float>::infinity();

    // Blind phase sweep, -45..45 degrees in 2 degree steps (same grid as
    // the Python reference's np.arange(-45, 46, 2)).
    for (int deg = -45; deg <= 45; deg += 2) {
        float angle = deg * static_cast<float>(M_PI) / 180.0f;
        gr_complex rot = std::polar(1.0f, -angle);

        double sum_sq_err = 0.0;
        for (std::size_t i = 0; i < test_n; i++) {
            gr_complex rs = rx_symbols_norm[i] * rot;
            float best_d = std::numeric_limits<float>::infinity();
            for (const auto& lp : lut) {
                float d = std::norm(rs - lp);
                if (d < best_d) best_d = d;
            }
            sum_sq_err += best_d;
        }
        float err = test_n > 0 ? static_cast<float>(sum_sq_err / test_n) : 0.0f;
        if (err < min_error) {
            min_error = err;
            best_angle = angle;
        }
    }

    gr_complex best_rot = std::polar(1.0f, -best_angle);
    std::size_t n = rx_symbols_norm.size();
    std::vector<gr_complex> aligned(n), referencia(n);
    for (std::size_t i = 0; i < n; i++) {
        gr_complex a = rx_symbols_norm[i] * best_rot;
        aligned[i] = a;
        float best_d = std::numeric_limits<float>::infinity();
        gr_complex best_p = lut.empty() ? gr_complex(0, 0) : lut[0];
        for (const auto& lp : lut) {
            float d = std::norm(a - lp);
            if (d < best_d) {
                best_d = d;
                best_p = lp;
            }
        }
        referencia[i] = best_p;
    }

    double pot_ref = 0.0;
    for (const auto& r : referencia) pot_ref += std::norm(r);
    if (pot_ref <= 0.0) return -100.0f;

    std::complex<double> alpha_num = 0.0;
    for (std::size_t i = 0; i < n; i++) {
        alpha_num += std::conj(std::complex<double>(referencia[i])) *
                     std::complex<double>(aligned[i]);
    }
    std::complex<double> alpha = alpha_num / pot_ref;

    double p_senal = 0.0, p_error = 0.0;
    for (std::size_t i = 0; i < n; i++) {
        std::complex<double> estimado = alpha * std::complex<double>(referencia[i]);
        std::complex<double> error = std::complex<double>(aligned[i]) - estimado;
        p_senal += std::norm(estimado);
        p_error += std::norm(error);
    }
    p_senal /= std::max<std::size_t>(n, 1);
    p_error /= std::max<std::size_t>(n, 1);

    if (p_error > 0.0 && p_senal > 0.0) {
        return static_cast<float>(10.0 * std::log10(p_senal / p_error));
    }
    return -100.0f;
}

int mer_snr_estimator_impl::work(int noutput_items,
                                  gr_vector_const_void_star& input_items,
                                  gr_vector_void_star& /*output_items*/)
{
    const gr_complex* in = reinterpret_cast<const gr_complex*>(input_items[0]);
    auto now = std::chrono::steady_clock::now();

    if (noutput_items > 0 &&
        std::chrono::duration<double>(now - d_last_calc_time).count() > 0.1) {
        std::size_t n = static_cast<std::size_t>(noutput_items);
        std::size_t sample_n = std::min<std::size_t>(2000, n);

        double pot = 0.0;
        for (std::size_t i = 0; i < sample_n; i++) pot += std::norm(in[i]);
        pot /= std::max<std::size_t>(sample_n, 1);

        if (pot > 0.0) {
            float umbral = 0.2f * static_cast<float>(std::sqrt(pot));
            std::vector<gr_complex> activos;
            activos.reserve(sample_n);
            for (std::size_t i = 0; i < sample_n; i++) {
                if (std::abs(in[i]) >= umbral) activos.push_back(in[i]);
            }

            if (activos.size() >= 16) {
                double avg_abs_sq = 0.0;
                for (const auto& s : activos) avg_abs_sq += std::norm(s);
                avg_abs_sq /= activos.size();
                float norm_factor = static_cast<float>(std::sqrt(avg_abs_sq));

                std::vector<gr_complex> simbolos_norm(activos.size());
                for (std::size_t i = 0; i < activos.size(); i++)
                    simbolos_norm[i] = activos[i] / norm_factor;

                float mer_qpsk = calc_mer_candidate(simbolos_norm, d_qpsk_lut);
                float mer_16qam = calc_mer_candidate(simbolos_norm, d_qam16_lut);
                float mer_64qam = calc_mer_candidate(simbolos_norm, d_qam64_lut);

                float mer_db;
                if (d_constellation == "QPSK") mer_db = mer_qpsk;
                else if (d_constellation == "16QAM") mer_db = mer_16qam;
                else if (d_constellation == "64QAM") mer_db = mer_64qam;
                else {
                    if ((mer_16qam - mer_qpsk) < 5.5f) mer_db = mer_qpsk;
                    else if ((mer_64qam - mer_16qam) > 5.8f) mer_db = mer_64qam;
                    else mer_db = mer_16qam;
                }

                if (mer_db > -50.0f) {
                    mer_db = std::max(-100.0f, std::min(100.0f, mer_db));
                    datacast::metrics_board::instance().set_mer_db(mer_db);
                }
            }
        }
        d_last_calc_time = now;
    }

    return noutput_items;
}

} // namespace isdbt
} // namespace gr
