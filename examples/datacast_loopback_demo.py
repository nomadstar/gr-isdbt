#!/usr/bin/env python3
"""
Datacast loopback demo (no RF hardware needed).

Connects isdbt.datacast_tx directly to isdbt.datacast_rx over an in-process
byte stream, bypassing the ISDB-Tb PHY chain entirely. Useful to validate
that the C++ port of the MPE encapsulation/carousel/reorder/rescue logic
round-trips real IP/UDP traffic correctly, without needing SDR hardware or a
GRC session.

Requires root (creates two TUN interfaces, tun0 for TX capture and tun1 for
RX delivery) and the `ip` command from iproute2.

Usage:
    sudo python3 datacast_loopback_demo.py
    # In another terminal, send traffic into tun0 (created automatically):
    sudo ip addr add 10.0.0.2/24 dev tun0   # if not already configured
    echo "hello datacast" | nc -u 10.0.0.1 9999
    # The recovered UDP payload is written by datacast_rx to tun1 and can be
    # observed with: sudo tcpdump -i tun1 -A
"""

from gnuradio import gr, blocks
import gnuradio.isdbt as isdbt
import signal
import time


class datacast_loopback_demo(gr.top_block):
    def __init__(self):
        gr.top_block.__init__(self, "Datacast Loopback Demo")

        self.datacast_tx = isdbt.datacast_tx(
            interface="tun0",
            modulation="QPSK",
            fec="2/3",
            private_pid=0x0100,
            dst_mac="FF:FF:FF:FF:FF:FF",
            heartbeat_interval=0.5,
            queue_limit=5000,
            carousel_repeats=2,
        )
        self.datacast_rx = isdbt.datacast_rx(
            target_pid=0x0100,
            port_detection_window=20,
            reorder_window=50000,
            output_interface="tun1",
        )
        self.payload_sink = blocks.file_sink(gr.sizeof_char, "/tmp/datacast_loopback_payload.bin", False)
        self.payload_sink.set_unbuffered(True)

        # TX emits a 188B-aligned TS byte stream; RX consumes it directly -
        # no PHY (modulation/RF/demodulation) in between.
        self.connect((self.datacast_tx, 0), (self.datacast_rx, 0))
        self.connect((self.datacast_rx, 0), (self.payload_sink, 0))


def main():
    tb = datacast_loopback_demo()
    tb.start()

    def _stop(sig, frame):
        tb.stop()
        tb.wait()

    signal.signal(signal.SIGINT, _stop)
    signal.signal(signal.SIGTERM, _stop)

    print("[demo] datacast_tx (tun0) -> datacast_rx (tun1) running. Ctrl-C to stop.")
    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
