# Truncated payloads and integrity checks

| | |
|---|---|
| Status | **Measured** on 14,298 real full frames (31 October 2026). A check is chosen for the byte plan (decode, re-encode, compare); it is **not yet written into a spec** and not tried on the air |
| Question from | F4MLV, issue #7 on moricef/LoRa_APRS_Tracker: a payload cut short before transmission is sent with a valid LoRa CRC, so we must verify that the decoder rejects it before deciding that no further check is needed |
| Depends on | [01 Compressed frames](../spec/01-compressed-frames.md), [08 Priming](../spec/08-priming-from-previous-packet.md) |
| Branch | `lacf` (LoRa APRS Compact Frame, LACF): the work on the byte plan for a native compact frame with this integrity rule |
| Code | [`tools/integrity/`](../tools/integrity/); `make integrity` runs the part that needs only this repository (fabricated packets) |

## Short version

1. **The decoder does not reject a truncated full frame.** Cut 1 to 6 bytes off the end of a full frame (`3C FF 81`) and it is accepted 100% of the
   time, with the right length and a wrong tail (the position fields at the front are usually intact). The LoRa CRC cannot help: it is computed after the cut.
2. **Appending a relay tail (RXT) breaks the current frame ending**, independently of truncation: with 6 bytes after the payload, only 83.8% of
   intact frames still decode to the right text.
3. **Chosen: the new ending, a CRC-16 of the text, and a receiver that re-encodes.** Decode, re-encode, compare (check C) rejected every cut frame in the test once the encoder uses a deterministic ending (+0.25 bytes on average). Check C alone still lets 1 in 208 to 1 in 2,286 damaged
   frames through; with a CRC-16 of the text (+2 bytes, the same CRC as the AX.25 frame check) **0 of about 2.85 million damaged frames were accepted**, which meets VHF APRS's 1 in 65,536. The cost is +2.25 bytes per full frame, identical encoders, and a frame mark that is provisional (same `3C FF 81` as model v1, so both ends must run the same build).
4. **Receiver time was the worry, and was fixed in software**: on a LILYGO LoRa32 v2.1 (ESP32-PICO-D4, 240 MHz) the unmodified reference code takes 16.2 ms to decode and 32.2 ms to decode with check C. A faster `build_freq` with byte-identical output, the key tables copied to RAM (35 KB), and a receiver that re-encodes while it decodes bring
   a full receive with check C to **6.3 ms**, and check C no longer costs anything measurable ([`results-esp32-timing.txt`](../tools/integrity/results-esp32-timing.txt)).

## What was tested

* **Packets.** 14,305 unique real LoRa APRS texts from two sources: a receiver log from an Arizona network, and an APRS-IS capture filtered on the LoRa destination codes
  (the per-source counts were not kept with the results). 14,298 of them compress to a full frame smaller than the plain frame (7 do not).
  The texts are **not** in this repository: they contain real callsigns and positions. See `tools/integrity/README.md` to build the same kind of file.
* **Targets and decoders.** The platforms this is meant for are the T-Deck tracker firmware and the ESP32 iGate firmware. The ESP32 iGate library (`lib/AprsCompress`, order-3 model, version 1) was built natively and tested;
  this repository's reference codec (`reference/aprs_ppm.c`) gives the same truncation numbers, and so does the C decoder in the author's Dire Wolf port, which is his test bench and not a deployment target.
* **Method.** Encode every text as a full frame, cut 1 to 16 bytes off the end (the cut frame still has a valid LoRa CRC because the sender made the cut), decode it,
  and classify the result: rejected, same text, a shorter text that is a prefix, or a different text. "Wrongly accepted" means the decoder returned text that is not the original.
  Intact frames decode back exactly in 14,298 of 14,298 cases.

## Results: the decoder as it is

**Full frames cut off the end** (n = 14,298 for cuts of 1 to 6 bytes; a few fewer for longer cuts, because short frames are skipped):

| Cut | Rejected | Wrongly accepted |
|---|---|---|
| 1 to 6 bytes | 0.00% | **100%** |
| 8 bytes | 0.04% | 99.96% |
| 12 bytes | 0.33% | 99.67% |
| 16 bytes | 0.87% | 99.13% |
| a random cut point | 0.00% | 100% |

The result has the right length and a wrong tail. For example, in one frame a tail `...|)_%H!a|` decoded as `...|)_%;X50`: the front of the packet is intact, the comment or telemetry at the end is wrong, and nothing marks it. (How much of the text is correct on average was not measured here.)

