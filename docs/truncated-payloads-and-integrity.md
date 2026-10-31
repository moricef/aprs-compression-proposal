# Truncated payloads and integrity checks

| | |
|---|---|
| Status | **Measured** on 14,298 real full frames (31 October 2026). A check is chosen for the byte plan (decode, re-encode, compare); it is **not yet written into a spec** and not tried on the air |
| Question from | F4MLV, issue #7 on moricef/LoRa_APRS_Tracker: a payload cut short before transmission is sent with a valid LoRa CRC, so we must verify that the decoder rejects it before deciding that no further check is needed |
| Depends on | [01 Compressed frames](../spec/01-compressed-frames.md), [08 Priming](../spec/08-priming-from-previous-packet.md) |
| Branch | `lacf` (LoRa APRS Compact Frame, LACF): the work on the byte plan for a native compact frame with this integrity rule |
| Code | [`tools/integrity/`](../tools/integrity/) |

## Short version

1. **The decoder does not reject a truncated full frame.** Cut 1 to 6 bytes off the end of a full frame (`3C FF 81`) and it is accepted 100% of the
   time, with the right length and a wrong tail. The LoRa CRC cannot help: it is computed after the cut.
2. **Appending a relay tail (RXT) breaks the current frame ending**, independently of truncation: with 6 bytes after the payload, only 83.8% of
   intact frames still decode to the right text.
3. **Three checks were measured.** Decode, re-encode, compare (check C) rejected every cut frame in the test, with 0 extra bytes on the air once the
   encoder uses a deterministic ending that costs +0.3 bytes on average. It costs receiver time and requires every encoder to produce identical bytes.
4. **The receiver time is the real price**: 25 to 50 ms per received full frame on an ESP32 (a measurement from an independent tester, see
   [known issues](known-issues.md)), not the few milliseconds a first estimate from a desktop machine suggested.

## What was tested

* **Packets.** 14,305 unique real LoRa APRS texts: 5,188 heard on the radio in an Arizona network over a day, and the packets of a 25-minute APRS-IS capture
  filtered on the LoRa destination codes (11,647 packets worldwide). 14,298 of them compress to a full frame smaller than the plain frame (7 do not).
  The texts are **not** in this repository: they contain real callsigns and positions. See `tools/integrity/README.md` to build the same kind of file.
* **Two decoders, the same algorithm.** The ESP32 iGate firmware's library (`lib/AprsCompress`, order-3 model, version 1) built natively, and the Dire Wolf
  C decoder (release 2.0.0). They gave the same numbers wherever both were run.
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

The result has the right length with the first 97% of the text correct and a wrong tail. For example a tail `...|)_%H!a|` decoded as `...|)_%;X50`. The position fields at the
front are intact; the comment or telemetry at the end is wrong, and nothing marks it.

**Corruption that gets past the CRC** (full frames): 1, 2, 3 or 8 flipped bits decode to wrong text 99.3% to 99.98% of the time, and a payload replaced by random bytes 99.5%
to 99.7%. For 1 to 3 flipped bits a CRC-16 would normally catch the error, so these rows show the decoder's own rejection power, which is close to none for full frames.

**Primed frames** (they carry a one-byte CRC-8 text check, [spec 08](../spec/08-priming-from-previous-packet.md)), n = 14,298:

| Cut | Rejected | Wrongly accepted |
|---|---|---|
| 1 byte | 99.66% | 0.34% (about 1 in 295) |
| 2 bytes | 99.84% | 0.16% |
| 3 bytes | 99.99% | 0.007% (1 frame; too few to rely on) |

Bit flips: 97% to 99.7% rejected, 0.09% to 0.13% wrongly accepted. A payload of random bytes: 0 accepted in 14,298. An 8-bit check leaves about 1 in 256 undetected,
which is what the 1-byte cut shows (0.34% against 0.39%).

**A garbled full frame also poisons the next primed frames.** The ESP32 iGate stores whatever it decoded as the reference for that station. After a full frame accepted 2 bytes
short, the next intact primed frame from the station was dropped in 14,298 of 14,298 cases (no matching reference). 53 matched the reference check by chance, and the text check stopped
all 53, so no wrong text came out of that path. It is a safe failure, but it costs the following primed frames until the next full or plain frame.

## Results: a relay tail breaks the current ending

The encoder ends each frame with the fewest bytes it can and strips trailing zero bytes; the decoder reads zeros past the end of the payload. So an intact frame asks the decoder for
bytes beyond its end: **3 bytes beyond in 82.1% of frames, 4 in 17.8%, 5 in 0.06%**. If a relay appends RXT bytes, the decoder reads them instead of zeros.
**With 6 bytes appended, 11,978 of 14,298 intact frames (83.8%) decode to the right text and the other 16.2% silently decode to wrong text.** So a relay tail needs a different ending
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

## What was chosen, and what it costs

**Chosen for the byte plan: check C**, with a deterministic encoder whose ending is the fewest closing bytes after which any following bytes still decode to the same text. On the air it costs about 0.3 bytes per frame and
it rejected every truncation in the test.

What it costs, plainly:

* **Receiver time.** The independent tester measured one decode on an ESP32-PICO-D4 (240 MHz, codec alone) at 12 to 29 ms (0.3 ms per character), so a round trip is about **25 to 50 ms per received full frame**
  ([known issues](known-issues.md)). On a desktop machine the same decode takes 59 to 73 microseconds, and the re-encode adds 57 to 72: the board is 200 to 400 times slower, so desktop figures do **not** predict
  the ESP32. 25 to 50 ms is small against the seconds between frames at SF12 and not small at SF7.
* **Every encoder must produce identical bytes** for the same text and the same model, or its good frames are dropped by a receiver that applies C. The encoding rule (including the exact ending) therefore has to be
  written into the specification, and a second implementation has to match it.
* **A format change.** Frames from the builds that use the current ending would be dropped by a receiver that applies C with the new ending. The new ending needs its own frame mark or model version.

## Not tested

* Corruption in the middle of a frame under check C (only the plain decoder and check B were measured with bit flips).
* Cuts at the start of a frame, or inside a byte.
* The deterministic ending and check C in the Dire Wolf C decoder (the full-frame truncation test was run on both decoders; the ending experiments only on the iGate library, in a private copy: [`igate-library-private-copy.patch`](../tools/integrity/igate-library-private-copy.patch)).
* Check C with a time measurement on an ESP32 (the 25 to 50 ms are the tester's figures for the current decode and encode).
* The Tracker firmware's own decoder.

## Open

* **Can check C replace the one-byte text check in primed frames? Measured: no.** 9,078 real packets that have an earlier packet from the same station were encoded as primed frames against the true reference (check C accepted all 9,078).
  Each was then decoded against 11 wrong references (earlier packets of the station, and the true reference with 1 or 2 characters changed), 100,229 decodes in all, with the 8-bit reference check ignored so that every wrong reference got a chance
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
* Whether a two-byte text check, instead of C, is the better trade (one more byte on every frame, no receiver time, about 1 in 65,536 left undetected).
* How the relay tail (RXT) is delimited when C is not used.
