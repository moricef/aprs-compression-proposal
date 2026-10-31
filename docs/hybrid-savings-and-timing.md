# Hybrid frame (spec 11): airtime saved and processor time

Status: draft, measured 1 November 2026 with the reference code in `tools/hb` on the `lacf` branch. All numbers come from one network's packets and will differ elsewhere.

## What was measured

* **Packets:** 14,305 real LoRa APRS packets heard on one set of receivers (the texts are private and not in this repository; only the totals below are published).
* **Encoder:** the C reference of the spec 11 frame (`3C FF 02`, destination code, PPM-coded content, content CRC). All 14,305 packets encode; none falls back to raw text.
* **Baseline:** the plain LoRa APRS packet, the 3-byte prefix plus the TNC2 text.
* **Airtime:** LoRa, 125 kHz, coding rate 4/5, explicit header, CRC on, 8 preamble symbols. Mean per packet, time-weighted.

## Bytes and airtime saved

| Case | Mean plain | Mean hybrid | Bytes saved | Airtime saved SF7 | SF9 | SF12 |
|---|---|---|---|---|---|---|
| As the origin sends it, no RXT (14,305 packets) | 86.6 B | 48.7 B | 43.8% | 36.3% | 35.2% | 35.0% |
| After one relay hop, with RXT (4,368 packets that a relay would forward) | 84.8 B | 52.8 B | 37.8% | 31.2% | 30.3% | 30.2% |

Mean airtime at SF7: 152 ms plain against 97 ms hybrid at the origin; 150 ms against 103 ms after a hop. At SF12: 3,565 ms against 2,317 ms.

**The airtime saving is smaller than the byte saving** because every LoRa frame pays a fixed preamble and header.

**Relayed traffic lowers it.** A relayed frame carries the RXT block (5 bytes for the first hop, 4 more for each further one) and a relay callsign in place of the one-byte `WIDE1-1`. The saving falls about linearly with the share of once-relayed traffic:

| Relayed share | SF7 | SF12 |
|---|---|---|
| 0% | 36.3% | 35.0% |
| 25% | 35.0% | 33.8% |
| 50% | 33.7% | 32.6% |
| 75% | 32.5% | 31.4% |
| 100% | 31.2% | 30.2% |

These mixes combine the origin result for all packets with the relay result for the relayable subset, so they are approximate. The baseline is the plain packet without RXT; a plain packet that also carried a text-form RXT block (7 bytes) would be longer, and the saving against it about 5 points larger.

**Your mileage may vary.** The result depends on the relay share, the path lengths and the mix of packet types (Mic-E and telemetry compress less than positions with comments). Another network measured 39.3% (Ariège, in bytes) and 43.8% (Arizona, in bytes) on its own packets with an earlier harness; the gap to the figure above is not explained, and the numbers should be read as "about a third less airtime, a little less where traffic is mostly relayed". The earlier 46.2% figure came from a harness with a different baseline and is not comparable.

## Processor time on an ESP32

LILYGO LoRa32 v2.1 (ESP32-PICO-D4, 240 MHz), 400 fabricated packets whose lengths follow the real traffic (median 80, 95th percentile 151 characters), 3 repetitions each (`tools/hb/esp32-bench`, output in `results-lora32-percentiles.txt`). WiFi on (soft access point plus scans) and off gave the same times.

| | Mean | Median | 95th | 99th | Max |
|---|---|---|---|---|---|
| Encode | 8.5 ms | 4.0 | 22.1 | 22.5 | 24.7 |
| Receive and verify | 3.9 ms | 1.9 | 11.0 | 11.2 | 12.3 |
| Relay hop (includes its own verify) | 4.2 ms | 2.2 | 11.4 | 11.6 | 12.6 |
| Relay hop as a share of the frame's own SF7 airtime | 3.6% | 2.6% | 9.0% | 10.1% | 10.2% |

Transmit time of the same frames, for scale:

| Text | Plain, SF7 | Hybrid frame | Hybrid, SF7 | Hybrid, SF12 | Relay hop |
|---|---|---|---|---|---|
| 30 chars | 72 ms | 29 B | 67 ms | 1.6 s | 0.1 ms |
| 49 | 103 ms | 36 B | 77 ms | 2.0 s | 0.1 ms |
| 80 (median) | 149 ms | 40 B | 82 ms | 2.0 s | 1.9 ms |
| 110 | 190 ms | 56 B | 108 ms | 2.6 s | 8.5 ms |
| 150 (95th percentile) | 251 ms | 132 B | 220 ms | 5.1 s | 12.6 ms |
| 183 (99th percentile) | 297 ms | 150 B | 246 ms | 5.6 s | 11.5 ms |

The two hand-made stress cases (a 249-character path case and a 253-character text) take 20 ms and 23 ms to relay, 10.3% and 6.3% of their own airtime. The very short rows are raw frames (the encoder found no gain), where the receiver only copies and checks.

Proposed rule: the 95th-percentile relay time stays under 25% of the airtime of the median compressed frame (about 25 ms). The measured 11.4 ms meets it with a factor of two to spare.

Needs at least a 12 KB task stack (10.4 KB used; the default 8 KB Arduino loop task overflows). Not measured: the T-Deck, and a WiFi client connection with APRS-IS traffic.
