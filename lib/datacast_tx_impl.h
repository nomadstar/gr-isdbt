/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_DATACAST_TX_IMPL_H
#define INCLUDED_ISDBT_DATACAST_TX_IMPL_H

#include <gnuradio/isdbt/datacast_tx.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gr {
namespace isdbt {

class datacast_tx_impl : public datacast_tx
{
private:
    // One repetition group in the carousel: `orig` is the immutable set of
    // TS packets for the section, `rem` is what is left to emit in the
    // current pass, `repeats_left` counts passes still owed after this one.
    struct carousel_group {
        std::vector<std::vector<uint8_t>> orig;
        std::vector<std::vector<uint8_t>> rem;
        int repeats_left;
    };

    std::string d_interface;
    int d_pid;
    std::array<uint8_t, 6> d_dst_mac;
    float d_heartbeat_interval;
    int d_queue_limit;
    int d_carousel_repeats;
    int d_mtu;

    std::mutex d_mutex;
    std::condition_variable d_cond;
    std::atomic<bool> d_stop{ true };
    std::thread d_thread;

    std::deque<std::vector<uint8_t>> d_priority_queue;
    std::deque<carousel_group> d_carousel_pending;

    uint8_t d_cc = 0;
    uint8_t d_null_cc = 0;
    uint32_t d_tx_packet_id = 1;
    std::vector<uint8_t> d_null_pkt;
    std::vector<uint8_t> d_residual;
    int d_tun_fd = -1;

    static int mtu_for(const std::string& modulation, const std::string& fec);
    static bool check_interface_exists(const std::string& interface);

    std::vector<uint8_t> build_mpe_section(const uint8_t* payload, std::size_t len) const;
    std::vector<std::vector<uint8_t>> section_to_ts(const std::vector<uint8_t>& section) const;
    std::vector<std::vector<uint8_t>> build_control_ts(uint32_t seq, const std::string& data) const;

    int get_adaptive_repeats();
    void enqueue_new_packet(const std::vector<std::vector<uint8_t>>& ts_pkts);
    void enqueue_control_burst(const std::vector<std::vector<uint8_t>>& ts_pkts);

    void sniffer_loop();

public:
    datacast_tx_impl(const std::string& interface,
                      const std::string& modulation,
                      const std::string& fec,
                      int private_pid,
                      const std::string& dst_mac,
                      float heartbeat_interval,
                      int queue_limit,
                      int carousel_repeats);
    ~datacast_tx_impl() override;

    bool start() override;
    bool stop() override;

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_DATACAST_TX_IMPL_H */
