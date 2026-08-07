/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_METRICS_LOGGER_IMPL_H
#define INCLUDED_ISDBT_METRICS_LOGGER_IMPL_H

#include <gnuradio/isdbt/metrics_logger.h>

#include <atomic>
#include <fstream>
#include <string>
#include <thread>

namespace gr {
namespace isdbt {

class metrics_logger_impl : public metrics_logger
{
private:
    float d_print_interval;
    std::ofstream d_csv_file;
    std::atomic<bool> d_stop{ true };
    std::thread d_thread;

    void writer_loop();
    void emit_metrics();

public:
    metrics_logger_impl(const std::string& filename, float print_interval);
    ~metrics_logger_impl() override;

    bool start() override;
    bool stop() override;

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_METRICS_LOGGER_IMPL_H */
