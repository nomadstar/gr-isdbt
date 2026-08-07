/* -*- c++ -*- */
/*
 * C++ port of the MPE datacasting receiver (rx.py, TransIsdtb repository)
 * described in Zamora Marambio's thesis. Reassembles MPE sections from a TS
 * byte stream, validates CRC-32 and per-packet xxHash64, reorders by
 * sequence number, and rescues packets that fail hash validation once the
 * transmitter's carousel resends a valid copy.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_DATACAST_RX_H
#define INCLUDED_ISDBT_DATACAST_RX_H

#include <gnuradio/block.h>
#include <gnuradio/isdbt/api.h>
#include <string>

namespace gr {
namespace isdbt {

/*!
 * \brief Datacasting receiver: TS parsing -> MPE reassembly -> CRC-32 ->
 * xxHash64 -> reorder/rescue -> IP defrag -> UDP payload delivery.
 * \ingroup isdbt
 *
 * Output 0 is the recovered UDP payload byte stream. Outputs 1-4 are
 * float32 status streams: cumulative PER, reorder buffer occupancy, rescue
 * buffer occupancy, and the length of the last delivered IP packet.
 */
class ISDBT_API datacast_rx : virtual public gr::block
{
public:
    typedef std::shared_ptr<datacast_rx> sptr;

    /*!
     * \param target_pid TS PID (13 bits) carrying the datacasting payload.
     * \param port_detection_window Number of UDP packets observed before
     *        locking onto the dominant destination port ("port learning").
     * \param reorder_window Max sequence-number span accepted before the
     *        flow is considered lost and delivery pauses until a new START.
     * \param output_interface Optional TUN interface name to additionally
     *        mirror the recovered payload to (empty = GR output port only).
     */
    static sptr make(int target_pid = 0x0100,
                      int port_detection_window = 20,
                      int reorder_window = 50000,
                      const std::string& output_interface = "");
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_DATACAST_RX_H */
