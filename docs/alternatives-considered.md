# Other compression methods considered, APRS 438, and why APRS-PPM was chosen

This page answers three questions: what else could compress APRS packets, how does the best-known
APRS-specific scheme (APRS 438) compare, and why the design in this repo is what it is. Measured
figures come from the captured APRS-IS packets described in `docs/how-it-works.md`; anything not
measured is marked. Nothing here has been tested over the air.

## What the compressor has to do

APRS packets are short (about 40 to 100 bytes) and varied (positions, weather, messages, objects,
status, telemetry, Mic-E). They are sent unprompted, so the receiver cannot ask for a retransmit or
negotiate. The code runs on small microcontrollers with perhaps 100 to 150 KB of flash to spare, must
produce the same bytes on every device, and must leave plain APRS working for stations that do not
implement it. That rules out a lot of methods before any numbers.

## The methods

| Method | How it works | Result here | Why not (or where it fits) |
|---|---|---|---|
| zlib / deflate, LZMA, bzip2 | Adaptive: learn repeats from the data as it goes | About nothing; raw deflate about 2% at best; headers can make packets bigger (measured) | A 70-byte packet is over before the compressor has anything to learn from |
| Deflate with a trained preset dictionary | As above, but start with a dictionary of typical APRS text | 37% (measured) | Better, but its matching is a poor fit for the byte-by-byte statistics of packets, and it is less accurate than a context model at the same size |
| Static Huffman, one table for all bytes | Short codes for common bytes | 26% (measured) | Uses only single-byte frequency; ignores what came before |
| Short-string compressors (SMAZ / SMAZ2) | Built-in dictionary of common words for short text | Not measured | Designed for English-like text, not callsigns, digits and APRS syntax; a dictionary made for APRS would be needed, which is a hand-built version of what training gives |
| Fixed per-character codes (PSK31 Varicode, PACTOR Huffman) | A fixed code per character | Not measured; expected near static Huffman | Same limit as static Huffman |
| Old modem compressors (V.42bis, MNP5) | Adaptive dictionary | Not measured; adaptive, so expected to do poorly on short packets | Same reason as zlib |
| Larger or neural models (context mixing, language models) | Many models blended, or a trained network | Not tried | Far too big and slow for the target microcontrollers, and hard to make bit-exact across devices |
| **Trained context model + range coder (this repo)** | A fixed table of "what usually follows these 3 characters", coded with an arithmetic coder | **41% on LoRa, 20% on VHF (info field only), 39% on VHF as a whole-packet frame** at a 144 KB table; 43% and 22% at 202 KB | Chosen; see below |
| APRS's own compact formats (base-91 compressed position, Mic-E) | Part of the APRS 1.0.1 specification: position in fewer characters | Already used by many trackers (see below) | Not an alternative: this repo's compression works on top of them |
| APRS 438 | A hand-designed fixed 17-byte binary layout for a position beacon | See the comparison below | Different scope: position only, no comment |
| Native LoRa APRS frame (moricef, draft 0) | A new binary layout: typed fields, packed callsigns, binary position, a fixed Huffman code for the text, optional receiver-quality data | 27% smaller than plain (measured, see below) | Different aim (clean fields, receiver data, exact translation to APRS-IS); about half the saving on ordinary beacons. The two can coexist |

The order of the context model was chosen by measurement, on an early LoRa-only corpus of about
1,400 packets: order 1 saves 44%, order 2 saves 55%, order 3 saves 58% (ideal cost), order 4 saves
57%. Order 3 is where the return runs out (`docs/how-it-works.md`).

## APRS compressed position and Mic-E

