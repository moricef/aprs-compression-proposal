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
* Hybrid B counts: 5 bytes for the packed callsign and SSID (1 to 6 letters or digits plus a numeric SSID 0 to 15; other SSIDs stay text), 1 byte for a destination code from a table of 14
  common LoRa APRS destinations in `hybrid_layouts.c` (edit `DCODES` for your network; others cost 1 + length), 1 byte for a path code (`CODES`;
  a path not in the table is costed as a packed block: 1 header byte, 1 byte per `WIDEn-N`, 5 bytes per packable callsign, 1 + length otherwise;
  `PACKPATH=0` restores the first version's 2 + length; after a relay hop a 6-byte block: code, packed relay callsign, flags). The codec's own 3-byte
  prefix doubles as the native type byte, so B adds no separate header byte. Primed frames in B drop the 1-byte station id.
* Texts that the codec cannot shrink go out as plain frames (about a third of info-only fields in our data).
* Savings count every beacon, so fixed stations that repeat their beacon raise the figures; compare layouts, not absolute values across corpora.
* Hops are simulated: no collisions, independent links, unlimited reference storage in the harness (the firmware store holds 24 stations).
* The relay time for hybrid is not measured here: an edit-and-copy relay was only timed as a stand-in.

## Our data and moricef's data disagree about hybrid A and B

moricef ran this harness, unmodified, on his Ariège (France) and Arizona captures (full frames only, N=1, no RXT, bytes saved):

| | Whole packet | Hybrid A | Hybrid B |
|---|---|---|---|
| His Ariège (8,062 test packets, 44 stations) | 56.2% | 38.8% | 34.8% |
| His Arizona (224 packets, 11 stations) | 58.3% | 42.5% | 43.2% |
| Our APRS-IS capture, first version of the harness | 45.8% | 44.1% | 45.1% |

On our data the hybrids are within about 2 points of whole packet at N=1. On his they are 15 to 21 points behind. **We do not see that result in
our data.** This section records what we checked, what explains it so far, and what is still open.

**What differs between the two captures.** 72% of his test packets have a path outside the harness's seven path codes (`F6ZDD-10*`,
`F6DEV-10*,WIDE2-1`: packets already relayed once). In our held-out stations only 9% do, and in the French stations in our capture (5,667 packets,
144 stations) 11%. Our capture comes from APRS-IS and is mostly packets delivered by a gateway with an ordinary path; his is an RF capture
that hears many already-relayed packets.

**Why that hurts the hybrids in the harness.** Whole-packet PPM learns common paths and relay callsigns and charges almost nothing for them. In
the first version of the harness the native path block cost 1 byte for a coded path and 2 + text length for any other (measured on our packets,
per packet, with the whole-packet table: coded paths 1 byte native against about 0 in PPM; other paths 13.5 bytes native against 5.5 in PPM).
On packets with an unusual path that is about 8 bytes per packet, about 11 points of a 73-byte frame. moricef pointed out that his encoder packs
ordinary callsigns into 5-byte elements, so this overstates the cost.

**Costing the path with packed callsign elements** (current harness; `PACKPATH=0` gives the old cost). Our held-out stations, N=1 / 2 / 4:

| | Whole packet | Hybrid A, old cost | Hybrid A, packed | Hybrid B, old cost | Hybrid B, packed |
|---|---|---|---|---|---|
| All packets | 45.8 / 56.9 / 62.6 | 44.1 / 55.0 / 60.6 | 44.8 / 55.7 / 61.3 | 45.1 / 52.9 / 56.9 | 45.8 / 53.6 / 57.6 |
| Only unusual paths (8,608 packets) | 40.7 / 56.2 / 63.7 | 31.2 / 44.1 / 50.2 | 39.7 / 52.6 / 58.7 | 32.4 / 41.7 / 46.1 | 41.0 / 50.3 / 54.7 |
| Only common paths (84,537 packets) | 46.2 / 57.0 / 62.6 | 45.3 / 56.0 / 61.5 | same | 46.2 / 53.9 / 57.9 | same |

On our packets with unusual paths, hybrid A is 9.5 points behind whole packet at N=1 with the old cost and 1 point behind with the packed cost.
That agrees with his explanation of most of the gap. It does not reproduce all of it: his gap is 17 points, and the packed cost leaves hybrid A about 1 point
(N=1) to 4 points (N=2) behind on our unusual-path packets, so the rest is not explained. The reasons we can name but have not tested:
his tables were trained on his own small network with `--min-count 2`, so PPM learns that network's callsigns and paths; his test set is small
(11 stations in Arizona); the harness's cost for his native fields is still an estimate and not his encoder. His real encoder's costs are the next check.

**What this does not show.** It does not show that his captures are unrepresentative. Already-relayed packets are normal on RF, and our
APRS-IS capture probably undercounts them. It also does not show that the hybrids are better on his data: only that the cost model was the main
reason for the gap. Hybrid B still falls behind whole packet as priming does more of the work (N=2 and N=4).

### Building one table from several networks' data

A PPM table must be identical at both ends of every link, so a French-only table cannot be mixed with another country's table on the air. The aim is
one table trained on the union of several networks. `train_layout_model.py` can now add your packets to ours:

    # ours: tables/counts_whole.gz, counts_hybridA_nopath.gz, counts_hybridB_info.gz (byte-context counts >= 8, no packet text)
    python3 tools/hybrid/train_layout_model.py --lora YOUR_CAPTURES... --mode whole --holdout 0.3 --version 2 \
        --base-counts tools/hybrid/tables/counts_whole.gz --base-weight 1.0 \
        --max-kb 105 --out model_whole.c          # nopath: 100, info: 79
    # --boost W --boost-re '^(F[0-9A-Z]|TM|TK)'  counts matching stations W times; --dump-counts FILE.gz saves your own counts

`--max-kb` raises the minimum count until the table fits the size, so the merged table has the same size as ours. The held-out split is the same callsign hash,
so a station of yours in buckets 0 to 2 stays out of training. `--base-counts` alone (no packets of yours) reproduces our committed tables, apart from the packet-count comment.

**What it did on our French stations** (a stand-in for the idea, not his data): our French-prefix stations in the training split counted 3 times and
8 times in total, tables matched to the same size as the committed ones, N=1, bytes saved:

| | French stations, base | ×3 | ×8 | Other stations, base | ×3 | ×8 |
|---|---|---|---|---|---|---|
| Whole packet | 44.7 | 46.5 | 48.1 | 45.8 | 45.1 | 44.3 |
| Hybrid A | 43.7 | 44.9 | 46.3 | 44.9 | 44.1 | 43.5 |
| Hybrid B | 44.9 | 45.4 | 46.4 | 45.8 | 45.2 | 44.4 |

Weighting a region helps that region by about 0.5 to 3.5 points at the same table size and costs the rest by about 0.7 to 1.5 points, because the table
has the same size, so something else gives. A weight near 1 for a large, varied capture of his is a more careful starting point than a large boost.
It improves compression of the PPM text for every layout; it does not change the difference between whole packet and hybrid, which comes from the native fields.
