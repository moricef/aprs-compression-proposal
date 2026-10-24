# 07 - AX.25 binding for compressed frames (VHF APRS)

**Status: idea. The compression itself is tested on a PC; this way of carrying it on AX.25 is not tested
against any real TNC, digipeater or iGate.**

Part 01 describes the codec and the LoRa binding (a three-byte header). VHF APRS carries the same
APRS packets inside AX.25 unconnected (UI) frames, so the same codec and the same model v1 apply.
Only the framing differs.

## What can be compressed

An AX.25 UI frame is:

    flag | destination (7) | source (7) | digipeaters (7 each) | control | PID | info | FCS | flag

The address fields are binary, and every digipeater on the path must read them to decide whether to
repeat the frame, so they stay as they are. Only the **info field** is compressed. Consequences:

* The source callsign stays readable in the address field, as the station-identification rules
  expect. (On the LoRa binding the callsign is inside the compressed text instead.)
* The saving is smaller than on LoRa, because about a quarter of the frame cannot be touched.

## Measured

834 held-out RF-heard VHF packets, a model trained on 70% of 14,186 packets, info field only,
assuming 7 bytes per address, 2 bytes of control and PID, 2 of checksum, and 2 bytes of overhead
for the length and version:

| | Plain | Compressed | Saved |
|---|---|---|---|
| Info field | 52.3 B | 34.4 B | 34% |
| Whole AX.25 frame | 79.0 B | 61.1 B | 23% |
| Frame airtime at 1200 baud | 0.53 s | 0.41 s | |

TXDELAY and preamble are not counted and do not shrink. This is a pessimistic figure: the model was
trained on whole text packets, so the first bytes of an info field have less context than they
could. A model trained on info fields alone, or a context primed with a fixed prefix, may do
better. Neither has been tried.

## Why VHF saves less than LoRa: the address fields

A VHF APRS packet is an AX.25 frame:

    flag | destination | source | digipeater 1 | digipeater 2 | control + PID | info (APRS text) | FCS | flag
           7 bytes       7 bytes  7 bytes        7 bytes        2 bytes         ~46 bytes          2 bytes