**Corruption that gets past the CRC** (full frames): 1, 2, 3 or 8 flipped bits decode to wrong text 99.3% to 99.98% of the time, and a payload replaced by random bytes 99.5%
to 99.7%. For 1 to 3 flipped bits a CRC-16 would normally catch the error, so these rows show the decoder's own rejection power, which is close to none for full frames.

**Primed frames** (they carry a one-byte CRC-8 text check, [spec 08](../spec/08-priming-from-previous-packet.md)), n = 14,298:

| Cut | Rejected | Wrongly accepted |
|---|---|---|
| 1 byte | 99.66% | 0.34% (about 1 in 295) |
| 2 bytes | 99.84% | 0.16% |
| 3 bytes | 99.99% | 0.007% (1 frame; too few to rely on) |

Bit flips: 97% to 99.7% rejected, 0.09% to 0.15% wrongly accepted. A payload of random bytes: 0 accepted in 14,298. An 8-bit check leaves about 1 in 256 undetected,
which is what the 1-byte cut shows (0.34% against 0.39%).

**A garbled full frame also poisons the next primed frames.** The ESP32 iGate stores whatever it decoded as the reference for that station. After a full frame accepted 2 bytes
short, the next intact primed frame from the station was dropped in 14,298 of 14,298 cases (no matching reference). 53 matched the reference check by chance, and the text check stopped
all 53, so no wrong text came out of that path. It is a safe failure, but it costs the following primed frames until the next full or plain frame.

## Results: a relay tail breaks the current ending

The encoder ends each frame with the fewest bytes it can and strips trailing zero bytes; the decoder reads zeros past the end of the payload. So an intact frame asks the decoder for
bytes beyond its end: **3 bytes beyond in 82.1% of frames, 4 in 17.8%, 5 in 0.06%**. If a relay appends RXT bytes, the decoder reads them instead of zeros.
**With 6 bytes appended, 11,978 of 14,298 intact frames (83.8%) decode to the right text and the other 16.2% do not decode to the right text.** So a relay tail needs a different ending
whatever else is decided.

A rule "reject if the decoder asked for more than 5 bytes beyond the end" costs nothing on the air and rejects no good frame, but catches only 7.4% of 1-byte cuts, 27.6% of 3-byte cuts,
57.4% of 8-byte cuts and 93.1% of 16-byte cuts. It is not enough on its own.

## The three checks, measured

All use an ending that does not depend on what follows the payload. Two such endings were measured against the current one (average full frame, 14,298 frames):

| Ending | Average frame | Change |
|---|---|---|
| Current (shortest, trailing zeros stripped) | 40.8 bytes | |
| Always write all 4 closing bytes | 44.0 bytes | **+3.2** |
| **The fewest closing bytes after which any following bytes still decode to the same text** | 41.0 bytes | **+0.3** |

The last one decoded correctly in 14,298 of 14,298 frames with nothing after them and 85,788 of 85,788 with 1 to 8 random bytes after them.

| Check | Extra bytes on air | Cut frames wrongly accepted (cuts of 1 to 16 bytes, no tail) |
|---|---|---|
| none (today) | 0 | 100% |
| **A** the decoder must end exactly at the payload's last byte | 0, but needs the 4-byte ending (+3.2) so the end is unambiguous | 0 of about 14,000 at every cut size |
| **B** CRC-8 (polynomial 0x1D) of the rebuilt text | 1 (+0.3 for the ending) | 0, 0.014, 0.035, 0.16, 0.30, 0.36, 0.40, 0.42% for cuts of 1, 2, 3, 4, 6, 8, 12, 16 bytes: about 1 in 256 |
| **C** decode, re-encode with the same rules, compare with the received payload | 0 (+0.3 for the ending) | **0 of 13,736 to 14,298 at every cut size** |

About half of the 1-byte cuts are accepted by B with the **correct** text, because the last closing byte is often not needed. That is harmless. A and C reject those frames
too, since a frame that is not the canonical encoding is damaged by definition.

With a 6-byte relay tail where the receiver has to find the payload end itself: cuts that only remove tail bytes leave the payload intact and A, B and C accept them with the right text; cuts into the payload
are rejected by A and C (100%) and by B 99.6% at 16 bytes (0.39% wrongly accepted). C also **finds the payload end** (the length the re-encoding comes to), which is what a relay needs to append RXT.

