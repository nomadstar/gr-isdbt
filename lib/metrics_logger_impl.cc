/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "metrics_logger_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/isdbt/metrics_board.h>

#include <chrono>
#include <cmath>
#include <ctime>
#include <iostream>
#include <thread>

namespace gr {
namespace isdbt {

metrics_logger::sptr metrics_logger::make(const std::string& filename, float print_interval)
{
    return gnuradio::get_initial_sptr(new metrics_logger_impl(filename, print_interval));
}

metrics_logger_impl::metrics_logger_impl(const std::string& filename, float print_interval)
    : gr::sync_block("metrics_logger",
                      gr::io_signature::make(1, 1, sizeof(float)),
                      gr::io_signature::make(0, 0, 0)),
      d_print_interval(print_interval)
{
    d_csv_file.open(filename, std::ios::out | std::ios::trunc);
    d_csv_file << "Timestamp,RMS_dB,RF_SNR_dB,MER_dB,PER\n";
    d_csv_file.flush();
    std::cerr << "[LOGGER] Archivo " << filename << " creado.\n";
}

metrics_logger_impl::~metrics_logger_impl()
{
    stop();
    if (d_csv_file.is_open()) d_csv_file.close();
}

void metrics_logger_impl::emit_metrics()
{
    auto& g = datacast::metrics_board::instance();

    float rms = std::max(g.rms(), 1e-12f);
    float rf_snr = g.rf_snr_db();
    float mer = g.mer_db();
    float per = g.per();

    float rms_db = 20.0f * std::log10(rms);

    std::time_t t = std::time(nullptr);
    std::tm tm_buf;
    localtime_r(&t, &tm_buf);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    auto fmt_or_nan = [](float v, int prec) {
        char buf[32];
        if (!std::isfinite(v)) return std::string("NaN");
        std::snprintf(buf, sizeof(buf), "%.*f", prec, v);
        return std::string(buf);
    };

    std::string per_str = fmt_or_nan(per, 6);
    std::string mer_str = fmt_or_nan(mer, 2);

    std::cerr << "[METRICAS] RMS=" << rms_db << " dB | RF_SNR=" << rf_snr
              << " dB | MER=" << mer_str << " dB | PER=" << per_str << "\n";

    if (d_csv_file.is_open()) {
        d_csv_file << ts << "," << rms_db << "," << rf_snr << "," << mer_str << ","
                    << per_str << "\n";
        d_csv_file.flush();
    }
}

void metrics_logger_impl::writer_loop()
{
    while (!d_stop.load()) {
        for (float slept = 0.0f; slept < d_print_interval && !d_stop.load();
             slept += 0.1f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (d_stop.load()) break;
        emit_metrics();
    }
}

bool metrics_logger_impl::start()
{
    d_stop = false;
    d_thread = std::thread(&metrics_logger_impl::writer_loop, this);
    return gr::sync_block::start();
}

bool metrics_logger_impl::stop()
{
    d_stop = true;
    if (d_thread.joinable()) d_thread.join();
    return gr::sync_block::stop();
}

int metrics_logger_impl::work(int noutput_items,
                               gr_vector_const_void_star& /*input_items*/,
                               gr_vector_void_star& /*output_items*/)
{
    // The background thread does all the emitting on a fixed wall-clock
    // interval, independent of how often the scheduler calls work(); this
    // input stream only exists to keep the block active in the flowgraph.
    return noutput_items;
}

} // namespace isdbt
} // namespace gr
