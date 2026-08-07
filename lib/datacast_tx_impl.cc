/* -*- c++ -*- */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "datacast_tx_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/isdbt/datacast_format.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

namespace gr {
namespace isdbt {

using namespace datacast;

datacast_tx::sptr datacast_tx::make(const std::string& interface,
                                     const std::string& modulation,
                                     const std::string& fec,
                                     int private_pid,
                                     const std::string& dst_mac,
                                     float heartbeat_interval,
                                     int queue_limit,
                                     int carousel_repeats)
{
    return gnuradio::get_initial_sptr(new datacast_tx_impl(
        interface, modulation, fec, private_pid, dst_mac, heartbeat_interval,
        queue_limit, carousel_repeats));
}

int datacast_tx_impl::mtu_for(const std::string& modulation, const std::string& fec)
{
    std::string mod = modulation;
    std::transform(mod.begin(), mod.end(), mod.begin(), ::toupper);
    bool low_rate = (fec == "1/2" || fec == "2/3");

    if (mod == "QPSK") return 1316;
    if (mod == "16QAM") return low_rate ? 1024 : 768;
    if (mod == "64QAM") return low_rate ? 768 : 576;
    return 768;
}

bool datacast_tx_impl::check_interface_exists(const std::string& interface)
{
    std::string cmd = "ip link show dev " + interface + " > /dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

datacast_tx_impl::datacast_tx_impl(const std::string& interface,
                                    const std::string& modulation,
                                    const std::string& fec,
                                    int private_pid,
                                    const std::string& dst_mac,
                                    float heartbeat_interval,
                                    int queue_limit,
                                    int carousel_repeats)
    : gr::sync_block("datacast_tx",
                      gr::io_signature::make(0, 0, 0),
                      gr::io_signature::make2(2, 2, sizeof(uint8_t), sizeof(float))),
      d_interface(interface),
      d_pid(private_pid & 0x1FFF),
      d_heartbeat_interval(heartbeat_interval),
      d_queue_limit(queue_limit),
      d_carousel_repeats(carousel_repeats),
      d_mtu(mtu_for(modulation, fec))
{
    unsigned int mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    std::sscanf(dst_mac.c_str(), "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2],
                &mac[3], &mac[4], &mac[5]);
    for (int i = 0; i < 6; i++) d_dst_mac[i] = static_cast<uint8_t>(mac[i]);

    d_null_pkt.assign(TS_PACKET_SIZE, 0xFF);
    d_null_pkt[0] = TS_SYNC_BYTE;
    d_null_pkt[1] = 0x1F;
    d_null_pkt[2] = 0xFF;
    d_null_pkt[3] = 0x10;

    std::fprintf(stderr,
                 "[TX] datacast_tx | PID: 0x%03X | MTU: %d (%s %s) | Carousel repeats: %d\n",
                 d_pid, d_mtu, modulation.c_str(), fec.c_str(), d_carousel_repeats);
}

datacast_tx_impl::~datacast_tx_impl() { stop(); }

std::vector<uint8_t> datacast_tx_impl::build_mpe_section(const uint8_t* payload,
                                                           std::size_t len) const
{
    std::size_t section_length = 9 + len + 4;
    std::vector<uint8_t> hdr(MPE_HEADER_SIZE);
    hdr[0] = 0x3E;
    hdr[1] = 0xB0 | ((section_length >> 8) & 0x0F);
    hdr[2] = section_length & 0xFF;
    hdr[3] = d_dst_mac[5];
    hdr[4] = d_dst_mac[4];
    hdr[5] = 0xC1;
    hdr[6] = 0x00;
    hdr[7] = 0x00;
    hdr[8] = d_dst_mac[3];
    hdr[9] = d_dst_mac[2];
    hdr[10] = d_dst_mac[1];
    hdr[11] = d_dst_mac[0];

    hdr.insert(hdr.end(), payload, payload + len);

    uint32_t crc = crc32_mpeg2(hdr.data(), hdr.size());
    uint8_t crc_be[4];
    put_u32_be(crc_be, crc);
    hdr.insert(hdr.end(), crc_be, crc_be + 4);
    return hdr;
}

std::vector<std::vector<uint8_t>>
datacast_tx_impl::section_to_ts(const std::vector<uint8_t>& section) const
{
    std::vector<std::vector<uint8_t>> packets;
    std::size_t idx = 0;
    bool first = true;

    while (idx < section.size()) {
        std::vector<uint8_t> pkt(TS_PACKET_SIZE, 0xFF);
        pkt[0] = TS_SYNC_BYTE;

        if (first) {
            std::size_t chunk_len = std::min(TS_FIRST_PACKET_PAYLOAD, section.size() - idx);
            pkt[1] = 0x40 | ((d_pid >> 8) & 0x1F);
            pkt[2] = d_pid & 0xFF;
            pkt[3] = 0x10; // CC placeholder, patched in work()
            pkt[4] = 0x00; // pointer_field
            std::memcpy(&pkt[5], &section[idx], chunk_len);
            idx += chunk_len;
            first = false;
        } else {
            std::size_t chunk_len = std::min(TS_CONT_PACKET_PAYLOAD, section.size() - idx);
            pkt[1] = (d_pid >> 8) & 0x1F;
            pkt[2] = d_pid & 0xFF;
            pkt[3] = 0x10;
            std::memcpy(&pkt[4], &section[idx], chunk_len);
            idx += chunk_len;
        }
        packets.push_back(std::move(pkt));
    }
    return packets;
}

std::vector<std::vector<uint8_t>> datacast_tx_impl::build_control_ts(uint32_t seq,
                                                                       const std::string& data) const
{
    uint64_t h = xxh64(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    std::vector<uint8_t> payload(APP_HEADER_SIZE + data.size());
    put_u32_be(payload.data(), seq);
    put_u64_be(payload.data() + 4, h);
    std::memcpy(payload.data() + 12, data.data(), data.size());
    return section_to_ts(build_mpe_section(payload.data(), payload.size()));
}

int datacast_tx_impl::get_adaptive_repeats()
{
    double q_occ;
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        q_occ = static_cast<double>(d_priority_queue.size()) /
                std::max(1, d_queue_limit);
    }

    int base = d_carousel_repeats;
    int repeats;
    // NOTE: unlike the Python prototype, this does not couple to the
    // receiver's PER (that required a shared global between separate GNU
    // Radio blocks/processes). Only queue backpressure is used here; PER-
    // aware coupling can be added later via a GR message port.
    if (q_occ > 0.75) repeats = 1;
    else if (q_occ > 0.4) repeats = std::max(1, base / 2);
    else repeats = base;

    return std::max(1, std::min(repeats, base + 2));
}

void datacast_tx_impl::enqueue_new_packet(const std::vector<std::vector<uint8_t>>& ts_pkts)
{
    int repeats = get_adaptive_repeats();

    std::unique_lock<std::mutex> lk(d_mutex);
    d_cond.wait(lk, [this] {
        return d_priority_queue.size() < static_cast<std::size_t>(d_queue_limit) ||
               d_stop.load();
    });
    if (d_stop.load()) return;

    for (const auto& p : ts_pkts) d_priority_queue.push_back(p);

    if (repeats > 1) {
        carousel_group g;
        g.orig = ts_pkts;
        g.rem = ts_pkts;
        g.repeats_left = repeats - 1;
        d_carousel_pending.push_back(std::move(g));
    }
}

void datacast_tx_impl::enqueue_control_burst(const std::vector<std::vector<uint8_t>>& ts_pkts)
{
    std::lock_guard<std::mutex> lk(d_mutex);
    for (const auto& p : ts_pkts) d_priority_queue.push_back(p);
}

void datacast_tx_impl::sniffer_loop()
{
    int tun = ::open("/dev/net/tun", O_RDWR);
    if (tun < 0) {
        std::cerr << "[TX] TUN Error: could not open /dev/net/tun\n";
        return;
    }
    d_tun_fd = tun;

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    std::strncpy(ifr.ifr_name, d_interface.c_str(), IFNAMSIZ - 1);

    if (::ioctl(tun, TUNSETIFF, reinterpret_cast<void*>(&ifr)) < 0) {
        std::cerr << "[TX] TUN Error: TUNSETIFF failed\n";
        ::close(tun);
        d_tun_fd = -1;
        return;
    }

    bool preexisting = check_interface_exists(d_interface);
    std::system(("ip link set " + d_interface + " mtu " + std::to_string(d_mtu)).c_str());
    if (!preexisting) {
        std::system(("ip addr add 10.0.0.1/24 dev " + d_interface).c_str());
    }
    std::system(("ip link set " + d_interface + " up").c_str());

    auto last_heartbeat = std::chrono::steady_clock::now();
    auto last_packet_time = std::chrono::steady_clock::now();
    bool first_packet = true;
    std::vector<uint8_t> buf(static_cast<std::size_t>(d_mtu) + 100);

    static const uint16_t noisy_ports[] = { 5353, 5355, 1900, 67, 68, 123, 137, 138, 17500 };

    while (!d_stop.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(tun, &rfds);
        struct timeval tv { 0, 100000 };
        int rv = ::select(tun + 1, &rfds, nullptr, nullptr, &tv);
        auto now = std::chrono::steady_clock::now();

        if (rv > 0 && FD_ISSET(tun, &rfds)) {
            ssize_t n = ::read(tun, buf.data(), buf.size());
            if (n >= 20 && (buf[0] >> 4) == 4 && buf[9] == 17) {
                std::size_t ihl = (buf[0] & 0x0F) * 4;
                bool skip = false;
                if (static_cast<std::size_t>(n) >= ihl + 4) {
                    uint16_t dst_port =
                        (static_cast<uint16_t>(buf[ihl + 2]) << 8) | buf[ihl + 3];
                    for (uint16_t p : noisy_ports) {
                        if (dst_port == p) { skip = true; break; }
                    }
                }
                if (!skip) {
                    last_packet_time = now;
                    if (first_packet) {
                        auto start_pkts =
                            build_control_ts(SEQ_START, "STREAM_START_PREAMBLE_SYNC");
                        for (int r = 0; r < 5; r++) enqueue_control_burst(start_pkts);
                        first_packet = false;
                        d_tx_packet_id = 1;
                    }

                    uint64_t h = xxh64(buf.data(), static_cast<std::size_t>(n));
                    std::vector<uint8_t> payload(APP_HEADER_SIZE + n);
                    put_u32_be(payload.data(), d_tx_packet_id);
                    put_u64_be(payload.data() + 4, h);
                    std::memcpy(payload.data() + 12, buf.data(), n);

                    auto ts_pkts = section_to_ts(build_mpe_section(payload.data(), payload.size()));
                    enqueue_new_packet(ts_pkts);
                    d_tx_packet_id++;
                }
            }
        }

        double inactivity = std::chrono::duration<double>(now - last_packet_time).count();
        if (!first_packet && inactivity > 1.5) {
            auto eot_pkts = build_control_ts(SEQ_EOT, "STREAM_END_EOT_SIGNAL");
            for (int r = 0; r < 5; r++) enqueue_control_burst(eot_pkts);
            {
                std::lock_guard<std::mutex> lk(d_mutex);
                d_carousel_pending.clear();
            }
            first_packet = true;
        }

        double since_hb = std::chrono::duration<double>(now - last_heartbeat).count();
        if (d_heartbeat_interval > 0 && since_hb >= d_heartbeat_interval) {
            last_heartbeat = now;
            auto hb_pkts = build_control_ts(SEQ_HEARTBEAT, "HEARTBEAT_SIGNAL_KEEP_ALIVE");
            std::lock_guard<std::mutex> lk(d_mutex);
            if (d_priority_queue.size() < static_cast<std::size_t>(d_queue_limit)) {
                for (const auto& p : hb_pkts) d_priority_queue.push_back(p);
            }
        }
    }

    if (d_tun_fd >= 0) {
        ::close(d_tun_fd);
        d_tun_fd = -1;
    }
}

bool datacast_tx_impl::start()
{
    d_stop = false;
    d_thread = std::thread(&datacast_tx_impl::sniffer_loop, this);
    return gr::sync_block::start();
}

bool datacast_tx_impl::stop()
{
    d_stop = true;
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        d_cond.notify_all();
    }
    if (d_thread.joinable()) d_thread.join();
    return gr::sync_block::stop();
}

int datacast_tx_impl::work(int noutput_items,
                            gr_vector_const_void_star& /*input_items*/,
                            gr_vector_void_star& output_items)
{
    uint8_t* out = reinterpret_cast<uint8_t*>(output_items[0]);
    float* out_debug = reinterpret_cast<float*>(output_items[1]);

    std::vector<uint8_t> working_buf;
    working_buf.swap(d_residual);

    while (working_buf.size() + TS_PACKET_SIZE <= static_cast<std::size_t>(noutput_items)) {
        std::vector<uint8_t> pkt;
        bool have_pkt = false;

        {
            std::lock_guard<std::mutex> lk(d_mutex);
            if (!d_priority_queue.empty()) {
                pkt = std::move(d_priority_queue.front());
                d_priority_queue.pop_front();
                have_pkt = true;
            } else if (!d_carousel_pending.empty()) {
                carousel_group& g = d_carousel_pending.front();
                pkt = g.rem.front();
                g.rem.erase(g.rem.begin());
                have_pkt = true;
                if (g.rem.empty()) {
                    int repeats_left = g.repeats_left;
                    std::vector<std::vector<uint8_t>> orig = std::move(g.orig);
                    d_carousel_pending.pop_front();
                    if (repeats_left > 1) {
                        carousel_group ng;
                        ng.rem = orig;
                        ng.orig = std::move(orig);
                        ng.repeats_left = repeats_left - 1;
                        d_carousel_pending.push_back(std::move(ng));
                    }
                }
            }
        }
        d_cond.notify_all();

        std::size_t start_idx = working_buf.size();
        if (have_pkt) {
            working_buf.insert(working_buf.end(), pkt.begin(), pkt.end());
            working_buf[start_idx + 3] = (working_buf[start_idx + 3] & 0xF0) | (d_cc & 0x0F);
            d_cc = (d_cc + 1) & 0x0F;
        } else {
            working_buf.insert(working_buf.end(), d_null_pkt.begin(), d_null_pkt.end());
            working_buf[start_idx + 3] =
                (working_buf[start_idx + 3] & 0xF0) | (d_null_cc & 0x0F);
            d_null_cc = (d_null_cc + 1) & 0x0F;
        }
    }

    std::size_t out_size = (working_buf.size() / TS_PACKET_SIZE) * TS_PACKET_SIZE;
    if (out_size > 0) std::memcpy(out, working_buf.data(), out_size);

    float q_total;
    {
        std::lock_guard<std::mutex> lk(d_mutex);
        q_total = static_cast<float>(d_priority_queue.size() + d_carousel_pending.size());
    }
    for (std::size_t i = 0; i < out_size; i++) out_debug[i] = q_total;

    if (working_buf.size() > out_size) {
        d_residual.assign(working_buf.begin() + out_size, working_buf.end());
    }

    return static_cast<int>(out_size);
}

} // namespace isdbt
} // namespace gr
