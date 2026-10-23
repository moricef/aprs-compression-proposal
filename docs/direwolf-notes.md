# Notes: implementing this in Direwolf

**Status: planning notes. Nothing here has been implemented, and I have not yet read the Direwolf
source for these paths, so the hook points are to be confirmed.**

## Why Direwolf

A Direwolf station is a software TNC: the Pi decodes and encodes the packets, and the transceiver is
only a radio. So older VHF rigs are not a barrier; only hardware-TNC gear (radios with a built-in
packet modem) cannot adopt this. Direwolf is also written in C, and the reference codec
(`reference/aprs_ppm.c`, C99, integer-only) and `model/aprs_ppm_model_v1.c` would drop in without a
port. MIT-licensed code can be included in a GPL project.

## Where it would hook in (to be confirmed)

* **Transmit:** after the packet is built and before the AX.25 frame is packed, compress the info
  field if the channel is configured for it and the result is smaller.
* **Receive:** after the frame is parsed and before the APRS decoder and the iGate see it, expand a
  marked info field back to plain text. The iGate then gates ordinary text to APRS-IS.
* **LoRa HAT channel:** the LoRa driver sends `<\xff\x01` + text; the same codec could send the
  `3C FF 81` frame there (spec 01). That is a separate change.

## Configuration

Opt-in, per channel, off by default. Name and syntax to be decided with the maintainer.

## Process

* Keep it out of PR #638 (the LoRa hardware driver). It is a separate feature.
* Raise it with WB2OSZ after #638 is merged, as one of the separate ideas already planned (device ID,
  comment-every-N, Mic-E encoding), and ask before writing it.
* Test against other software first, because a legacy iGate or decoder may mishandle an unknown
  marker: see spec/07-ax25-binding.md.

## Things to check before anyone transmits it on VHF

* How common decoders and iGates handle the chosen marker.
* The amateur rules for data emissions and station identification. My understanding, from memory and
  not verified, is that a publicly documented code is permitted, which is a reason to keep the spec
  and the model table public. Check the current rules for your service.