## Results: the new ending, written from the spec

[`lacf_ending.c`](../tools/integrity/lacf_ending.c) implements the ending, the CRC-16 and the receiver rule of [spec/10](../spec/10-lacf-ending-and-roundtrip-check.md) on the reference codec (`make lacf-ending`; real-text results in
[`results-lacf-ending-real.txt`](../tools/integrity/results-lacf-ending-real.txt)). On the 14,293 real frames that are smaller than plain with the layout (14,298 before the two CRC bytes were added): deterministic (a second encode gives the same bytes) in all; average frame 43.04 bytes against 40.79 (**+2.25**: 2 for the CRC-16, 0.25 for the ending);
385,911 of 385,911 decodes right with 0 to 8 bytes of random, 0x00 or 0xFF after the payload; minimal (drop the last byte and a 0x00 or 0xFF continuation fails) in all; intact frames accepted, with and without a 6-byte relay tail, with the payload end found correctly, in all.
Cut frames: 0 wrongly accepted at every cut size (1 to 16 bytes), with and without the tail, as in the earlier private-copy run. The one-pass receiver agrees with the two-pass one on every call (0 disagreements).

**Damage inside the frame is not zero with check C alone.** A damaged frame whose decoding happens to re-encode to itself is accepted. Wrong text accepted after the LoRa CRC has passed, 40 damaged copies of each of the 14,305 texts per row, with the shipping frame layout of
[spec/10](../spec/10-lacf-ending-and-roundtrip-check.md) ([`results-lacf-stress.txt`](../tools/integrity/results-lacf-stress.txt)):

| Damage | Trials | C alone | CRC-16 alone | **C + CRC-16** |
|---|---|---|---|---|
| 1 bit flipped | 571,720 | 2,747 (1 in 208) | 5 (1 in 113,000) | **0** |
| 2 bits flipped | 567,649 | 828 (1 in 685) | 4 (1 in 142,000) | **0** |
| 3 bits flipped | 571,720 | 498 (1 in 1,148) | 11 (1 in 52,000) | **0** |
| 8 bits flipped | 571,720 | 263 (1 in 2,173) | 11 (1 in 52,000) | **0** |
| payload replaced by random bytes | 571,720 | 250 (1 in 2,286) | 10 (1 in 57,000) | **0** |

These rows apply only after the LoRa CRC has passed, which is a small fraction of corrupted frames; the end-to-end rate was not computed. The CRC-16 alone behaves like a 16-bit check (about 1 in 65,536); the two checks together showed none in 2.85 million trials, and 0 of 2.85 million
means the rate is probably below about 1 in 950,000 (95% bound). The new ending makes C much stronger than the current ending's 1.3% to 6.2% for the same damage. Zero observed cut frames out of 14,298 means the rate is probably below about 1 in 4,800 (95% bound).

## What was chosen, and what it costs

**Chosen for the byte plan: check C plus a CRC-16 of the text**, with a deterministic encoder whose ending is the fewest closing bytes after which any following bytes still decode to the same text. On the air it costs about 2.25 bytes per full frame (2 for the CRC-16, 0.25 for the ending) and
it rejected every truncation and every damaged frame in the test.

**Check C only works with the new ending.** With the ending the codec uses today, C rejects almost nothing: re-encoding a cut frame's wrong text produces the cut bytes again as that text's own shortest ending. Measured with this repository's reference codec on the same 14,298 real texts
([`trunc_ref.c`](../tools/integrity/trunc_ref.c), [`results-ref-real.txt`](../tools/integrity/results-ref-real.txt)), the share of cut frames that C still accepts with wrong text is 85.2% (1 byte cut), 90.2% (2), 93.7% (3), 95.4% (4) and 97.1% (16).
For frames damaged inside, C helps with today's ending but leaves a residual: wrongly accepted 6.2% (1 bit flipped), 2.6% (2), 2.1% (3), 1.3% (8 bits) and 1.4% (random bytes), against 99.4% to 100% for the plain decoder. The deterministic ending (previous table) brings the cut-frame figure to 0 in every case tested; mid-frame
corruption under the new ending was **not** measured. So C and the new ending are one change, not two.

What it costs, plainly:

