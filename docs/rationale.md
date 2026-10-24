# Rationale and measurements

Everything here was measured in October 2026 on packets read from APRS-IS (read-only logins).
The raw captures are not published: they contain real callsigns and positions. `tools/train_model.py`
retrains from any APRS-IS text capture.

## Why not just zlib?

Packets are 40-100 bytes. zlib, LZMA and bzip2 expand them or save almost nothing. Raw deflate with
a trained dictionary saved about 37%; a static order-0 Huffman code about 26%. An order-3 context
model with a range coder did best, because the statistics (callsign characters, `APLRT`, `WIDE1-1`,
position digits, common comment words) are learned ahead of time.

## Compression results (held-out stations)

Trained on 70% of the stations, tested on the other 30% (4,514 packets, mean plain frame 75.6 B
including the 3-byte header). `--min-count` prunes rare contexts and sets the table size.

| `--min-count` | Table size (70% data) | Saved |
|---|---|---|
| 4 | 179 KB | 52% |
| 6 | 121 KB | 50% |
| 8 | 94 KB | 48% |
| 12 | 64 KB | 45% |
| 20 | 39 KB | 42% |

Model v1 is trained on all 14,186 packets at min count 8 (125 KB of flash). Expect about 48% on
stations it has not seen. Training-set size matters less than the pruning level, but more data
helps the ratio at a given size a little.

## Compared with APRS 438

APRS 438 (ON4AA; aprs438.readthedocs.io) hand-designs a fixed layout: 4 bytes of callsign, 1 byte
of SSID/path/type, and a 12-14 byte compressed position, 17-19 bytes total. It has no comment
field. On 555 RF-heard VHF position packets:

| Method | Mean bytes | Saved | Comment kept? |
|---|---|---|---|
| Plain text (+3 B header) | 80.7 | – | yes (baseline) |
| **Like for like: the whole packet is carried** | | | |
| Order-3 model, whole packet (a smaller, earlier model) | 33.6 | 58% | yes |
| APRS 438 plus the comment as a second frame | 38.2 | 53% | yes |
| 438 header and position, comment by the model | 32.8 | 59% | yes |
| **Not like for like: part of the packet is thrown away** | | | |
| APRS 438, position only (comment dropped) | 17.0 | 79% | **no** |

438 is smallest only in the row that drops the comment (98% of position packets carry one, mean 33.5 characters, most
longer than 438's 28-character limit). Only 44% of the VHF paths fit 438's four path codes. The 438
sizes are the documented layout, not output from a real encoder.

## VHF (AX.25) framing

On VHF only the info field can be compressed, because digipeaters must read the binary address
fields. On 834 held-out VHF packets: info field 52.3 B -> 34.4 B (34%), whole AX.25 frame 79.0 B ->
61.1 B (23%), 0.53 s -> 0.41 s at 1200 baud (TXDELAY and preamble not counted). See spec/07.

## Airtime (SF12 / BW125 / CR4:5, one position beacon with a 19-character comment)

| Form | Bytes | SF12 | SF11 |
|---|---|---|---|
| Plain + comment | 57 | 2.63 s | 1.40 s |
| `compress=1` (Base91) + comment | 51 | 2.47 s | 1.31 s |
| Plain, no comment | 38 | 1.97 s | 1.07 s |
| `compress=1`, no comment | 32 | 1.81 s | 0.99 s |
| APRS 438 core | 17-18 | 1.32 s | 0.66 s |

## What other mesh systems do

Moved to the phase 2 repo (`aprs-smarts`, `docs/other-mesh-systems.md`), together with the comparison PDF.

## Limits of these numbers

One 7-minute VHF window and about an hour of LoRa traffic. The LoRa packets are worldwide (mostly European); the VHF packets were taken within 2,500 km of central US. By callsign prefix the combined corpus is mostly non-US. Packets are
unique full strings, so a station that beacons constantly counts once. No over-the-air test yet.
