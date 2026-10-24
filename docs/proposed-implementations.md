# Proposed implementations and how much they cut

A summary of the ideas in this repo, with the saving each one is estimated to give for LoRa APRS and
for traditional (VHF, AX.25) APRS. **All figures come from simulations and offline measurement on
captured APRS-IS packets. Nothing has been tested over the air.** Details and caveats are in the
linked files; read them before quoting a number.

How to read the numbers:

* **Bytes saved** is the average cut in frame size, against the plain frame used today (LoRa frame
  of `3C FF 01` plus text; VHF AX.25 frame without flags, 73.5 B on average here).
* **Airtime saved** is smaller than bytes saved because the preamble does not shrink. LoRa figures
  use a straight-line fit to the SF12 airtimes measured earlier (about 0.65 s plus 0.035 s per byte,
  `docs/rationale.md`); VHF figures use 1200 baud plus an assumed 0.3 s of TXDELAY and preamble.
* The savings **do not add**. Priming already makes repeated text nearly free, so trimming a repeated
  comment on top of it saves much less than trimming it alone.
* Compression figures use a 144 KB table, the size that fits a small device; a 202 KB table gives
  about 2 points more (LoRa 43%, VHF info-only 22%). They come from a three-hour capture.

## Changes to the packet

| # | Implementation | LoRa APRS: bytes / airtime saved | Traditional VHF APRS: bytes / airtime saved | Legacy stations | Status and source |
|---|---|---|---|---|---|
| 1 | **Static compressed frame** (APRS-PPM): fixed trained table + range coder | **41% / about 32%** (43% / 34% with a 202 KB table) | Whole packet: **39% / 24%**. Needs a new frame type | Plain frames unchanged; legacy cannot decode the new frame, and it drops or ignores it | Codec tested on a PC; spec 01, `docs/per-country-estimate.md` |
| 2 | Info field only compressed, standard AX.25 header (PID marker) | n/a | **20% / 12%** (22% / 14% without the 2-byte overhead) | Legacy decodes the frame and shows junk in the info; a Direwolf iGate does not gate it | Spec 07; legacy test `docs/legacy-decoder-test.md` |
| 3 | Hybrid: keep source and destination addresses, compress path + info | n/a | **30% / 19%** (rough estimate from the earlier 20-minute data, not rerun) | Valid AX.25, callsign readable | Spec 07 |
| 4 | **Priming with the station's previous packet** (on top of 1) | **70% / about 56%** overall (73% for the primed packets); 63% / 50% if a full frame is sent every 4th, 55% / 44% if every 2nd | **66% / 41%** overall (68% for the primed packets); 60% if a full frame is sent every 4th (60% to 64% for stationary stations at every 4th to 8th); 53% (33% airtime) if every 2nd, the proposed default for moving stations | As row 1; receivers must also hold the reference packet | Simulation only; spec 08; `tools/delta_sim.py` |

## Changes to behaviour (no change to the packet format)

| # | Implementation | LoRa APRS | Traditional VHF APRS | Status |
|---|---|---|---|---|
| 5 | Send the comment less often, or drop it | 57 B to 38 B on one measured beacon (33% / 25% airtime), from earlier Direwolf measurements | Same idea; not measured on VHF | `docs/rationale.md` |
| 6 | Shorter digipeater paths | about 8 B per hop of text path (arithmetic) | Each hop is 7 B; the average packet carries 1.55 digipeater addresses, about 11 B (15% of the frame) | Measured composition only; the saving depends on what network coverage allows |

Rows 5 and 6 are practices stations could adopt today. They overlap with row 4, since primed packets
already carry a repeated comment and path for almost nothing.

## Ideas that cut transmissions rather than bytes

| # | Idea | Effect | Status |
|---|---|---|---|
| 7 | Implicit acks, smarter digipeating, message path learning, reach-aware repeating | Fewer repeated and redundant transmissions; **not quantified here**. One secondary source claims 95-98% less airtime for repeated mesh traffic with learned paths (MeshCore); not verified | Drafts in the companion repo `aprs-smarts`; a pilot network and simulator are needed to measure |

## What the estimates are worth

| Estimate | Basis | Main weakness |
|---|---|---|
| Static compression, LoRa and VHF | 135,299 unique packets from a three-hour capture (a European evening), held-out stations; more data did not help at a fixed table size | One window; the table is now limited by its format, not by data |
| Priming | Python simulation of ideal coding on 3,000 sampled packets; consecutive identical packets removed | Assumes the receiver holds the reference; real receivers often do not (spec 08); not real codec bytes |
| Airtime | Line fit (LoRa), assumed TXDELAY (VHF) | Ignores channel access and collisions |
| Overall | None of it is on-air | The over-the-air test is not done |

Reproduce: see `docs/how-it-works.md`, `docs/per-country-estimate.md` and the tools in `tools/`.