* **Receiver time** (LILYGO LoRa32 v2.1, ESP32-PICO-D4, 240 MHz, codec alone, no WiFi; 48 fabricated packets of 30 to 89 characters, 20 repetitions each; [`esp32-timing/`](../tools/integrity/esp32-timing/), [`results-esp32-timing.txt`](../tools/integrity/results-esp32-timing.txt)).
  With the unmodified reference code a plain decode takes **16.2 ms** (about 0.27 ms per character, in line with the independent tester's 12 to 29 ms in [known issues](known-issues.md)) and a decode with check C **32.2 ms**. Three changes that do not alter a single output byte
  (all checked against the reference at 2.39 million model positions, [`fast_freq_test.c`](../tools/integrity/fast_freq_test.c)) bring that down: a `build_freq` without 64-bit arithmetic and without a 256-entry scan per entry ([`fast_freq.h`](../tools/integrity/fast_freq.h)): 7.9 ms; copying the key tables (35 KB)
  to RAM, because the binary searches thrash the flash cache: 6.4 ms; and a receiver that re-encodes while it decodes, since both need the same probabilities for the same character: **a full receive with check C in 6.3 ms**, check C costing nothing measurable. Copying the whole model (127 KB) instead of the keys gave 5.5 ms for the plain decode.
  Two other tries were slower on this chip and were dropped (a float estimate with an integer correction, and a reciprocal multiply instead of the divide). On a desktop machine the same decode takes about 60 microseconds, so desktop figures do **not** predict the ESP32.
  All 20 conformance vectors came out of the board byte-identical to the host's, and the receiver accepted them. The T-Deck's processor was not measured, and 6.3 ms was measured without WiFi running.
* **Every encoder must produce identical bytes** for the same text and the same model, or its good frames are dropped by a receiver that applies C. The encoding rule (including the exact ending) therefore has to be
  written into the specification, and a second implementation has to match it.
* **A format change.** Frames from the builds that use the current ending would be dropped by a receiver that applies C with the new ending, and the two CRC-16 bytes change the header. The layout keeps the mark `3C FF 81` for now, so receivers built for spec 01 misread it and both ends must run the same build; the mark must change before anyone else runs it (spec/10 "Open").

## Not tested

* Cuts at the start of a frame, or inside a byte.
* The new ending is implemented in [`lacf_core.h`](../tools/integrity/lacf_core.h) on this repository's reference codec, written from [spec/10](../spec/10-lacf-ending-and-roundtrip-check.md), and in a private copy of the ESP32 iGate library ([`igate-library-private-copy.patch`](../tools/integrity/igate-library-private-copy.patch)).
  It is not in any firmware, and a second independent implementation has not been written.
* The T-Deck tracker firmware's own decoder (the tests ran on the codec library and the reference codec, not on the device).

## Open

* **Can check C replace the one-byte text check in primed frames? Measured: no.** 9,078 real packets that have an earlier packet from the same station were encoded as primed frames against the true reference (check C accepted all 9,078).
  Each was then decoded against about 11 wrong references (earlier packets of the station, and the true reference with 1 or 2 characters changed), 100,229 decodes in all, with the 8-bit reference check ignored so that every wrong reference got a chance
  ([`primed_c_test.c`](../tools/integrity/primed_c_test.c), [`results-primed-c.txt`](../tools/integrity/results-primed-c.txt)). 100,104 of them gave wrong text. How many passed each filter:

  | Filter on the decoded text | Wrong texts accepted (of 100,104) |
  |---|---|
  | station id byte (S) | 31,385 (31%) |
  | **text check byte (T)** | **397 (0.40%, about 1 in 256)** |
  | **re-encode and compare (C)** | **45,805 (46%)** |
  | S + T (today) | 129 (0.13%) |
  | S + C (the text byte replaced by C) | 13,433 (13%) |
  | S + T + C | 55 (0.055%) |

  A decode against a wrong reference is re-encoded with the same wrong reference, so the two cancel except in the closing bytes, and C catches fewer than half. It cannot take the place of the text check byte. Added on top of S and T it cuts the wrongly accepted
  texts by about 2.3 times (129 to 55), which is not worth 25 to 50 ms on an ESP32 for a case the reference check byte has already filtered 255 times in 256. Not tested: the deterministic ending (the test used the current ending, so it may change the 46%), and references
  that match the 8-bit reference check, which is the real-world case (a wrong reference reaches the decoder only 1 time in 256; these figures apply to those).
* A two-byte text check instead of C was weighed and **rejected in favour of both**: the CRC-16 meets the 1 in 65,536 target on its own, and C removes the damage that happens to re-encode to itself and supplies the payload end.
* How the relay tail (RXT) is delimited when C is not used.
