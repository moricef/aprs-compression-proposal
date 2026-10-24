# Proposal draft 09 - A structured header for position packets, order-3 for the comment

| | |
|---|---|
| Status | **Idea with arithmetic from measured parts. Not specified, not built, not tested on air.** |
| Depends on | 01 Compressed frames |
| Changes to existing APRS | None to plain packets. A new frame type, like 01 |
| Credit | The idea of a fixed binary layout for a position beacon comes from APRS 438 (ON4AA). This draft is an independent sketch; check 438's terms before reusing any of its layout |

## Summary

For a position packet, send the callsign, path, type and position in a compact fixed binary layout,
and compress only the free-text comment with the order-3 model of spec 01. Everything else (messages,
weather, objects, telemetry, any position packet the layout cannot represent) is sent as in spec 01.

"Stateless" here means the receiver needs nothing it heard earlier to decode a frame. This is the best
stateless candidate measured so far for LoRa; spec 08 (priming) saves more but needs the receiver to
hold an earlier packet.

## Why: where the order-3 model spends its bytes

LoRa position packets from held-out stations (4,282 packets), min-count 24, without frame overhead:

| Part | Plain | Order-3 whole packet |
|---|---|---|
| Header: callsign, destination, path, packet type | 25.1 B | 8.4 B |
| Position block | 13.7 B | 13.8 B |
| Comment | 29.2 B | 14.9 B |
| Total | 68.0 B | 37.1 B |

The position block does not compress, because most of these packets already use the base-91 position,
which is close to random. The comment halves. The header is where fixed fields beat a model: a
callsign is about 31 bits however it is coded, and a path or type is a few bits, not several characters
that the model has to predict.

## What 438 actually contributes

From 438's documentation (`geolocation.html`, `ccccd.html`): its 12-byte position field is the standard
APRS base-91 compressed position written as characters (`/XXXXYYYY$cs`: table, 4 characters of
longitude, 4 of latitude, symbol, 2 of course and speed). That is the same position most LoRa trackers
already send, and the order-3 model cannot shrink it (13.8 B before and after in the table above). So
the part of 438 that saves bytes over the order-3 model is the **header packing**: the callsign in 4
bytes and SSID, path code and type in 1 byte, about 5 B against 8.4 B. The position and a binary form
of it (about 6 to 7 B instead of 12) are where a purpose-built layout could do better than 438's.

## Sketch of the layout (indicative, not final)

| Field | Size | Note |
|---|---|---|
| Marker, version, length | about 2 B | as in spec 01 |
| Callsign | 4 B | six characters of A-Z, 0-9 and space need about 31 bits |
| SSID, path code, type | 1 B | a few bits each |
| Device ID (the destination / tocall, such as `APLRT1`) | 1 to 2 B | an index into a table of common device IDs, with an escape; 438's 17 B has no field for this, so a faithful version adds it |
| Latitude, longitude | about 7 B | the same precision as the base-91 position (8 characters) |
| Symbol table and code | 2 B | |
| Comment | order-3 coded | |

Fixed part: about 15 to 18 B, against 22.2 B for the header and position under the order-3 model. The
438 layout is 17 B in total with no comment. That is an arithmetic sketch from the measured parts, not
a measured result.

**Path codes.** In the LoRa position packets seen, five paths cover 89% (of 14,210 unique): the
iGate-heard path with no digipeater (`TCPIP*`, 45%), `WIDE1-1` (33%), `WIDE1-1,WIDE2-1` (5.6%), none
(4.4%) and `WIDE2-1` (1.6%). 438's own four codes fit only 57% of these packets, mainly because
`WIDE1-1` alone is not among them. A table of about five to eight codes, plus an escape for a path
written out in text, would cover nearly all.

**Could 438 simply gain a `WIDE1-1` code?** In principle yes, and it is the single most useful addition:
with it, 89.4% of these LoRa position packets fit, against 56.6%. The obstacle is the byte. From
438's documentation (`ccccd.html`) one byte holds the SSID (0-15, 4 bits), the path code (0-3, 2 bits)
and the data type (0-3, 2 bits). All values of all three fields are assigned and the documentation
defines no spare or reserved value, so a fifth path code does not fit without either widening the
field to a second byte (17 B becomes 18 B), or taking a bit from the SSID or the data type. That is a
change to 438's format and would belong in a protocol revision by its maintainer. In our own layout
(this draft) the path field can simply be wider. 438 lists `WIDE2-2` and `WIDE1-1,WIDE2-2` as having no
code.

What is still left after adding `WIDE1-1` (10.6%): paths that already contain a digipeater callsign
with a used mark, such as `XE2OK-15*` (about 8%), and `WIDE2-2` (1.1%) and `WIDE1-1,WIDE2-2` (0.7%).
These would use the escape or fall back to spec 01.

## Expected size (from earlier measurement)

| | Order-3 whole packet | Structured header + order-3 comment |
|---|---|---|
| LoRa position packets, min-count 24 (small-device table) | 41.6 B (41%) | 34.0 B (52%) |
| Same, min-count 8 (larger table) | 36.4 B (49%) | 31.1 B (56%) |

Both assume every packet fits the layout (89% of paths do; the rest fall back to spec 01), and charge
the hybrid 2 B of overhead against 4 B for the whole-packet frame. A real marker and version could
cost a byte or two more. The 17 B used for the hybrid in the table is 438's figure, which has no device ID; adding one costs 1 to 2 B, which brings the gain to about 3 to 6.5 B on a position packet (3 to 4 B at the larger table, 5.5 to 6.5 B at the small one).

**Table size.** The comment-only table is about half the size of the whole-packet table: 16 KB against
33 KB at min-count 24, 45 KB against 95 KB at min-count 8 (LoRa-only training data). A tracker that
sends only position packets needs only the small one.

## Interaction with priming (spec 08)

A packet primed with an earlier packet already pays little for a repeated header and comment, so the
structured header helps less there. It still helps the full frames: with a full frame every 2nd to
4th transmission, those are the structured-header frames.

## Open issues

1. The actual layout: field widths, the path-code table and its escape, how SSIDs 0-15 and the type
   fit in one byte, whether altitude, course and speed (carried today inside the comment) get fields.
2. Position packets with a timestamp, objects, items and Mic-E: layout or fallback to spec 01.
3. Whether to align with 438 so that one codec can read both (interoperability) or stay independent.
4. Flash: an iGate needs both tables; a tracker needs only the comment table.
5. A real codec and byte counts, and an over-the-air test.
6. 438's current status and terms (not re-checked since the earlier research).
