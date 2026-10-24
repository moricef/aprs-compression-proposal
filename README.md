# Proposals for extending APRS: compressed frames and capability announcement

**Phase 1 of bringing APRS up to date:** proposal drafts for carrying the same APRS packets in fewer
bytes (APRS-PPM compressed frames), a way for stations to learn what the stations around them can
decode, and a binding for VHF/AX.25. It extends APRS; it does not replace it, and legacy stations
keep working. Phase 2, the harder "smarts" work (smarter digipeating, path learning, reach-aware
repeating, a measured pilot), is in the companion repo `aprs-smarts`.

**This is a draft by one person. It is not a standard, and the on-air format has not been tested over
the air.** Licensed under the [MIT License](LICENSE).

## Results so far (simulation on captured packets, nothing on air yet)

**The test data.** About an hour of read-only APRS-IS captures (plus a 5-minute pilot) for about 25
countries, taken on an evening in Europe: 57,883 unique packets, of which 4,656 LoRa and 12,988 VHF
packets were held out for testing. LoRa packets were chosen by device type, so they come mostly from
Europe; VHF packets are those an iGate heard over the air. The model is trained on 70% of stations and
tested on the other 30%, so every figure is on stations the model has not seen. The raw captures hold
real callsigns and are not published, only these aggregates.

**What each saves** (average frame size against today's plain frame, 127 KB table; airtime saved is
smaller because the preamble does not shrink). Full table and caveats in
[docs/proposed-implementations.md](docs/proposed-implementations.md).

| Implementation | LoRa APRS | Traditional VHF APRS |
|---|---|---|
| Static compressed frame (spec 01): fixed trained table + range coder | **43%** bytes, about 34% airtime | whole packet in a new frame type **41%**, about 25% airtime |
| Same, but keeping the AX.25 header and compressing only the info field (spec 07) | n/a | **21%**, about 13% airtime |
| Priming each packet with the station's previous packet (spec 08) | **66%** overall, about 52% airtime | **62%** overall, about 38% airtime |
| Priming, with a **full frame every 2nd transmission: proposed default for moving stations** | 56% (all LoRa stations) | **56%** (moving stations); about 86% of receivers can decode, 65% at the edge of range |
| Priming, with a full frame every 4th transmission: suggested for stationary stations | 62% | 60% (63% for stationary stations) |

**What it means.**
* LoRa APRS carries the whole packet, so compression helps most there. Traditional VHF APRS spends
  about 38% of each frame on the AX.25 addresses, which legacy digipeaters have to read, so
  compressing only the info field saves about a fifth. Using a LoRa-style frame on VHF brings it to
  about 41%, at the cost of legacy digipeaters not repeating it. See spec 07.
* Priming helps the most, because stations repeat themselves: same comment, symbol and path. It needs
  the receiver to have heard the reference packet. For moving stations that is often not the case at
  the edge of range; the data for that is weak (APRS-IS drops duplicates). See spec 08.
* More packets of the same kind did not improve the saving at a fixed table size. The limit is now
  the table format and the model.
* How often to send a full frame is a trade between saving and being decodable by receivers that missed the previous packet; the proposed default is every 2nd transmission for moving stations and every 4th to 8th for stationary ones (spec 08).
* The figures do not add up, because the methods overlap.
* **None of this has been tested over the air.** Simulation uses ideal coding length for the priming
  figures, an assumed TXDELAY for VHF airtime, and one hour of data.

See [docs/per-country-estimate.md](docs/per-country-estimate.md) for the result by country and device
type, and [docs/legacy-decoder-test.md](docs/legacy-decoder-test.md) for what a stock Direwolf does with
the new VHF frames.

## Parts

| Part | Status |
|---|---|
| [01 Compressed frames (APRS-PPM)](spec/01-compressed-frames.md) | Spec + C reference codec + frozen model v1 + test vectors. Host-tested only |
| [02 Capability announcement](spec/02-capability-announcement.md) | Proposal draft: how a sender learns a station can decode compressed frames |
| [07 AX.25 binding (VHF)](spec/07-ax25-binding.md) | Proposal draft: carry the same compressed payload in AX.25 info fields; smaller gain, marker undecided |
| [08 Priming with a station's previous packet](spec/08-priming-from-previous-packet.md) | Idea + simulation: about 62-66% saved on VHF whole-packet frames, against 41% for the static table |
| [09 Structured header for position packets](spec/09-structured-header-layout.md) | Idea: a compact fixed layout for callsign, path and position plus an order-3 comment; about 4-7 B smaller than order-3 alone on LoRa position packets (arithmetic) |
| [Direwolf notes](docs/direwolf-notes.md) | Planning notes for a Direwolf implementation |
| [Other methods considered, APRS 438, and why APRS-PPM was chosen](docs/alternatives-considered.md) | Comparison table and the reasoning |
| [How the compression works and how it was trained](docs/how-it-works.md) | Plain-language explanation, comparison with other methods, training method and caveats |
| [Compression by country (rough estimate)](docs/per-country-estimate.md) | Estimate per country and device type, LoRa and VHF, from one hour of capture; to be replaced by the 8-hour run |
| [Legacy decoder test (no radio)](docs/legacy-decoder-test.md) | What stock Direwolf does with compressed VHF frames, using generated audio |
| [Proposed implementations and how much they cut](docs/proposed-implementations.md) | One table: LoRa and VHF, bytes and airtime, with status |
| [Rationale and measurements](docs/rationale.md) | Numbers behind the above |

## Layout

    spec/        the drafts
    reference/   plain-C codec (integer-only, C99)
    model/       frozen model v1 table + SHA-256
    tools/       train_model.py (retrain), capture_aprsis.py (read-only APRS-IS capture)
    test/        test vectors (fabricated packets) and the tool that checks them
    docs/        measurements, notes

## Try it

    make test                     # checks the reference codec against test/vectors_v1.txt
    build/ppm_tool stats < packets.txt

## Implementations

* Firmware fork of richonguzman/LoRa_APRS_iGate with receive-always, opt-in transmit
  (`lora.compressTx`): local branch `feature/order3-compression`, builds for `ttgo-lora32-v21`.
  Its C++ codec produces byte-identical frames to the reference on 4,520 vectors.
* Nothing else yet. A Direwolf port and an STM32WLE5 tracker build are candidates.

## TODO

* Reframed as proposal drafts. Any project or extension name is left to whoever adopts it; no branding chosen.
* Test over the air on two radios.
* Once it works over the air, bring the proposal to the APRS Foundation (contact form / aprssig) and open a PR to richonguzman/LoRa_APRS_iGate that links here for the format details.
