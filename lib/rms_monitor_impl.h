/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_RMS_MONITOR_IMPL_H
#define INCLUDED_ISDBT_RMS_MONITOR_IMPL_H

#include <gnuradio/isdbt/rms_monitor.h>

namespace gr {
namespace isdbt {

class rms_monitor_impl : public rms_monitor
{
public:
    rms_monitor_impl();
    ~rms_monitor_impl() override;

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_RMS_MONITOR_IMPL_H */