APRS already has two ways to shrink a position. The base-91 compressed position writes latitude and
longitude in 13 characters instead of 19 or so. Mic-E packs the position into the destination field.
They are a different thing from this proposal and they combine with it. In the captured data, 99% of
the packets from one common ESP32 LoRa tracker already use the base-91 position and a short comment
(5 bytes on average), and the table still saves about 29% on them (`docs/per-country-estimate.md`).
Mic-E encoding would save only a few bytes more than base-91 on a LoRa packet (Direwolf's notes).

## APRS 438

APRS 438 (ON4AA, documented at https://aprs438.readthedocs.io) takes the opposite route: a hand-designed
binary layout of 17 to 19 bytes with 4 bytes of callsign, 1 byte of SSID/path/type and a compressed
position. It has no comment field. At the time of the earlier research it was described as a tracker
release on an existing iGate with the packed-frame format not yet released, and as needing its own
frequencies, SF11 and its own iGate; I have not re-checked that since.

On 555 RF-heard VHF position packets (an earlier, smaller model; the 438 sizes are from the documented
layout, not from a real encoder):

| Method | Mean bytes | Saved | Comment kept? |
|---|---|---|---|
| Plain text (+3 B header) | 80.7 | – | yes (baseline) |
| **Like for like: the whole packet is carried** | | | |
| Order-3 model, whole packet | 33.6 | 58% | yes |
| APRS 438 plus the comment as a second frame | 38.2 | 53% | yes |
| 438 header and position, comment compressed with the order-3 model (estimate, not built) | 32.8 | 59% | yes |
| **Not like for like: part of the packet is thrown away** | | | |
| APRS 438, position only (comment dropped) | 17.0 | 79% | **no**: 98% of these packets carry one, mean 33.5 characters |

The 79% row is smaller only because it drops the comment, which is about a third of the packet. It is
not a comparison of the two methods. Compare the rows above it.

| | APRS 438 | APRS-PPM (this repo) |
|---|---|---|
| Smallest packet | 17 B, position only | About 24 B average overall with priming (LoRa, 66%); more for a packet with an unusual comment |
| Comment | None; 98% of position packets carry one (mean 33.5 characters), mostly longer than 438's 28-character limit | Carried, at whatever size it costs |
| Packet types | Position | Any packet: messages, weather, objects, telemetry, Mic-E |
| VHF paths | Four path codes; only 44% of the VHF paths seen fit | Any path |
| Adding a field later | A new revision of the layout | Retrain the table (a new model version) |
| Needs | New frequencies, its own SF and iGate | Nothing new on air for plain frames; upgraded receivers |
| Legacy APRS | Separate network | Plain frames unchanged; compressed frames ignored by legacy |

438 looks best only in the row that drops the comment. With the comment kept, the model alone is
smaller than 438 plus the comment as a second frame (33.6 against 38.2 bytes), and a 438 header and
position with the comment compressed with the order-3 model comes out about the same (32.8 bytes, an estimate). For a pure position
beacon with no comment, 438 is smaller than anything here (in one earlier single-beacon test, about
17 against 21 bytes for the model). It is not a general replacement for APRS packets, and it forks the network. The two are not
exclusive: the like-for-like table shows a 438-style header and position with the comment compressed with the order-3 model.

## The native LoRa APRS frame (moricef, draft 0)

Another effort covers some of the same ground. moricef (F4MLV, France) has built a **native frame** for LoRa APRS,
documented as a draft (`docs/LORA_APRS_NATIVE_FORMAT.md` and `..._DRAFT0.md` in his `LoRa_APRS_iGate` repository,
branch `rxt`) and already in his T-Deck tracker (`devel` branch, setting `txFormat`: text, native or both). It marks
its frames `3C FF 02`, so it cannot be confused with a plain frame (`01`) or an APRS-PPM frame (`81`, `C1`).

**What it is.** Each packet type gets its own binary layout: callsigns packed into a few bytes, the path as short
codes (`WIDEn-N`), the position as numbers, and a fixed Huffman code for the free text. The iGate translates every
frame back to ordinary APRS text for APRS-IS, deterministically, so nothing downstream changes. The frame can also
carry the receiver's signal measurements (RSSI, SNR) for network maps. His stated aims are less ambiguity in the
data and a measurable network, as much as shorter frames.

**What it measured, from his published report** (a T-Deck on a desk, 117 minutes of native beacons, three receivers):
native frames were received as well as text frames, and a Mic-E beacon went from 47 to 29 bytes.

**What the two give on the same packets.** I ran his encoder (copied unchanged from his tracker's `devel` branch) and
the APRS-PPM codec on the same packets, with the plain frame (text plus the 3-byte header) as the baseline. Every
native frame decoded back to the original packet; none failed to encode. The program is in `tools/compare_native/`.

| Packets | Count | Plain | Native | APRS-PPM, one frame | APRS-PPM, full then primed |
|---|---|---|---|---|---|
| Live LoRa APRS traffic captured on 24 October 2026 (after both formats were built) | 1,037 | 70.7 B | 51.8 B (27% smaller) | 33.2 B (53%) | 32.1 B (55%) |
| The three-hour capture of the earlier tests (the model's training data may overlap with it) | 27,276 | 69.2 B | 50.4 B (27%) | 32.1 B (54%) | 24.4 B (65%) |
| Mic-E packets from a VHF capture | 1,090 | 61.1 B | 44.5 B (27%) | 49.7 B (19%) | 40.7 B (33%) |

* On ordinary LoRa beacons (positions, messages, status) APRS-PPM's saving is about twice the native frame's.
* On Mic-E, which is already a compact layout, the native frame is a little smaller than one APRS-PPM frame; with
  priming APRS-PPM is smaller again.
* The primed column assumes the receiver heard the station's previous frame. In the live sample only 13% of packets
  had an earlier packet from the same station within the 20-minute window, so priming adds little there; in the
  three-hour sample 79% did.
* These figures leave out what the native frame adds beyond size (the optional receiver data, the strict grammar).
  Draft 0 may change, and its Huffman table was built for his network's traffic.
* My earlier "about 41% on LoRa" figure above came from an older, smaller model and a held-out-station test; the 53%
  here is the shipped model on newer traffic. I have not reconciled the two, so quote the live figure with that in mind.

**Where it leaves the two.** They solve different problems and can run side by side in one firmware (a T-Deck firmware
carrying both has been built; it has not been run on the air yet). A natural combination is a native frame for structure and receiver data, with
APRS-PPM for what is left as text; that has not been tried.

## Why APRS-PPM was chosen

1. **It works on 70-byte packets.** Because the statistics are learned in advance, it needs nothing
   from the packet itself, unlike zlib and the other adaptive coders.
2. **It handles every packet type,** not one layout. Weather, messages and Mic-E fit without a new
   format each time.
3. **It uses the structure of the text.** Of the methods that fit in flash, a context model captures
   most of the predictability (order 3 measured best); the others capture less.
4. **It fits and is exact.** About 125 to 130 KB of table and integer-only arithmetic, so every device
   computes the same bytes (checked against a C reference on 4,520 vectors).
5. **It cannot fail to encode.** Every byte keeps a nonzero probability, so any packet can be coded; if
   the result is not smaller, the plain frame is sent instead.
6. **Plain APRS keeps working.** The new frame has its own header; nothing existing changes.
7. **It can improve.** The model is versioned; a better one (more data, field-specific models,
   priming from a station's earlier packet, `spec/08`) can follow without breaking v1.

## Limits of this comparison

* The context-model figures for orders 1 to 4 come from an early, small corpus. Current figures are
  lower (about 41% on LoRa at a 144 KB table), and the corpus is a three-hour evening capture in Europe.
* The 438 comparison used an earlier, smaller model and the documented 438 layout, not a 438 encoder.
* SMAZ/SMAZ2, Varicode, PACTOR and V.42bis were looked at but not run on the packets. Their rows are
  judgements, not measurements.
* The native-frame comparison used the draft-0 encoder as copied into his tracker's `devel` branch; if his format changes,
  rerun `tools/compare_native/`. Neither format was tuned on the live sample.
* A better approach may exist. I found no APRS-specific scheme using a trained context model, which
  is a statement about what I could find.

## References

* APRS 1.0.1 specification (base-91 compressed position, Mic-E): a copy at
  https://op-co.de/tmp/APRS101/APRS.html
* APRS 438: https://aprs438.readthedocs.io
* Cleary and Witten, "Data compression using adaptive coding and partial string matching", 1984, and
  Moffat, "Implementing the PPM data compression scheme", 1990 (the escape method used, method C).
* Range coding: G. N. N. Martin, 1979.
* The model, trainer and codec: `spec/01-compressed-frames.md`, `tools/train_model.py`, `reference/`.
