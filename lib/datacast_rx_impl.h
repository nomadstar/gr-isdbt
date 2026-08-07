/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_DATACAST_RX_IMPL_H
#define INCLUDED_ISDBT_DATACAST_RX_IMPL_H

#include <gnuradio/isdbt/datacast_rx.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace gr {
namespace isdbt {

class datacast_rx_impl : public datacast_rx
{
private:
    using clock = std::chrono::steady_clock;

    struct rescue_entry {
        std::vector<uint8_t> data;
        uint64_t expected_hash;
        int attempts;
        clock::time_point ts;
    };

    struct frag_entry {
        std::map<uint16_t, std::vector<uint8_t>> parts;
        clock::time_point ts;
    };

    // ---- configuration ----
    int d_target_pid;
    int d_port_detection_window;
    uint32_t d_reorder_window;
    std::string d_output_interface;
    int d_tun_fd = -1;

    // ---- TS/section reassembly (target PID only) ----
    std::vector<uint8_t> d_ts_leftover;
    std::vector<uint8_t> d_section_buffer;
    std::size_t d_expected_section_len = 0;
    std::optional<uint8_t> d_last_cc;

    // ---- sequencing state ----
    uint32_t d_next_expected_seq = 1;
    std::set<uint32_t> d_delivered_seqs;
    std::deque<uint32_t> d_delivered_order;
    std::map<uint32_t, std::vector<uint8_t>> d_reorder_buffer;
    std::map<uint32_t, rescue_entry> d_rescue_buffer;

    bool d_gap_active = false;
    clock::time_point d_gap_since;
    bool d_gap_since_set = false;

    bool d_flow_lost = false;
    clock::time_point d_flow_lost_since;
    bool d_started_once = false;
    clock::time_point d_last_flow_reset_ts;
    bool d_eot_flag = false;

    // ---- IP fragment reassembly ----
    std::map<std::tuple<uint32_t, uint32_t, uint16_t>, frag_entry> d_ip_fragments;

    // ---- UDP port learning ----
    bool d_port_learning = true;
    std::map<uint16_t, int> d_port_votes;
    int d_target_udp_port = -1;

    // ---- output accumulation ----
    std::deque<uint8_t> d_output_queue;
    float d_last_output_ip_len = 0.0f;

    // ---- metrics ----
    uint64_t d_total_seen_packets = 0;
    uint64_t d_error_packets = 0;
    uint64_t d_drops_total = 0;
    uint64_t d_rescued_count = 0;

    void reset_state(uint32_t seq);
    void mark_flow_lost(uint32_t seq);

    std::vector<uint8_t> handle_ip_frag(const std::vector<uint8_t>& ip_pkt);
    void expire_ip_frags();
    std::vector<uint8_t> extract_and_filter_payload(const std::vector<uint8_t>& ip_packet);

    void emit(const std::vector<uint8_t>& pure_data);
    void handle_hash_result(uint32_t seq,
                             const std::vector<uint8_t>& ip_data,
                             uint64_t expected_hash,
                             bool hash_ok,
                             bool is_rescue);
    void process_sequenced_payload(const uint8_t* payload, std::size_t len);

    void flush_reorder_buffer();
    void force_advance_to_buffer_head();
    void flush_buffer_on_eot();

    void parse_ts_chunk(const uint8_t* data, std::size_t n);

public:
    datacast_rx_impl(int target_pid,
                      int port_detection_window,
                      int reorder_window,
                      const std::string& output_interface);
    ~datacast_rx_impl() override;

    bool start() override;
    bool stop() override;

    int general_work(int noutput_items,
                      gr_vector_int& ninput_items,
                      gr_vector_const_void_star& input_items,
                      gr_vector_void_star& output_items) override;
};

} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_DATACAST_RX_IMPL_H */
