# Proposal draft 01 - Compressed frames for APRS (APRS-PPM)

| | |
|---|---|
| Status | **Proposal draft by an individual. Not a standard, not endorsed by any organization.** |
| Author | Walter Dunckel, K6ATV |
| Targets | APRS over amateur-radio LoRa (this part); AX.25 info field (see 07-ax25-binding.md) |
| Changes to existing APRS | None. Additive: a new frame header that existing stations ignore |
| Implementation | Reference C codec, one firmware fork, host-tested. **Not tested over the air yet** |
| Name | "APRS-PPM" is a working name for this proposal only; the name of an adopted extension is for the standards body to decide |

## Summary

An optional way to send the same APRS text packet in about half the bytes on a LoRa link, using a
fixed, versioned, publicly documented statistical model. Stations that do not implement it never
see a compressed frame as valid APRS and are unaffected.

## Motivation

At SF12 a 57-byte beacon takes about 2.6 s on the air. Airtime, not bandwidth, is what limits a
shared LoRa APRS channel, and the packets are small, so general-purpose compressors do not help.
See docs/how-it-works.md for the measurements and the comparison with other methods.

This part defines the codec and the **LoRa binding**. The same codec and model carry over to VHF APRS
through an AX.25 binding: see 07-ax25-binding.md.

## What it is

A way to send an ordinary APRS text packet in roughly half the bytes on a LoRa link. It is not a
new idea: it is prediction by partial matching (PPM, an order-3 context model with "method C"
escape weights) feeding a range coder. The one design choice that matters for 40-100 byte packets
is that the context statistics are trained ahead of time on real APRS traffic and shipped as a
fixed table, so there is nothing to learn from the packet itself. Adaptive compressors (zlib,
LZMA) have nothing to learn from a packet this short and expand it.

## On-air frame

    offset  size  meaning
    0       1     0x3C
    1       1     0xFF
    2       1     0x80 | model version (1..127). Model v1 -> 0x81
    3       1     L = length in bytes of the original text, 1..255
    4       n     range-coded bytes (n may be 0)

"Text" is the packet as it appears after the 3-byte plain LoRa APRS header, i.e. the TNC2 form
`SRC>DEST,PATH:info`. A plain LoRa APRS frame stays `3C FF 01` + text; this format never changes it.

A sender MAY send a compressed frame only when it is strictly shorter than the plain frame
(`4 + n < 3 + L`). Otherwise it sends the plain frame.

Receivers that do not implement this format check for exactly `3C FF 01` and drop the frame, so
compressed frames are invisible to them. The consequence is that a station sending compressed
frames is not heard by stations that do not decode them (see 02-capability-announcement.md).

## The model (normative)

* The model is the table in `model/aprs_ppm_model_v1.c`, identified by `model/MODEL-v1.sha256`.
  **It is frozen.** Any change, including retraining, gets a new version number.
* A decoder holding a different table than the encoder decodes garbage without noticing, so a
  receiver MUST only decode versions it holds the exact table for.
* Tables for orders 0..3. Each is a sorted list of contexts (the preceding 0..3 bytes of the
  same packet, packed big-endian into a 3-byte key), each with a list of (byte, count) pairs.
  Counts are 8-bit; a context's total `n` is the sum of its stored counts and `u` is the number of
  stored pairs.

## Prediction (normative)

Position `i` in the text is predicted from the bytes before it in the same packet. Using fewer
than 3 bytes of history at the start of a packet is allowed (`k = min(3, i)`).

All arithmetic is integer. All values are unsigned.

    rem = 0xFFFFFFFF                     (64-bit)
    pr[0..255] = 0                       (32-bit)
    for k = min(3, i) down to 0, while rem > 0:
        find the context for the last k bytes in the order-k table; if absent, continue
        denom = n + u
        for each (byte b, count c) in that context:
            pr[b] += floor(rem * c / denom)
        rem = floor(rem * u / denom)
    floor_share = rem >> 8
    for each byte value s in 0..255:
        freq[s] = 1 + floor( (pr[s] + floor_share) * (65536 - 256) / 2^32 )
    total = sum(freq[0..255])            (always <= 65536)

