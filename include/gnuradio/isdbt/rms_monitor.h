/* -*- c++ -*- */
/*
 * C++ port of RMS_ISO.py: captures the latest RMS scalar produced by a
 * GNU Radio RMS block and publishes it to the metrics blackboard.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_RMS_MONITOR_H
#define INCLUDED_ISDBT_RMS_MONITOR_H

#include <gnuradio/isdbt/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace isdbt {

/*!
 * \brief Publishes the last value of an incoming RMS float stream to the
 * process-wide metrics blackboard. \ingroup isdbt
 */
class ISDBT_API rms_monitor : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<rms_monitor> sptr;
    static sptr make();
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_RMS_MONITOR_H */
