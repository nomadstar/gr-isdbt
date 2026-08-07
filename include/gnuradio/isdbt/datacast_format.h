/* -*- c++ -*- */
/*
 * Wire-format definitions for the ISDB-Tb datacasting layer described in
 * Zamora Marambio, "Transmision de paquetes por protocolo ISDB-Tb" (UDP,
 * 2026). Ported for bit-compatibility from the reference Python prototype
 * (tx.py / rx.py, TransIsdtb repository) - see docs/datacasting-isdbtb.md.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_ISDBT_DATACAST_FORMAT_H
#define INCLUDED_ISDBT_DATACAST_FORMAT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xxhash.h>

namespace gr {
namespace isdbt {
namespace datacast {

// Sequence-number sentinels used for the control plane. Any other value of
// `seq` is a real data packet.
constexpr uint32_t SEQ_START = 0x00000000u;
constexpr uint32_t SEQ_EOT = 0xFFFFFFFEu;
constexpr uint32_t SEQ_HEARTBEAT = 0xFFFFFFFFu;

constexpr std::size_t TS_PACKET_SIZE = 188;
constexpr std::size_t TS_FIRST_PACKET_PAYLOAD = 183; // after sync+pid+cc+pointer_field
constexpr std::size_t TS_CONT_PACKET_PAYLOAD = 184;  // after sync+pid+cc
constexpr uint8_t TS_SYNC_BYTE = 0x47;
constexpr uint16_t NULL_PID = 0x1FFF;

// MPE section header is 12 bytes; the application sub-header appended right
// after it (seq + xxh64) is another 12 bytes, before the real payload.
constexpr std::size_t MPE_HEADER_SIZE = 12;
constexpr std::size_t APP_HEADER_SIZE = 12; // 4 (seq) + 8 (xxh64)
constexpr std::size_t MPE_CRC_SIZE = 4;

// CRC-32/MPEG-2: poly 0x04C11DB7, init 0xFFFFFFFF, no input/output
// reflection, no final XOR. Identical to the table built by
// _build_crc32_table() in tx.py/rx.py.
inline uint32_t crc32_mpeg2(const uint8_t* data, std::size_t len)
{
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t crc = i << 24;
            for (int b = 0; b < 8; b++) {
                crc = (crc & 0x80000000u) ? ((crc << 1) ^ 0x04C11DB7u) : (crc << 1);
            }
            t[i] = crc;
        }
        return t;
    }();

    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ table[((crc >> 24) ^ data[i]) & 0xFF];
    }
    return crc;
}

// xxHash64, seed 0 - bit-identical to Python's xxhash.xxh64(data).digest().
inline uint64_t xxh64(const uint8_t* data, std::size_t len)
{
    return XXH64(data, len, 0);
}

inline void put_u32_be(uint8_t* out, uint32_t v)
{
    out[0] = (v >> 24) & 0xFF;
    out[1] = (v >> 16) & 0xFF;
    out[2] = (v >> 8) & 0xFF;
    out[3] = v & 0xFF;
}

inline uint32_t get_u32_be(const uint8_t* in)
{
    return (uint32_t(in[0]) << 24) | (uint32_t(in[1]) << 16) | (uint32_t(in[2]) << 8) |
           uint32_t(in[3]);
}

inline void put_u64_be(uint8_t* out, uint64_t v)
{
    for (int i = 0; i < 8; i++) {
        out[i] = (v >> (56 - 8 * i)) & 0xFF;
    }
}

inline bool bytes_equal(const uint8_t* a, const uint8_t* b, std::size_t len)
{
    return std::memcmp(a, b, len) == 0;
}

} // namespace datacast
} // namespace isdbt
} // namespace gr

#endif /* INCLUDED_ISDBT_DATACAST_FORMAT_H */
