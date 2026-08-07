/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "datacast_rx_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/isdbt/datacast_format.h>
#include <gnuradio/isdbt/metrics_board.h>

#include <algorithm>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace gr {
namespace isdbt {

using namespace datacast;

namespace {
constexpr double GAP_TIMEOUT = 1.5;
constexpr double FLOW_LOST_RECOVERY_TIMEOUT = 10.0;
constexpr double START_RESET_DEBOUNCE = 1.0;
constexpr double IP_FRAG_TIMEOUT = 2.0;
constexpr std::size_t MAX_IP_FRAGS = 512;
constexpr std::size_t MAX_RESCUE_BUF = 5000;
constexpr std::size_t MAX_OUTPUT_QUEUE = 8 * 1024 * 1024;
} // namespace

datacast_rx::sptr datacast_rx::make(int target_pid,
                                     int port_detection_window,
                                     int reorder_window,
                                     const std::string& output_interface)
{
    return gnuradio::get_initial_sptr(
        new datacast_rx_impl(target_pid, port_detection_window, reorder_window, output_interface));
}

datacast_rx_impl::datacast_rx_impl(int target_pid,
                                    int port_detection_window,
                                    int reorder_window,
                                    const std::string& output_interface)
    : gr::block("datacast_rx",
                gr::io_signature::make(1, 1, sizeof(uint8_t)),
                gr::io_signature::make(
                    5,
                    5,
                    std::vector<size_t>{ sizeof(uint8_t), sizeof(float), sizeof(float),
                                          sizeof(float), sizeof(float) })),
      d_target_pid(target_pid & 0x1FFF),
      d_port_detection_window(port_detection_window),
      d_reorder_window(static_cast<uint32_t>(reorder_window)),
      d_output_interface(output_interface)
{
    d_last_flow_reset_ts = clock::now();
}

datacast_rx_impl::~datacast_rx_impl() { stop(); }

bool datacast_rx_impl::start()
{
    if (!d_output_interface.empty()) {
        int tun = ::open("/dev/net/tun", O_RDWR);
        if (tun >= 0) {
            struct ifreq ifr;
            std::memset(&ifr, 0, sizeof(ifr));
            ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
            std::strncpy(ifr.ifr_name, d_output_interface.c_str(), IFNAMSIZ - 1);
            if (::ioctl(tun, TUNSETIFF, reinterpret_cast<void*>(&ifr)) == 0) {
                d_tun_fd = tun;
                std::cerr << "[RX] TUN interface ready: " << d_output_interface << "\n";
            } else {
                std::cerr << "[RX] TUNSETIFF failed for " << d_output_interface << "\n";
                ::close(tun);
            }
        }
    }
    return gr::block::start();
}

bool datacast_rx_impl::stop()
{
    if (d_tun_fd >= 0) {
        ::close(d_tun_fd);
        d_tun_fd = -1;
    }
    return gr::block::stop();
}

void datacast_rx_impl::reset_state(uint32_t seq)
{
    d_reorder_buffer.clear();
    d_rescue_buffer.clear();
    d_delivered_seqs.clear();
    d_delivered_order.clear();
    d_ip_fragments.clear();
    d_output_queue.clear();

    d_next_expected_seq = seq;
    d_gap_active = false;
    d_gap_since_set = false;
    d_flow_lost = false;
    d_started_once = true;
    d_last_flow_reset_ts = clock::now();
    d_eot_flag = false;

    d_section_buffer.clear();
    d_expected_section_len = 0;

    d_port_learning = true;
    d_port_votes.clear();
    d_target_udp_port = -1;
}

void datacast_rx_impl::mark_flow_lost(uint32_t seq)
{
    if (!d_flow_lost) {
        std::cerr << "[RX] FLOW LOST: seq=" << seq << " outside reorder window.\n";
        d_flow_lost_since = clock::now();
        d_flow_lost = true;
    }
}

std::vector<uint8_t> datacast_rx_impl::handle_ip_frag(const std::vector<uint8_t>& ip_pkt)
{
    if (ip_pkt.size() < 20) return {};
    std::size_t ihl = (ip_pkt[0] & 0x0F) * 4;
    if (ip_pkt.size() < ihl) return {};

    uint16_t total_len = (uint16_t(ip_pkt[2]) << 8) | ip_pkt[3];
    uint16_t ident = (uint16_t(ip_pkt[4]) << 8) | ip_pkt[5];
    uint16_t flags_frag = (uint16_t(ip_pkt[6]) << 8) | ip_pkt[7];
    bool mf = (flags_frag & 0x2000) != 0;
    uint16_t offset = (flags_frag & 0x1FFF) * 8;

    if (!mf && offset == 0) {
        if (ip_pkt.size() < total_len) return {};
        return std::vector<uint8_t>(ip_pkt.begin(), ip_pkt.begin() + total_len);
    }
    if (ip_pkt.size() < total_len) return {};

    uint32_t src, dst;
    std::memcpy(&src, &ip_pkt[12], 4);
    std::memcpy(&dst, &ip_pkt[16], 4);
    auto key = std::make_tuple(src, dst, ident);

    if (d_ip_fragments.find(key) == d_ip_fragments.end()) {
        if (d_ip_fragments.size() >= MAX_IP_FRAGS) {
            auto oldest = std::min_element(
                d_ip_fragments.begin(), d_ip_fragments.end(),
                [](const auto& a, const auto& b) { return a.second.ts < b.second.ts; });
            if (oldest != d_ip_fragments.end()) d_ip_fragments.erase(oldest);
        }
        d_ip_fragments[key] = frag_entry{ {}, clock::now() };
    }

    d_ip_fragments[key].parts[offset] =
        std::vector<uint8_t>(ip_pkt.begin() + ihl, ip_pkt.begin() + total_len);

    if (!mf) {
        auto& parts = d_ip_fragments[key].parts;
        std::vector<uint8_t> full;
        uint32_t expected_off = 0;
        for (const auto& kv : parts) {
            if (kv.first != expected_off) return {}; // gap: wait for more fragments
            full.insert(full.end(), kv.second.begin(), kv.second.end());
            expected_off += static_cast<uint32_t>(kv.second.size());
        }
        std::vector<uint8_t> res(ip_pkt.begin(), ip_pkt.begin() + ihl);
        uint16_t new_total = static_cast<uint16_t>(ihl + full.size());
        res[2] = (new_total >> 8) & 0xFF;
        res[3] = new_total & 0xFF;
        res.insert(res.end(), full.begin(), full.end());
        d_ip_fragments.erase(key);
        return res;
    }
    return {};
}

void datacast_rx_impl::expire_ip_frags()
{
    auto now = clock::now();
    for (auto it = d_ip_fragments.begin(); it != d_ip_fragments.end();) {
        if (std::chrono::duration<double>(now - it->second.ts).count() > IP_FRAG_TIMEOUT) {
            it = d_ip_fragments.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<uint8_t>
datacast_rx_impl::extract_and_filter_payload(const std::vector<uint8_t>& ip_packet)
{
    if (ip_packet.size() < 20) return {};
    std::size_t ihl = (ip_packet[0] & 0x0F) * 4;
    uint8_t protocol = ip_packet[9];
    if (protocol != 17) return {}; // only UDP is transported by this protocol
    if (ip_packet.size() < ihl + 8) return {};

    uint16_t dst_port = (uint16_t(ip_packet[ihl + 2]) << 8) | ip_packet[ihl + 3];
    uint16_t udp_len = (uint16_t(ip_packet[ihl + 4]) << 8) | ip_packet[ihl + 5];
    if (ip_packet.size() < ihl + udp_len || udp_len < 8) return {};

    if (d_port_learning) {
        d_port_votes[dst_port]++;
        int total_votes = 0;
        for (const auto& kv : d_port_votes) total_votes += kv.second;
        if (total_votes >= d_port_detection_window) {
            auto best = std::max_element(
                d_port_votes.begin(), d_port_votes.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            d_target_udp_port = best->first;
            d_port_learning = false;
            d_port_votes.clear();
            std::cerr << "[RX] UDP port auto-detected: " << d_target_udp_port << "\n";
        }
        return std::vector<uint8_t>(ip_packet.begin() + ihl + 8, ip_packet.begin() + ihl + udp_len);
    }

    if (dst_port != d_target_udp_port) return {};
    return std::vector<uint8_t>(ip_packet.begin() + ihl + 8, ip_packet.begin() + ihl + udp_len);
}

void datacast_rx_impl::emit(const std::vector<uint8_t>& pure_data)
{
    if (pure_data.empty()) return;
    if (d_tun_fd >= 0) {
        ssize_t written = ::write(d_tun_fd, pure_data.data(), pure_data.size());
        (void)written;
    }
    d_output_queue.insert(d_output_queue.end(), pure_data.begin(), pure_data.end());
    d_last_output_ip_len = static_cast<float>(pure_data.size());
    while (d_output_queue.size() > MAX_OUTPUT_QUEUE) {
        d_output_queue.pop_front();
        d_drops_total++;
    }
}

void datacast_rx_impl::handle_hash_result(uint32_t seq,
                                           const std::vector<uint8_t>& ip_data,
                                           uint64_t expected_hash,
                                           bool hash_ok,
                                           bool is_rescue)
{
    if (hash_ok) {
        if (is_rescue) {
            d_rescued_count++;
            d_rescue_buffer.erase(seq);
        }
        if (seq >= d_next_expected_seq && !d_delivered_seqs.count(seq)) {
            d_reorder_buffer[seq] = ip_data;
        }
        return;
    }

    if (is_rescue) {
        auto it = d_rescue_buffer.find(seq);
        if (it != d_rescue_buffer.end()) it->second.attempts++;
        return;
    }

    if (!d_delivered_seqs.count(seq) && !d_reorder_buffer.count(seq)) {
        d_error_packets++;
        d_rescue_buffer[seq] = rescue_entry{ ip_data, expected_hash, 1, clock::now() };
        if (d_rescue_buffer.size() > MAX_RESCUE_BUF) {
            auto oldest = std::min_element(
                d_rescue_buffer.begin(), d_rescue_buffer.end(),
                [](const auto& a, const auto& b) { return a.second.ts < b.second.ts; });
            if (oldest != d_rescue_buffer.end()) {
                d_rescue_buffer.erase(oldest);
                d_drops_total++;
            }
        }
    }
}

void datacast_rx_impl::process_sequenced_payload(const uint8_t* payload, std::size_t len)
{
    if (len < APP_HEADER_SIZE) return;
    uint32_t seq = get_u32_be(payload);

    if (seq == SEQ_HEARTBEAT) return;
    if (seq == SEQ_EOT) {
        if (!d_eot_flag) flush_buffer_on_eot();
        return;
    }
    if (seq == SEQ_START) {
        bool debounce_ok =
            d_flow_lost &&
            std::chrono::duration<double>(clock::now() - d_last_flow_reset_ts).count() >=
                START_RESET_DEBOUNCE;
        if (!d_started_once || debounce_ok) reset_state(1);
        return;
    }

    uint64_t received_hash = 0;
    for (int i = 0; i < 8; i++) received_hash = (received_hash << 8) | payload[4 + i];
    std::vector<uint8_t> ip_data(payload + APP_HEADER_SIZE, payload + len);

    if (d_flow_lost) {
        double elapsed =
            std::chrono::duration<double>(clock::now() - d_flow_lost_since).count();
        if (elapsed >= FLOW_LOST_RECOVERY_TIMEOUT) {
            std::cerr << "[RX] FLOW LOST auto-recovery after " << elapsed << "s.\n";
            reset_state(seq);
        } else {
            return;
        }
    }

    bool is_known = d_reorder_buffer.count(seq) || d_rescue_buffer.count(seq) ||
                    d_delivered_seqs.count(seq);

    if (!is_known && seq > d_next_expected_seq + d_reorder_window) {
        mark_flow_lost(seq);
        return;
    }
    if (seq < d_next_expected_seq && !d_delivered_seqs.count(seq)) return; // arrived too late
    if (d_delivered_seqs.count(seq)) return;
    if (d_reorder_buffer.count(seq)) return;

    bool is_rescue = d_rescue_buffer.count(seq) > 0;
    d_total_seen_packets++;

    uint64_t computed = xxh64(ip_data.data(), ip_data.size());
    handle_hash_result(seq, ip_data, received_hash, computed == received_hash, is_rescue);
}

void datacast_rx_impl::force_advance_to_buffer_head()
{
    if (d_reorder_buffer.empty()) return;
    uint32_t head_seq = d_reorder_buffer.begin()->first;
    if (head_seq <= d_next_expected_seq) return;

    for (uint32_t cur = d_next_expected_seq; cur < head_seq; cur++) {
        if (d_rescue_buffer.count(cur)) return; // still recoverable: wait longer
    }

    uint32_t skipped = head_seq - d_next_expected_seq;
    d_next_expected_seq = head_seq;
    d_drops_total += skipped;
    d_gap_active = false;
    d_gap_since_set = false;
}

void datacast_rx_impl::flush_reorder_buffer()
{
    d_gap_active = !d_reorder_buffer.empty() &&
                   d_reorder_buffer.find(d_next_expected_seq) == d_reorder_buffer.end();
    if (d_gap_active) {
        if (!d_gap_since_set) {
            d_gap_since = clock::now();
            d_gap_since_set = true;
        } else if (std::chrono::duration<double>(clock::now() - d_gap_since).count() >=
                   GAP_TIMEOUT) {
            force_advance_to_buffer_head();
        }
    } else {
        d_gap_since_set = false;
    }

    auto it = d_reorder_buffer.find(d_next_expected_seq);
    while (it != d_reorder_buffer.end()) {
        std::vector<uint8_t> ip_data = std::move(it->second);
        d_reorder_buffer.erase(it);

        std::vector<uint8_t> final_ip = handle_ip_frag(ip_data);
        if (!final_ip.empty()) {
            std::vector<uint8_t> pure = extract_and_filter_payload(final_ip);
            if (!pure.empty()) emit(pure);
        }

        d_delivered_seqs.insert(d_next_expected_seq);
        d_delivered_order.push_back(d_next_expected_seq);
        d_rescue_buffer.erase(d_next_expected_seq);
        d_next_expected_seq++;

        while (d_delivered_order.size() > d_reorder_window) {
            d_delivered_seqs.erase(d_delivered_order.front());
            d_delivered_order.pop_front();
        }

        it = d_reorder_buffer.find(d_next_expected_seq);
    }

    while (d_reorder_buffer.size() > d_reorder_window) {
        auto worst = std::prev(d_reorder_buffer.end());
        d_reorder_buffer.erase(worst);
        d_drops_total++;
    }
}

void datacast_rx_impl::flush_buffer_on_eot()
{
    d_eot_flag = true;
    for (auto& kv : d_reorder_buffer) {
        std::vector<uint8_t> final_ip = handle_ip_frag(kv.second);
        if (!final_ip.empty()) {
            std::vector<uint8_t> pure = extract_and_filter_payload(final_ip);
            if (!pure.empty()) emit(pure);
        }
    }
    std::cerr << "[RX] EOT received. Flushed " << d_reorder_buffer.size()
              << " pending entries. Rescued=" << d_rescued_count << "\n";
    d_reorder_buffer.clear();
}

int datacast_rx_impl::general_work(int noutput_items,
                                    gr_vector_int& ninput_items,
                                    gr_vector_const_void_star& input_items,
                                    gr_vector_void_star& output_items)
{
    expire_ip_frags();

    const uint8_t* in = reinterpret_cast<const uint8_t*>(input_items[0]);
    int n_in_items = ninput_items[0];
    d_ts_leftover.insert(d_ts_leftover.end(), in, in + n_in_items);

    std::size_t n_in = d_ts_leftover.size();
    std::size_t consumed = 0;

    while (consumed + TS_PACKET_SIZE <= n_in) {
        if (d_ts_leftover[consumed] != TS_SYNC_BYTE) {
            consumed += 1;
            continue;
        }

        bool pusi = (d_ts_leftover[consumed + 1] & 0x40) != 0;
        int pid = ((d_ts_leftover[consumed + 1] & 0x1F) << 8) | d_ts_leftover[consumed + 2];

        if (pid == d_target_pid) {
            int afc = (d_ts_leftover[consumed + 3] >> 4) & 0x03;
            bool has_payload = (afc & 0x01) != 0;

            if (has_payload) {
                std::size_t payload_start = consumed + 4;
                if (afc & 0x2) {
                    uint8_t afl = d_ts_leftover[payload_start];
                    payload_start += 1 + afl;
                }

                uint8_t cc = d_ts_leftover[consumed + 3] & 0x0F;
                bool is_duplicate = false;
                if (d_last_cc.has_value()) {
                    if (cc == *d_last_cc) {
                        is_duplicate = true;
                    } else if (cc != ((*d_last_cc + 1) & 0x0F)) {
                        d_section_buffer.clear();
                        d_expected_section_len = 0;
                    }
                }
                if (!is_duplicate) d_last_cc = cc;

                if (!is_duplicate && payload_start < consumed + TS_PACKET_SIZE) {
                    if (pusi) {
                        uint8_t ptr = d_ts_leftover[payload_start];
                        std::size_t cand_start = payload_start + 1 + ptr;
                        d_section_buffer.clear();
                        d_expected_section_len = 0;
                        if (cand_start < consumed + TS_PACKET_SIZE) {
                            std::size_t cand_len = (consumed + TS_PACKET_SIZE) - cand_start;
                            if (cand_len > 3 && d_ts_leftover[cand_start] == 0x3E) {
                                d_expected_section_len =
                                    (((d_ts_leftover[cand_start + 1] & 0x0F) << 8) |
                                     d_ts_leftover[cand_start + 2]) +
                                    3;
                                d_section_buffer.assign(d_ts_leftover.begin() + cand_start,
                                                         d_ts_leftover.begin() + cand_start +
                                                             cand_len);
                            }
                        }
                    } else if (d_expected_section_len > 0) {
                        d_section_buffer.insert(d_section_buffer.end(),
                                                 d_ts_leftover.begin() + payload_start,
                                                 d_ts_leftover.begin() + consumed +
                                                     TS_PACKET_SIZE);
                    }

                    if (d_expected_section_len > 0 &&
                        d_section_buffer.size() >= d_expected_section_len) {
                        if (d_expected_section_len >= MPE_HEADER_SIZE + MPE_CRC_SIZE) {
                            const uint8_t* sect = d_section_buffer.data();
                            std::size_t sect_len = d_expected_section_len;
                            uint32_t expected_crc = get_u32_be(sect + sect_len - 4);
                            uint32_t crc = crc32_mpeg2(sect, sect_len - 4);
                            if (crc == expected_crc) {
                                process_sequenced_payload(sect + MPE_HEADER_SIZE,
                                                           sect_len - MPE_HEADER_SIZE -
                                                               MPE_CRC_SIZE);
                            } else {
                                d_error_packets++;
                                d_total_seen_packets++;
                            }
                        }
                        d_expected_section_len = 0;
                        d_section_buffer.clear();
                    }
                }
            }
        }

        consumed += TS_PACKET_SIZE;
    }

    if (consumed > 0) {
        d_ts_leftover.erase(d_ts_leftover.begin(), d_ts_leftover.begin() + consumed);
    }

    if (d_flow_lost) {
        double elapsed = std::chrono::duration<double>(clock::now() - d_flow_lost_since).count();
        if (elapsed >= FLOW_LOST_RECOVERY_TIMEOUT) reset_state(1);
    }

    flush_reorder_buffer();

    uint8_t* out0 = reinterpret_cast<uint8_t*>(output_items[0]);
    std::size_t n_out = std::min(static_cast<std::size_t>(std::max(noutput_items, 0)),
                                  d_output_queue.size());
    for (std::size_t i = 0; i < n_out; i++) {
        out0[i] = d_output_queue.front();
        d_output_queue.pop_front();
    }

    if (noutput_items > 0) {
        float per = d_total_seen_packets > 0
                        ? static_cast<float>(d_error_packets) /
                              static_cast<float>(d_total_seen_packets)
                        : 0.0f;
        datacast::metrics_board::instance().set_per(per);
        float status[4] = { per, static_cast<float>(d_reorder_buffer.size()),
                             static_cast<float>(d_rescue_buffer.size()), d_last_output_ip_len };
        for (int p = 1; p <= 4; p++) {
            *reinterpret_cast<float*>(output_items[p]) = status[p - 1];
            produce(p, 1);
        }
    }

    consume(0, n_in_items);
    produce(0, static_cast<int>(n_out));
    return gr::block::WORK_CALLED_PRODUCE;
}

} // namespace isdbt
} // namespace gr