The `+ 1` guarantees every byte value is encodable, including bytes the model never saw.

## Range coder (normative)

32-bit range, 64-bit low, cumulative frequency coding.

    encode(cum, freq, total):  r = range / total;  low += r * cum;  range = r * freq
                               while range < 2^24: shift
    shift:                     if low >= 2^32: add 1 to the output so far (propagate carry), low -= 2^32
                               output byte (low >> 24); low = (low << 8) & 0xFFFFFFFF; range <<= 8

Start: `low = 0`, `range = 0xFFFFFFFF`. For each text byte, encode with `cum = sum(freq[0..byte-1])`.

**Finishing.** After the last byte, apply any pending carry, then find the smallest `nb` in 0..4 such
that some `v` with its low `8*(4-nb)` bits zero lies in `[low, low + range - 1]`
(`v = (low + mask) & ~mask`); output the top `nb` bytes of `v`. Then **strip all trailing zero bytes**
from the whole output. The decoder pads missing input with zero bytes.

**Decoding.** `code` = first four input bytes (zero padded). For each of the `L` bytes:
build `freq`, `r = range / total`, `v = min(code / r, total - 1)`, pick the byte whose cumulative
interval contains `v`, then `code -= r * cum; range = r * freq`, and while `range < 2^24`
read another input byte (zero padded) into `code` and `range <<= 8`.

## Conformance

`test/vectors_v1.txt` holds frames made from fabricated packets. An implementation conforms if
`encode(text) == frame` for every vector (and declines exactly where the vector shows `-`), and
`decode(frame) == text`. `make test` runs the C reference against them. The vectors use fabricated
callsigns and positions only.

## Measured

On held-out stations (trained on 70% of 14,186 packets, tested on the other 30%): about 48% fewer
bytes at the pruning level model v1 uses (min count 8); see `docs/rationale.md` for the table and the method. Figures
measured on packets the model was trained on are higher and not representative.

## Compatibility with legacy stations

* A plain LoRa APRS frame (`3C FF 01` + text) is unchanged and remains the default.
* A compressed frame starts `3C FF` followed by a byte with the high bit set. Existing firmware
  expects `01` there and drops the frame; it is not misparsed as APRS. (Checked against one
  firmware's source, not against every implementation.)
* A sender uses a compressed frame only when it is strictly smaller, and a receiver that
  implements this gateways to APRS-IS as the original plain text, so APRS-IS and maps see ordinary
  packets.
* A digipeater that does not implement the format cannot repeat a compressed frame in a useful way.
  Deployment therefore needs the receiving side to implement it first (receive-always,
  opt-in transmit), which is how the reference firmware is built.

## Security and regulatory considerations

* The format is not encryption and hides nothing: the model is public and the codec is
  deterministic. Where the rules require the content of an amateur transmission to be decodable by
  anyone, a publicly documented code satisfies that; this is the author's understanding and has
  not been checked against the rules of each country.
* On the LoRa binding the callsign is inside the compressed payload. Receivers must decode before
  they can identify the station. The AX.25 binding keeps it readable in the address field.
* Decoders must reject malformed input (bad length, truncated data) without crashing; the reference
  decoder is fuzzed, and the length byte bounds the output.

## Model versioning

The model is part of the protocol. Model v1 is frozen (`model/MODEL-v1.sha256`). A new model gets a
new version number in the header, and implementations may support several. How versions are
published and retired would be for a standards body to set.

## Open issues

1. Over-the-air validation on real radios.
2. The marker for the AX.25 binding (distinct PID or in-band bytes) and how legacy iGates treat it.
3. Training data is one corpus (a few hours of APRS-IS over two days); a broader, ideally
   community-contributed corpus would be needed before a model is treated as final.
4. Whether to ship one model or several (for example per-region or LoRa-only).
5. Interaction with capability announcement (02), and with the phase 2 drafts in the companion repo `aprs-smarts`, which are ideas only.

## Implementations and tests

See the Conformance section above and the README.

