# Proposal draft 10 - LACF: a deterministic ending, a CRC-16 and a re-encode check

| | |
|---|---|
| Status | **Draft for the LoRa APRS Compact Frame (LACF) byte plan. Not a standard. Implemented only in the test code in this repository (`tools/integrity/`) and in a timing sketch for an ESP32; not in any firmware. Frame mark not decided (see Open).** |
| Depends on | 01 Compressed frames (the codec and model v1 are unchanged) |
| Replaces | the ending ("Finishing") and the missing integrity check of 01 for frames that carry this layout |
| Evidence | `docs/truncated-payloads-and-integrity.md` (why, and every number), `tools/integrity/lacf_core.h` (the rule as code), `test/vectors_lacf_v1.txt` |

## Why

A payload cut short before it is sent has a valid LoRa CRC, because the sender computes the CRC after the cut. The decoder of spec 01 accepts such a frame
every time with a wrong tail, and a relay tail (RXT) appended after the payload changes what that decoder reads. This draft makes the end of the payload unambiguous,
adds a CRC-16 of the text (the same CRC as the AX.25 frame check, so VHF APRS's 1 in 65,536 is met or beaten), and makes the receiver prove that the payload is the one an encoder would have sent.

## Frame

    offset  size  meaning
    0       1     0x3C
    1       1     0xFF
    2       1     frame mark (open; the tests use 0x81, the mark of model v1)
    3       1     L = length in bytes of the original text, 1..255
    4       2     CRC-16 of the L text bytes, low byte first (CRC-16/X-25: polynomial 0x1021 reflected, init 0xFFFF, xorout 0xFFFF; check value of "123456789" is 0x906E)
    6       n     range-coded payload, n >= 0

A sender MAY send this frame only when it is strictly shorter than the plain frame (`6 + n < 3 + L`). The model, the range coder and every probability are those of spec 01;
only the ending below and the two header bytes differ.

## Ending (normative)

After the last text byte is encoded, the encoder holds `low` (up to 33 bits, with a carry pending in bit 32) and `range`. Then:

1. If `low >= 2^32`: add 1 to the payload so far (propagating the carry through the bytes already written) and set `low = low - 2^32`.
2. Let `hi = low + range - 1`.
3. For `nb = 0, 1, 2, 3, 4`: let `blk = 2^(8*(4-nb))` and `v = ceil(low / blk) * blk`. If `v + blk - 1 <= hi`, stop.
   (`blk` values are the 32-bit values a decoder could see if it read `nb` bytes and then anything at all.)
4. If `v >= 2^32`: add 1 to the payload so far and set `v = v - 2^32`.
5. Append the top `nb` bytes of `v` (most significant first).
6. **Do not strip trailing zero bytes.** A zero last byte is sent.

`nb = 4` always succeeds (`blk = 1`, `v = low`). The result is the fewest bytes after which **any** following bytes decode to the same text: a receiver that
reads past the payload into a relay tail, random bytes or nothing gets the same text. Spec 01's ending (the fewest bytes assuming zeros follow, then stripping zeros)
is replaced; on 14,293 real frames the new ending is on average 0.25 bytes longer.

The encoder is deterministic: the same text always gives the same bytes, and a second implementation MUST produce them exactly (the receiver check below depends on it).

## Receiver (normative)

Given the received bytes after the LoRa CRC has passed:

1. Check `3C FF <mark>`; drop the frame if the third byte is unknown, and never show it as text (spec 01).
2. Decode `L` text bytes with the range decoder from byte 6 onward. Bytes beyond the end of the frame count as zero (as in spec 01).
3. Compute the CRC-16 of the decoded text; if it differs from bytes 4 to 5, **drop the frame**.
4. Re-encode the decoded text with the sender's rules (header, CRC, ending) and call the result `E` (`6 + m` bytes). If `E`'s payload is not the first `m` bytes of the received payload, **drop the frame**.
5. The payload is exactly those `m` bytes. Any bytes after them are a relay tail. A receiver that does not allow a tail drops the frame if there are any.
6. Only an accepted frame may be shown, gated, forwarded, or kept as a reference for priming (spec 08).

Steps 2 and 4 use the same probabilities for the same character, so a receiver MAY re-encode while it decodes (one pass over the model instead of two). It must give the same
accept and drop answers as the two-pass form; `tools/integrity/lacf_ending.c` tests both against each other on every case it runs.

## Conformance

`test/vectors_lacf_v1.txt` holds frames made from fabricated packets (format as `vectors_v1.txt`). An implementation conforms if `encode(text) == frame` for every vector (declining
where it shows `-`) and the receiver accepts each frame and returns the text. `make lacf-ending` checks the reference code against the vectors and runs the property tests.

## Measured (see the doc for the tables)

* Cut frames, 1 to 16 bytes, with and without a 6-byte relay tail: 0 wrongly accepted of 13,731 to 14,293 at every size, 14,305 real texts.
* Damage inside the frame after the LoRa CRC has passed (1, 2, 3 and 8 flipped bits, random payload), about 2.85 million trials: 0 wrongly accepted with the CRC-16 and the re-encode check together.
  The re-encode check alone leaves 1 in 208 to 1 in 2,286; the CRC-16 alone leaves about 1 in 52,000 to 1 in 142,000.
* Cost: +2.25 bytes per full frame on average (+2 for the CRC-16, +0.25 for the ending).
* Receiver time on an ESP32-PICO-D4 (LILYGO LoRa32 v2.1): 6.3 ms per received frame with a one-pass receiver and the key tables copied to RAM (32.2 ms with the unmodified reference code and two passes).

## Open

* **The frame mark.** Options: a new model version number with the same table (the existing "unknown version: drop" rule then protects old receivers), or a flag bit if one is freed. Not decided; the tests use 0x81, which old receivers would misread, so
  this layout must not go on the air under that mark.
* Primed frames (spec 08) keep their own one-byte text check; the re-encode check was measured on them and is much weaker there (docs/truncated-payloads-and-integrity.md).
* A second, independent implementation of the encoder and the receiver (the check needs byte-identical encoders).
* The T-Deck's processor was not timed.
