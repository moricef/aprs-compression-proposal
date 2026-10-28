# Hybrid frame layouts: size, relay and timing harness

Code behind [`docs/hybrid-layouts-results.pdf`](../../docs/hybrid-layouts-results.pdf). It compares three ways to lay out a compressed LoRa
APRS frame, so the numbers can be checked and rerun on other captures (for example the Ariège and Arizona sets).

| Layout | Native part (a relay reads and edits it) | PPM part (a relay copies it) | Table to train |
|---|---|---|---|
| **whole packet** (APRS-PPM v2.0.0) | type byte | `SRC>DST,PATH:INFO` | `--mode whole` |
| **hybrid A** | type byte, path block, RXT tuples + mask | `SRC>DST:INFO` | `--mode nopath` |
| **hybrid B** | packed source (5 B), destination code (1 B), path block, RXT | `:INFO` | `--mode info` |

Bytes are measured with the real reference codec (`reference/aprs_ppm.c`); every frame is decoded by a receiver that holds only what it was
sent and compared with the original, and the run reports decode failures (must be 0).

## Credit

**RXT was created by Jon N7UV**: digipeaters appending per-hop reception measurements (RSSI, SNR, frequency offset, time to hop). moricef worked
with Jon on its extensions and its integration into the native format: the RXT v2 format and the native-frame version with its mask byte
(`docs/RXT.md`, `docs/RXT_V2.md` in `moricef/LoRa_APRS_iGate`, branch `rxt`). This harness only counts its size. The native part and several of the other ideas come from his native LoRa APRS frame (`moricef/LoRa_APRS_iGate` branch `rxt`,
`moricef/LoRa_APRS_Tracker` branch `devel`) and his measurements on the Ariège and Arizona captures: a native frame that structures the
fields and ties each RXT measurement to the relay that made it, the RXT tuple and mask byte as integrated by him, packed callsigns and short path codes, keeping the
RXT block outside the compressed payload, "source in clear, then PPM of the rest" (the model for hybrid B) and the relay timing method. The
native bytes in `hybrid_layouts.c` are simplified stand-ins for those fields, not his encoder; his encoder is used only for the native column
(`tools/compare_native/`). Hybrid A, the matched-table comparison, the mixed-reception model and this harness are new in this work.

## Quick start

    INPUTS="capture1.txt capture2.txt.gz" tools/hybrid/run_pipeline.sh

`INPUTS` are raw captures, one packet per line: APRS-IS style `SRC>DST,PATH,qAR,IGATE:info` or plain `SRC>DST,PATH:info`.
Options (environment): `MINCOUNT=128` (raise until each table fits the 16-bit offset limit; about 128 for 175,000 packets),
`WORK=dir`, `PREP_ARGS="--all-rf --dest-prefix ''"` for logs without a q-construct or with other destinations, `TRAIN_ARGS=...`.
It prepares the test set, trains one table per layout, builds three binaries, runs them (direct, one relay hop, with RXT) and prints the tables.

## The steps

1. `prep.py OUT.tsv FILE...` writes the test set: only stations whose callsign hash falls in buckets 0 to 2 of 10 (a held-out 30%), duplicates
   removed, each station tagged stationary / moving. `--all-rf` treats every row as heard over RF; `--dest-prefix ''` accepts any destination.
2. `train_layout_model.py` is `tools/train_model.py` plus `--mode whole|nopath|info`. Train all three on the same files with
   `--holdout 0.3` and the same `--min-count`, so the test stations are in no training set and the tables differ only in the text they learn.
3. `hybrid_layouts.c` is the size harness (`cc -std=c99 -O2 -Ireference tools/hybrid/hybrid_layouts.c reference/aprs_ppm.c MODEL.c -lm`).
   `hybrid_layouts TSV [N...]`: N is the full-frame period (every Nth transmission is full, the rest primed). Environment: `LAYOUTS=1|2|4`
   (whole / A / B; run each layout with its own table), `NOHOP=1`, `RXT=R` (R relays each add a 4-byte tuple plus one mask byte, relayed
   frames only), `RXTFULL=1` (RXT on full frames only). Relay hop: `WIDE1-1` replaced by the relay's callsign; whole-packet relays are
   assumed to re-encode with their own priming state (this favors whole packet).
4. `summarize.py whole.out nopath.out info.out` prints the tables.
5. `mixed_reception.c`: a receiver hearing a station directly and through a relay (independent links, one reference per source, as
   `AprsCompressStore` keeps). `mixed TSV N pd ph pr` with pd = receiver hears the originator, ph = relay hears it, pr = receiver hears the relay.
   Whole packet vs hybrid A only, both coded with the one table you link (use `model_whole.c`); the decode shares do not depend on the table, the byte columns do slightly.
6. `bench/` is the ESP32 timing firmware: encode and decode time, full and primed, per layout, on the iGate firmware's own codec.
   `gen_bench_data.py` writes the packet sets, `make_firmware_model.py` converts a table, `make_bench_libs.sh IGATE_LIB model_whole.c model_nopath.c model_info.c`
   builds three library copies (IGATE_LIB is `lib/AprsCompress` of LoRa_APRS_iGate, branch `feature/order3-compression`), then
   `pio run -e whole -t upload` (and `nopath`, `info`) and read the `RESULT` lines on the serial port at 115200.

The native comparison (`tools/compare_native/`) needs moricef's encoder files, which are GPL and not in this repository; see its README.
Run it on the same `OUT.tsv` text (column 5) to get the native column.

## Choices and limits worth checking

* **Do not use the shipped model v1 for these comparisons.** It was trained on all stations, so test stations leak into it and it favors the
  layout closest to its training text. The pipeline trains its own tables.
* Hybrid B counts: 5 bytes for the packed callsign and SSID (6 letters or digits plus a 4-bit SSID), 1 byte for a destination code from a table of 14
  common LoRa APRS destinations in `hybrid_layouts.c` (edit `DCODES` for your network; others cost 1 + length), 1 byte for a path code (`CODES`;
  a path not in the table costs 2 + length; after a relay hop a 6-byte block: code, packed relay callsign, flags). The codec's own 3-byte
  prefix doubles as the native type byte, so B adds no separate header byte. Primed frames in B drop the 1-byte station id.
* Texts that the codec cannot shrink go out as plain frames (about a third of info-only fields in our data).
* Savings count every beacon, so fixed stations that repeat their beacon raise the figures; compare layouts, not absolute values across corpora.
* Hops are simulated: no collisions, independent links, unlimited reference storage in the harness (the firmware store holds 24 stations).
* The relay time for hybrid is not measured here: an edit-and-copy relay was only timed as a stand-in.
