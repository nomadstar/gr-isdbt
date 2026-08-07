/* -*- c++ -*- */
/*
 * Process-wide metrics blackboard, C++ equivalent of the Python prototype's
 * `sys.mis_metricas_globales` shared dict: a decoupled channel between the
 * datacasting receiver (PER), the RF/constellation metric blocks (RMS,
 * MER/SNR), and the CSV logger, without wiring extra GNU Radio stream
 * connections between them.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_METRICS_BOARD_H
#define INCLUDED_ISDBT_METRICS_BOARD_H

#include <chrono>
#include <cmath>
#include <mutex>

namespace gr {
namespace isdbt {
namespace datacast {

class metrics_board
{
public:
    static metrics_board& instance()
    {
        static metrics_board inst;
        return inst;
    }

    void set_rms(float rms)
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        d_rms = rms;
    }
    float rms() const
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        return d_rms;
    }

    void set_mer_db(float mer_db)
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        d_mer_db = mer_db;
        d_mer_ts = now_s();
    }
    float mer_db() const
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        return d_mer_db;
    }

    void set_rf_snr_db(float snr_db)
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        d_rf_snr_db = snr_db;
    }
    float rf_snr_db() const
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        return d_rf_snr_db;
    }

    void set_per(float per)
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        d_per = per;
        d_per_ts = now_s();
    }
    // Returns NaN if no PER sample was reported within `stale_after_s`
    // seconds, mirroring rx.py's staleness handling in loggermaster.py.
    float per(double stale_after_s = 1.5) const
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        if (d_per_ts > 0.0 && (now_s() - d_per_ts) > stale_after_s) {
            return std::nanf("");
        }
        return d_per;
    }

private:
    metrics_board() = default;

    static double now_s()
    {
        using namespace std::chrono;
        return duration<double>(system_clock::now().time_since_epoch()).count();
    }

    mutable std::mutex d_mutex;
    float d_rms = 0.0f;
    float d_mer_db = std::nanf("");
    double d_mer_ts = 0.0;
    float d_rf_snr_db = std::nanf("");
    float d_per = std::nanf("");
    double d_per_ts = 0.0;
};

} // namespace datacast
} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_METRICS_BOARD_H */
