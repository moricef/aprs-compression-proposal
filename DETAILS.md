# Details: specifications, simulation tables and roadmap

(The short version is the [front page](README.md).)

## Proposals for extending APRS: compressed frames and capability announcement

**Phase 1 of bringing APRS up to date:** proposal drafts for carrying the same APRS packets in fewer
bytes (APRS-PPM compressed frames), a way for stations to learn what the stations around them can
decode, and a binding for VHF/AX.25. It extends APRS; it does not replace it, and legacy stations
keep working. Phase 2, the harder "smarts" work (smarter digipeating, path learning, reach-aware
repeating, a measured pilot), is in the companion repo `aprs-smarts` (not yet public).

**This is a draft by one person. It is not a standard.** The compressed-frame format is in early
development and is being tested over the air: it started on 24 October 2026 with an ESP32 and a Raspberry Pi running Direwolf
(both directions, every packet decoded exactly; see [docs/over-the-air-test.md](docs/over-the-air-test.md)), and the tests continue, now
with the current format (LACF, spec 11) on an iGate, a T-Deck tracker and a Direwolf station
([results so far](docs/lacf-test-results-2026-11-01.md)). Expect it to change. Licensed under the [MIT License](LICENSE).

## Try it

**LACF (spec 11, the current format).** Test firmware for the LILYGO LoRa32 V2.1 iGate and the LILYGO T-Deck Plus 433 tracker, flashable from a browser:
<https://tiles.aivu.com/lacf>. For Dire Wolf on a Raspberry Pi: branch [`feature/lora-hybrid`](https://github.com/radiohound/direwolf/tree/feature/lora-hybrid).
Test results: [docs/lacf-test-results-2026-11-01.md](docs/lacf-test-results-2026-11-01.md).

**The earlier whole-text format.** The working firmware is on a public branch, with a page that says what it is for, what has been proven, what
has not, and how to test it safely on spare hardware:

