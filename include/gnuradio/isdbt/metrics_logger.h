/* -*- c++ -*- */
/*
 * C++ port of loggermaster.py: periodically reads the metrics blackboard
 * (RMS, MER/SNR, PER) and writes it to console and a timestamped CSV file.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_METRICS_LOGGER_H
#define INCLUDED_ISDBT_METRICS_LOGGER_H

#include <gnuradio/isdbt/api.h>
#include <gnuradio/sync_block.h>
#include <string>

namespace gr {
namespace isdbt {

/*!
 * \brief Consolidates RMS/MER/SNR/PER from the metrics blackboard into
 * console output and a CSV file, on a fixed interval, independent of the
 * scheduler's call rate.
 * \ingroup isdbt
 */
class ISDBT_API metrics_logger : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<metrics_logger> sptr;

    static sptr make(const std::string& filename = "resultados_isdbt.csv",
                      float print_interval = 1.0f);
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_METRICS_LOGGER_H */