Each address is exactly 7 bytes (six characters and an SSID/flags byte) however short the callsign.
In 4,948 held-out RF-heard packets (20 minutes of multi-country capture, see
`docs/per-country-estimate.md`) the average frame is 73.6 bytes: the addresses, control/PID and FCS
are 27.8 bytes (38%) and the info field is 45.8 bytes. The packets carry 1.4 digipeater addresses
on average, about 10 bytes. Only the info field is compressed, so a 39% cut on the info is 24% of
the frame. The most that compressing only the info could ever save is 62% (the info's share).

The addresses stay uncompressed because a digipeater works by reading them in a fixed layout: it
checks whether its callsign is next in the path, decrements the hop count (`WIDE2-2` to `WIDE2-1`),
sets the "has been repeated" flag and inserts its own callsign. Much of that is hardware or old
firmware that will not be updated, and it would drop a frame whose address field it cannot parse.
This is a deployment choice, not a law: LoRa APRS routes by its text path as well, and a LoRa
digipeater that does not implement the compressed format also cannot repeat a compressed frame. We
accept that on LoRa because the base is new and its firmware is updated often. VHF has a very large
installed base of digipeaters.

### Measured with the 20-minute data

Whole AX.25 frame, 4,948 held-out VHF packets, min-count 8:

| Variant | Saved |
|---|---|
| Current: info compressed, 2 bytes of overhead | 24.0% |
| A table trained only on VHF info fields, 2 bytes overhead | 24.0% (no gain from specialising) |
| Current table, overhead removed (version in the marker, no length byte) | 26.4% |

Overhead means the version and length bytes that start the compressed info field. A registered marker
(a PID or tocall) can carry the version, and an end-of-data symbol can replace the length.

## Making VHF more like LoRa APRS: whole-packet compression

LoRa APRS compresses the whole text packet, callsign and path included, because there is no AX.25
address field to preserve. The same could be done on VHF with a frame type of its own:

    flag | marker + version | length | range-coded TNC2 text (SRC>DEST,PATH:info) | FCS | flag

The marker would be a PID value or a reserved tocall. This is the same codec and model, applied to
the whole packet. Estimated on the same 4,948 held-out packets (global table), against the
plain AX.25 frame:

| Packets | Plain AX.25 | Whole-packet | Saved | With no length byte |
|---|---|---|---|---|
| All VHF | 73.6 B | 40.6 B | 45% | 47% |
| No digipeater in the path | 66.0 B | 39.2 B | 41% | 44% |
| 1 digipeater | 74.5 B | 38.5 B | 48% | 51% |
| 2 or more digipeaters | 75.3 B | 43.1 B | 43% | 45% |

That is close to the LoRa figure (about 49%). A **hybrid** that keeps the source and destination
addresses (so the frame is still valid AX.25 with a readable callsign) and a marker PID, and
compresses the path and info, saves about 30% (51.5 B). It is a rough estimate: the compressed text
used placeholder callsigns.

Airtime at 1200 baud: 73.6 B is 0.49 s, 55.9 B (current method) is 0.37 s, 40.6 B (whole-packet)
is 0.27 s. TXDELAY and preamble do not shrink; if they take 0.3 s per transmission (an assumption,
settings vary), total airtime goes from 0.79 s to 0.67 s (15% less) with the current method and to
0.57 s (28% less) with whole-packet compression.

### Would it break legacy APRS?

Plain frames stay as they are, and a station that does not implement the new frame type sends and
receives them as before. But "legacy would be uncompressed" does not make everything free:

* **Reach.** A whole-packet frame has no digipeater field a legacy digipeater can read, so legacy
  digipeaters ignore it. It reaches only stations within direct range that implement the format,
  plus any digipeaters that are upgraded. A legacy iGate does not put it on APRS-IS.
  A station would send plain until it knows the stations it needs can decode (see 02), or send both.
* **What legacy decoders do with the frame.** Tested in simulation (`docs/legacy-decoder-test.md`): stock
  Direwolf decodes a frame with a valid FCS, shows binary junk for the payload, and drops a frame
  with a different FCS without comment. Not tested with other decoders; this matters most for the hybrid
  (valid AX.25) and least for a non-AX.25 frame, which a TNC may simply drop on the failed address
  parse. Needs testing with Direwolf, aprx, APRSdroid and common hardware TNCs.
* **Station identification.** In the whole-packet form the callsign is inside the compressed bytes.
  Where the rules require the callsign to be sent in a form others can read, a publicly documented
  code may satisfy this, but I have not checked it against the rules (US or elsewhere). The hybrid
  keeps the callsign readable in the address field.
* **Channel load.** The compressed frame is shorter, so the channel carries more; the legacy side is
  affected only by there being fewer collisions.

So not "anything we want", but a lot of room: the constraint is who can decode, not the legacy
packets themselves.

### A different FCS or start pattern as the marker

A legacy TNC drops a frame whose checksum does not match, silently. A new frame type could use a
different CRC polynomial or seed, or a distinct start pattern instead of the flag run, so legacy
equipment never sees it (confirmed with stock Direwolf in simulation: 0 of 6 decoded). It does not
help reach (legacy digipeaters still cannot repeat the frame) or station identification.

## Marker: how a receiver knows the info is compressed

Two candidates; neither is chosen.

1. **A distinct PID value.** Cleaner. Needs a value that is not assigned in the AX.25 v2.2 PID list
   (to be checked). Hardware digipeaters repeat UI frames according to the address field, so I expect
   them to repeat a frame with an unusual PID, but that is unverified.
2. **A marker at the start of the info field.** The LoRa bytes `3C FF 81` would work technically, but
   `<` (0x3C) is already an APRS data-type identifier (station capabilities), so legacy decoders
   might misread the frame.

Either way the info field becomes `[version][length][coded bytes]`: a version byte (model version)
and the original length, then the range-coded data. When the PID is the marker, the three-byte LoRa
header is not needed.

## Rules

* A station MUST NOT gate a compressed frame to APRS-IS unless it first expands it. APRS-IS carries
  plain text only.
* A station that cannot decode a version MUST drop the frame and MUST NOT gate or digipeat-modify it.
* Sender MUST fall back to a plain frame when compression does not make the frame smaller.
* The source callsign stays in the address field untouched.

## Open questions

* Which marker, and how legacy iGates, TNCs and decoders react to each. Needs testing against the
  common ones (Direwolf, APRSdroid, aprx, hardware TNCs) off the air or on a private channel first.
* Whether digipeaters need to know anything. Ideally not: that is the point of leaving the address
  field alone.
* Compressing the addresses too (the whole-packet frame above, about 45%) only works for stations
  that implement it, and for digipeaters only if they are upgraded. Direct-range use needs no
  digipeater at all. A pilot region, or a direct-to-iGate mode, is the natural first step.
* Whole-packet or hybrid, and how legacy decoders react to either (see above).
* Station identification for the whole-packet form.
* 9600 baud: the saving in seconds is smaller than at 1200 baud.