* Branch: <https://github.com/radiohound/LoRa_APRS_iGate/tree/feature/order3-compression>
* Start here: [tools/aprs_compress/EXPERIMENT.md](https://github.com/radiohound/LoRa_APRS_iGate/blob/feature/order3-compression/tools/aprs_compress/EXPERIMENT.md)

It is opt-in (transmit is off by default) and is not a pull request to anyone's project.

## Results so far (simulation on captured packets; the first over-the-air results are in docs/over-the-air-test.md)

*These results are for the earlier whole-text format. For LACF (spec 11), which is in early development and still being reviewed for change, see the [README](README.md) and [docs/hybrid-savings-and-timing.md](docs/hybrid-savings-and-timing.md).*

**The test data.** A three-hour read-only APRS-IS capture (plus a 5-minute pilot) for about 25 countries,
taken on an evening in Europe: 135,299 unique packets, of which 10,841 LoRa and 30,872 VHF packets were
held out for testing. LoRa packets were chosen by device type, so they come mostly from Europe; VHF packets
are those an iGate heard over the air. The model is trained on 70% of stations and tested on the other 30%,
so every figure is on stations the model has not seen. The raw captures hold real callsigns and are not
published, only these aggregates.

**What each saves** (average frame size against today's plain frame, 144 KB table; airtime saved is smaller
because the preamble does not shrink). Full table and caveats in
[docs/proposed-implementations.md](docs/proposed-implementations.md).

| Implementation | LoRa APRS | Traditional VHF APRS |
|---|---|---|
| Static compressed frame (spec 01): fixed trained table + range coder | **41%** bytes, about 32% airtime | whole packet in a new frame type **39%**, about 24% airtime |
| Same, but keeping the AX.25 header and compressing only the info field (spec 07) | n/a | **20%**, about 12% airtime |
| Priming each packet with the station's previous packet (spec 08) | **70%** overall, about 56% airtime | **66%** overall, about 41% airtime |
| Priming, with a **full frame every 2nd transmission: proposed default for moving stations** | 55% (all LoRa stations), about 44% airtime | **53%** (moving stations), about 33% airtime; about 87% of receivers can decode, 70% at the edge of range |
| Priming, with a full frame every 4th transmission: suggested for stationary stations | 63%, about 50% airtime | 60% (stationary stations), about 39% airtime |

**What it means.**
* LoRa APRS carries the whole packet, so compression helps most there. Traditional VHF APRS spends
  about 38% of each frame on the AX.25 addresses, which legacy digipeaters have to read, so
  compressing only the info field saves about a fifth. Using a LoRa-style frame on VHF brings it to
  about 39%, at the cost of legacy digipeaters not repeating it. See spec 07.
* Priming helps the most, because stations repeat themselves: same comment, symbol and path. It needs
  the receiver to have heard the reference packet. For moving stations that is often not the case at
  the edge of range; the data for that is weak (APRS-IS drops duplicates). See spec 08.
* More packets of the same kind did not improve the saving at a fixed table size. The limit is now
  the table format and the model.
* How often to send a full frame is a trade between saving and being decodable by receivers that missed the previous packet; the proposed default is every 2nd transmission for moving stations and every 4th to 8th for stationary ones (spec 08).
* The figures do not add up, because the methods overlap.
* **Over the air, so far, two links and three kinds of hardware** (24 to 25 October 2026: ESP32, Raspberry Pi with Direwolf, and T-Deck, in both directions, and a full-day run over about 2 miles; see `docs/over-the-air-test.md`). Tests of LACF (spec 11) followed from 31 October ([results so far](docs/lacf-test-results-2026-11-01.md)) and continue. The other figures
  come from simulation, which uses ideal coding length for the priming figures, an assumed TXDELAY for VHF
  airtime, and one evening of data (three hours, Europe).

See [docs/per-country-estimate.md](docs/per-country-estimate.md) for the result by country and device
type, and [docs/legacy-decoder-test.md](docs/legacy-decoder-test.md) for what a stock Direwolf does with
the new VHF frames.

## Roadmap for LoRa APRS (suggested order)

*This roadmap was written for the earlier whole-text format and predates LACF ([spec 11](spec/11-native-ppm-frame-byte-plan.md)), which is now the working direction; layers 1 to 3 below are the earlier format.*

Most LoRa APRS devices are ESP32 boards, so this order favours what fits and what pays off there. Each
layer works alone and adds to the one before. All savings are from simulation or arithmetic on captured
packets (see "Results so far"); the earlier format has been tested over the air on three kinds of hardware (see
`docs/over-the-air-test.md`) and LACF tests continue; the VHF binding has not been tested.

1. **Static order-3 frame ([spec 01](spec/01-compressed-frames.md)) with capability learning
   ([spec 02](spec/02-capability-announcement.md)).** About 41% saved at a 144 KB table, 43% at 202 KB.
   The iGate firmware branch has receive always on and opt-in transmit; the capability announcement of spec 02 itself is only a proposal (not implemented). *Tested over the air since 24 October 2026 (ESP32, Direwolf and T-Deck; see `docs/over-the-air-test.md`). Weak-signal and other-board tests are still to do.* It is stateless: a receiver needs nothing it heard earlier.
2. **A structured header for position packets ([draft 09](spec/09-structured-header-layout.md)).**
   Callsign, path, type and position in a compact layout; the comment with the order-3 model. About 6 to
   7 B smaller than layer 1 on a position packet (arithmetic, not built), and the biggest help for the
   short tracker beacons that layer 1 compresses least (about 29%). The comment-only table is about half
   the size, which helps trackers. Still stateless.
3. **Priming with the station's last full packet ([spec 08](spec/08-priming-from-previous-packet.md)).**
   **Built** in the C reference codec and the iGate firmware branch (second frame type `3C FF 0xC1`; a full frame
   at least every Nth packet per station, `compressFullEvery`, default 2). On real packets from stations the
   table had not seen, a primed frame averaged 21 B against 33 B for a full frame and 60 B plain (65% against
   45% saved, real codec bytes). In simulation, about 66 to 70% overall; 53% with a full frame every 2nd
   transmission for moving stations, and 60 to 64% with one every 4th to 8th for stationary ones. Needs the
   receiver to have heard the reference, and about 13 KB of RAM for the two packet stores on the ESP32. Tested
   over the air once (ESP32 to Direwolf): primed frames 13.8 B, full frames 38.0 B, against 75 B plain for
   an average-sized position, 65% smaller overall.

Alongside, no new code needed: send the comment less often or shorten it (33% on one measured beacon).

Not planned first: VHF. The compatible VHF form saves 21 to 24% and the LoRa-style frame 41 to 45%, but
digipeaters, iGates and KISS TNCs make deployment much harder (spec 07).

**On APRS 438.** Layer 2 borrows 438's central idea, a fixed binary header. It does not use 438's bytes:
438's header byte is full (4 bits SSID, 2 path, 2 data type) with no `WIDE1-1` code, so only 53% of LoRa
position packets fit; it has no comment field; and its position field is the ordinary base-91 text. See
[docs/alternatives-considered.md](docs/alternatives-considered.md).

## Parts

| Part | Status |
|---|---|
| [01 Compressed frames (APRS-PPM)](spec/01-compressed-frames.md) | Spec + C reference codec + frozen model v1 + test vectors. Host-tested, and tested over the air in short runs (24 to 25 October 2026) |
| [02 Capability announcement](spec/02-capability-announcement.md) | Proposal draft: how a sender learns a station can decode compressed frames |
| [07 AX.25 binding (VHF)](spec/07-ax25-binding.md) | Proposal draft: carry the same compressed payload in AX.25 info fields; smaller gain, marker undecided |
| [08 Priming with a station's last full packet](spec/08-priming-from-previous-packet.md) | Wire format + C reference codec + 103 test vectors + firmware; about 65% saved on real LoRa pairs (real codec bytes), 62-70% in simulation. Host-tested, and tested over the air in short runs |
| [09 Structured header for position packets](spec/09-structured-header-layout.md) | Idea: a compact fixed layout for callsign, path and position plus an order-3 comment; about 4-7 B smaller than order-3 alone on LoRa position packets (arithmetic) |
| [11 Native PPM frame: byte plan](spec/11-native-ppm-frame-byte-plan.md) | **DRAFT, in early development and still being reviewed for change.** Byte plan for a first common format: moricef's native frame (`3C FF 02`) with a PPM-coded content field, the PPM payload integrity rules (deterministic ending, CRC-16, re-encode check), and the RXT block at the end of the frame with the mask byte last (tested against truncation). Test implementations exist (the C and Python reference, and test branches of the iGate, tracker and Dire Wolf) and have run on the air in short tests; expect the format to change |
| [Direwolf notes](docs/direwolf-notes.md) | Where the Direwolf implementation is (LoRa HAT channel, built) and planning notes for the VHF part |
| [Other methods considered, APRS 438, and why APRS-PPM was chosen](docs/alternatives-considered.md) | Comparison table and the reasoning |
| [How the compression works and how it was trained](docs/how-it-works.md) | Plain-language explanation, comparison with other methods, training method and caveats |
| [Compression by country (rough estimate)](docs/per-country-estimate.md) | Estimate per country and device type, LoRa and VHF, from a three-hour capture (135,299 packets), with a head-to-head check that more data does not help at a fixed table size |
| [Legacy decoder test (no radio)](docs/legacy-decoder-test.md) | What stock Direwolf does with compressed VHF frames, using generated audio |
| [Proposed implementations and how much they cut](docs/proposed-implementations.md) | One table: LoRa and VHF, bytes and airtime, with status |
| [Rationale and measurements](docs/rationale.md) | Numbers behind the above |

## Layout

    spec/        the drafts
    reference/   plain-C codec (integer-only, C99)
    model/       frozen model v1 table + SHA-256
    tools/       train_model.py (retrain), capture_aprsis.py (read-only APRS-IS capture); tools/hb: C and Python reference of LACF (spec 11)
    test/        test vectors (fabricated packets) and the tool that checks them; vectors_hb_*.txt are the LACF vectors
    docs/        measurements, notes

## Try it

    make test                     # checks the reference codec against test/vectors_v1.txt (earlier format)
    make hb-test                  # checks the LACF (spec 11) reference code and its vectors
    build/ppm_tool stats < packets.txt

## Implementations

**LACF (spec 11, in early development and still being reviewed for change)**

* C and Python reference code (`tools/hb`; `make hb-test`), checked against 554 encode, 503 relay and 38 rejection vectors and against each other on 142,282 frames.
* Test firmware, flashable from a browser: <https://tiles.aivu.com/lacf>. Sources: iGate [`feature/lacf`](https://github.com/radiohound/LoRa_APRS_iGate-1/tree/feature/lacf),
  tracker [`hybrid`](https://github.com/radiohound/LoRa_APRS_Tracker-moricef/tree/hybrid), Dire Wolf [`feature/lora-hybrid`](https://github.com/radiohound/direwolf/tree/feature/lora-hybrid).
  Test results: [docs/lacf-test-results-2026-11-01.md](docs/lacf-test-results-2026-11-01.md).

**The earlier whole-text format (superseded by LACF)**

* C reference codec (`reference/`), including primed frames; `make test` checks 26 full-frame and 103 primed
  test vectors, that a primed frame is rejected if the receiver's reference differs, and a collision test (a stale
  reference with the same 8-bit check, and cut frames). See `docs/known-issues.md`.
* Firmware fork of richonguzman/LoRa_APRS_iGate with receive-always, opt-in transmit
  (`lora.compressTx`) and primed frames (`lora.compressFullEvery`): public branch
  [`feature/order3-compression`](https://github.com/radiohound/LoRa_APRS_iGate/tree/feature/order3-compression), builds for `ttgo-lora32-v21` (flash 88.3%, RAM 21.7%). Its C++ codec
  produces byte-identical frames to the reference on 4,520 full-frame vectors and all 103 primed vectors,
  and a host test simulates a sender and receiver with a lost full frame and a lost primed frame. A
  self-test build exercises both frame types between two boards.
* **Direwolf (LoRa HAT channel):** receive always on, opt-in transmit (`LORACOMPRESS`), primed frames, and an iGate that
  expands compressed frames and gates them to APRS-IS as normal packets. Branch
  [`feature/lora-compression`](https://github.com/radiohound/direwolf/tree/feature/lora-compression) of the
  `radiohound/direwolf` fork, with its own instructions in `doc/LoRa-Compression.md`. It sits on the LoRa driver
  of the pending pull request #638 and is **not** part of that pull request. The VHF/AX.25 side of Direwolf is still
  only planned (`docs/direwolf-notes.md`).
* **T-Deck tracker** (a fork of the LoRa APRS Tracker firmware of moricef): receive always on, opt-in transmit. Branch
  [`ppm-compression`](https://github.com/radiohound/LoRa_APRS_Tracker-moricef/tree/ppm-compression), start at
  its `docs/APRS-PPM-TDECK.md`.
* An STM32WLE5 tracker build is a candidate; nothing is built.

## TODO

* Reframed as proposal drafts. Any project or extension name is left to whoever adopts it; no branding chosen.
* Done once, 24 October 2026: ESP32 sender to Direwolf receiver, full and primed frames (`docs/over-the-air-test.md`).
  Direwolf transmit to an ESP32 receiver also done the same day (5 of 5 beacons decoded; full 25 B, primed 8 B against 57 B plain). A T-Deck tracker was tested in both directions as well, and a station running our Direwolf build expanded its compressed beacon and passed it to APRS-IS as a normal packet one second later. Still to do: two ESP32 units, a weak-signal run with lost frames, and a mixed network with a stock unit.
* No pull request has been opened for this work. The branches are shown as an idea being tested, not as a request, and no pull request will be opened unless a maintainer says one would be welcome. The format is being discussed with F4MLV in issue 7 of moricef/LoRa_APRS_Tracker.
