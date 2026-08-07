/* -*- c++ -*- */
/*
 * C++ port of MER_SNR_ISO.py: blind-phase MER/SNR estimator over a complex
 * symbol stream, comparing received symbols against ideal QPSK/16-QAM/
 * 64-QAM constellations.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_MER_SNR_ESTIMATOR_H
#define INCLUDED_ISDBT_MER_SNR_ESTIMATOR_H

#include <gnuradio/isdbt/api.h>
#include <gnuradio/sync_block.h>
#include <string>

namespace gr {
namespace isdbt {

/*!
 * \brief Estimates MER/SNR (dB) from a complex symbol stream via blind phase
 * de-rotation against ideal constellation points, and publishes the result
 * to the metrics blackboard.
 * \ingroup isdbt
 *
 * \param constellation "QPSK", "16QAM", "64QAM", or "AUTO" to classify at
 *        runtime from the relative MER of all three candidates (same
 *        thresholds as the Python reference).
 */
class ISDBT_API mer_snr_estimator : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<mer_snr_estimator> sptr;
    static sptr make(const std::string& constellation = "16QAM");
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_MER_SNR_ESTIMATOR_H */
