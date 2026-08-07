#!/usr/bin/env python3
"""
Metrics pipeline demo (no RF hardware needed).

Synthesizes a noisy QPSK symbol stream and feeds it through the C++ port of
the metrics pipeline (isdbt.rms_monitor, isdbt.mer_snr_estimator,
isdbt.metrics_logger), the same three blocks that in a real flowgraph would
tap the receiver's `blocks.rms_cf` output and the equalized constellation
stream. Useful to see the CSV/console output without needing an SDR link.

Usage:
    python3 metrics_demo.py
    # Ctrl-C to stop; results are also in resultados_isdbt_demo.csv
"""

from gnuradio import gr, blocks, analog
import gnuradio.isdbt as isdbt
import signal
import time


class metrics_demo(gr.top_block):
    def __init__(self):
        gr.top_block.__init__(self, "Metrics Pipeline Demo")

        samp_rate = 32000

        # Synthetic QPSK-like symbol stream + AWGN, just to exercise the
        # metrics blocks end-to-end without needing a real receiver.
        qpsk_symbols = [1 + 1j, 1 - 1j, -1 + 1j, -1 - 1j]
        qpsk_symbols = [s / (2 ** 0.5) for s in qpsk_symbols]
        self.symbol_source = blocks.vector_source_c(qpsk_symbols, True)
        self.noise_source = analog.noise_source_c(analog.GR_GAUSSIAN, 0.15, 0)
        self.add = blocks.add_cc()

        # RMS branch (mirrors RMS_ISO.py: fed by GNU Radio's own rms_cf).
        self.rms = blocks.rms_cf(0.0001)
        self.rms_monitor = isdbt.rms_monitor()

        # MER/SNR branch (mirrors MER_SNR_ISO.py).
        self.mer_snr_estimator = isdbt.mer_snr_estimator("QPSK")

        # CSV/console logger (mirrors loggermaster.py). Any continuous
        # stream works as its scheduling clock; reuse the RMS output.
        self.metrics_logger = isdbt.metrics_logger("resultados_isdbt_demo.csv", 1.0)

        self.connect((self.symbol_source, 0), (self.add, 0))
        self.connect((self.noise_source, 0), (self.add, 1))
        self.connect((self.add, 0), (self.rms, 0))
        self.connect((self.rms, 0), (self.rms_monitor, 0))
        self.connect((self.rms, 0), (self.metrics_logger, 0))
        self.connect((self.add, 0), (self.mer_snr_estimator, 0))


def main():
    tb = metrics_demo()
    tb.start()

    def _stop(sig, frame):
        tb.stop()
        tb.wait()

    signal.signal(signal.SIGINT, _stop)
    signal.signal(signal.SIGTERM, _stop)

    print("[demo] Synthetic noisy QPSK -> RMS/MER/PER metrics pipeline running. Ctrl-C to stop.")
    while True:
        time.sleep(1)


if __name__ == "__main__":
    main()
