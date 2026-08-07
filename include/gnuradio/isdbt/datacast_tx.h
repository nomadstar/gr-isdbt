/* -*- c++ -*- */
/*
 * C++ port of the MPE datacasting transmitter (tx.py, TransIsdtb repository)
 * described in Zamora Marambio's thesis. Captures IP/UDP traffic from a TUN
 * interface, encapsulates it into MPE sections over a private PID, and
 * schedules an adaptive repetition carousel for a simplex broadcast link.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_DATACAST_TX_H
#define INCLUDED_ISDBT_DATACAST_TX_H

#include <gnuradio/isdbt/api.h>
#include <gnuradio/sync_block.h>
#include <string>

namespace gr {
namespace isdbt {

/*!
 * \brief Datacasting transmitter: TUN capture -> MPE encapsulation -> TS
 * segmentation -> adaptive repetition carousel, over a private PID.
 * \ingroup isdbt
 */
class ISDBT_API datacast_tx : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<datacast_tx> sptr;

    /*!
     * \param interface TUN interface name to capture from (created if missing).
     * \param modulation Used only to pick a conservative TUN MTU ("QPSK",
     *        "16QAM", "64QAM").
     * \param fec Code rate string ("1/2", "2/3", "3/4", "5/6", "7/8"), used
     *        together with modulation for the MTU heuristic.
     * \param private_pid TS PID (13 bits) used for the datacasting payload.
     * \param dst_mac Filler bytes for the MPE header's MAC fields (no
     *        addressing meaning in this protocol).
     * \param heartbeat_interval Seconds between HEARTBEAT control packets.
     * \param queue_limit Max packets in the priority queue before
     *        backpressure blocks the capture thread.
     * \param carousel_repeats Baseline number of repetitions per data
     *        packet in the carousel (adapted at runtime by queue occupancy).
     */
    static sptr make(const std::string& interface = "tun0",
                      const std::string& modulation = "16QAM",
                      const std::string& fec = "2/3",
                      int private_pid = 0x0100,
                      const std::string& dst_mac = "FF:FF:FF:FF:FF:FF",
                      float heartbeat_interval = 0.1f,
                      int queue_limit = 5000,
                      int carousel_repeats = 3);
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_DATACAST_TX_H */
