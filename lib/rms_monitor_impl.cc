/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rms_monitor_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/isdbt/metrics_board.h>

namespace gr {
namespace isdbt {

rms_monitor::sptr rms_monitor::make()
{
    return gnuradio::get_initial_sptr(new rms_monitor_impl());
}

rms_monitor_impl::rms_monitor_impl()
    : gr::sync_block("rms_monitor",
                      gr::io_signature::make(1, 1, sizeof(float)),
                      gr::io_signature::make(0, 0, 0))
{
}

rms_monitor_impl::~rms_monitor_impl() {}

int rms_monitor_impl::work(int noutput_items,
                            gr_vector_const_void_star& input_items,
                            gr_vector_void_star& /*output_items*/)
{
    const float* in = reinterpret_cast<const float*>(input_items[0]);
    if (noutput_items > 0) {
        datacast::metrics_board::instance().set_rms(in[noutput_items - 1]);
    }
    return noutput_items;
}

} // namespace isdbt
} // namespace gr
