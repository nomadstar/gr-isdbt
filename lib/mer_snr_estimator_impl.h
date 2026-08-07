/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_MER_SNR_ESTIMATOR_IMPL_H
#define INCLUDED_ISDBT_MER_SNR_ESTIMATOR_IMPL_H

#include <gnuradio/gr_complex.h>
#include <gnuradio/isdbt/mer_snr_estimator.h>

#include <chrono>
#include <string>
#include <vector>

namespace gr {
namespace isdbt {

class mer_snr_estimator_impl : public mer_snr_estimator
{
private:
    std::string d_constellation;
    std::vector<gr_complex> d_qpsk_lut;
    std::vector<gr_complex> d_qam16_lut;
    std::vector<gr_complex> d_qam64_lut;
    std::chrono::steady_clock::time_point d_last_calc_time;

    void init_luts();
    float calc_mer_candidate(const std::vector<gr_complex>& rx_symbols_norm,
                              const std::vector<gr_complex>& lut) const;

public:
    explicit mer_snr_estimator_impl(const std::string& constellation);
    ~mer_snr_estimator_impl() override;

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_MER_SNR_ESTIMATOR_IMPL_H */
