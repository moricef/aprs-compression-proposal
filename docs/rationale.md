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

| Method | Mean bytes | Saved |
|---|---|---|
| Plain text (+3 B header) | 80.7 | - |
| Order-3 model, whole packet | 33.6 | 58% (this was a smaller, earlier model) |
| APRS 438, position only (comment dropped) | 17.0 | 79% |
| APRS 438 + comment as a second frame | 38.2 | 53% |
| 438 header and position + comment by model | 32.8 | 59% |

438 wins by dropping the comment (98% of position packets carry one, mean 33.5 characters, most
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

* **MeshCore:** only repeaters relay; flood once to discover a path, then send along the stored
  path; repeaters deprioritise flood packets from distant senders; path discovery rides on normal
  traffic. https://buymeacoffee.com/ripplebiz/meshcore-philosophy ; comparison
  https://simeononsecurity.com/articles/meshcore-vs-meshtastic-comparison-guide/ (secondary source;
  the 95-98% airtime figure is from one wiki page).
* **Meshtastic 2.6:** next-hop routing for direct messages, compatible with older firmware.
  https://meshtastic.org/blog/meshtastic-2-6-preview/
* **Reticulum:** short- and long-term airtime caps on RNode interfaces; boundary nodes that keep
  internet announces off the LoRa channel. Details are from a mirror of its commits and a third-party
  boundary-node project, not the official manual.
* Encryption in MeshCore/Meshtastic does not fit amateur radio content rules; nothing here uses it.

See also `LoRa-APRS-smarter-notes.pdf` in this folder.

## Limits of these numbers

One 7-minute VHF window and about an hour of LoRa traffic; central-US-heavy VHF sample. Packets are
unique full strings, so a station that beacons constantly counts once. No over-the-air test yet.
