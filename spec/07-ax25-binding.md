# 07 - AX.25 binding for compressed frames (VHF APRS)

**Status: idea. The compression itself is tested; this way of carrying it on AX.25 is not tested
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
* Compressing the addresses too (closer to the LoRa ratio) only works if every digipeater on the
  path is upgraded; with mixed hardware it is not possible.
* 9600 baud: the saving in seconds is smaller than at 1200 baud.
