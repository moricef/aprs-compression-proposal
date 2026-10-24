# Proposal draft 08 - Priming the model with a station's previous packet

| | |
|---|---|
| Status | **Idea with a simulation. Not a standard, not implemented, not tested on air.** |
| Depends on | 01 Compressed frames |
| Changes to existing APRS | None to plain packets. A new frame variant; needs state at both ends |
| Evidence | `tools/delta_sim.py`, an ideal-cost simulation in Python on an hour of APRS-IS capture |

## Summary

A station sends the same kind of packet again and again: same comment, symbol, path, often most of the
same position. Spec 01 compresses each packet from scratch with a fixed table. This draft adds the
station's own earlier packet as extra context, so text the receiver has already seen costs almost
nothing and only what changed is coded, still with the fixed table.

Preferred reference: the **station's previous packet**. (A packet a few back, or a periodic full
packet, is more robust to loss; see below.)

## How it works

Both ends keep the last packet heard from each station. To code packet N, the model for that packet is
the static table plus the counts from packet N-1 of the same station, with the earlier packet's counts
weighted heavily (the simulation used weights 32 to 512; a fixed value around 128 to 512 is enough, so
nothing needs to be signalled). The result is then range-coded as in spec 01. The receiver builds the
same model from its copy of packet N-1 and decodes.

## Simulation result

3,000 VHF packets, sampled at random from held-out stations in an hour of multi-country APRS-IS
capture (57,883 unique packets in the training mix), 127 KB table (min-count 24). The frame is the
LoRa-style whole-packet frame of spec 07 (coded bytes plus 2 for version and length plus 2 for FCS),
compared with a plain AX.25 frame. Consecutive identical packets from one station are removed, so a
beacon heard twice by different iGates is not counted as a free repeat. 2,568 of the 3,000 packets had
an earlier packet from the same station; 432 were the first heard.

| Method | Avg frame | Saved vs plain AX.25 (74.9 B) |
|---|---|---|
| Static table (spec 01), same packets | 44.3 B | 41% |
| Primed with the **previous** packet, w=32 | 28.0 B | 63% |
| Primed with the previous packet, w=128 | 26.4 B | 65% |
| Primed with the previous packet, w=512 | 25.4 B | 66% |
| Primed with the packet four back, w=512 | 22.4 B | 70% |

Counting the 432 first-heard packets at the static cost, the overall saving is about **62%** with the
previous packet and about 66% with the packet four back. The static column agrees with the real codec
(41% measured at this table size).

## Limits of the simulation

* Ideal arithmetic-coding length from a Python version of the PPM blend, not real bytes from a codec.
* Traffic-weighted: frequent beaconers appear more often, as on the air.
* The receiver is assumed to hold the reference. Loss is not simulated.
* A beacon heard by two iGates with a different path can still look like a new packet, so some gain
  may be overstated; the result does not depend on a clock, because the captures carry no timestamps.
* One hour, one evening in Europe, one table. The 8-hour capture should repeat it.

## Costs and risks

* **State.** Each receiver stores the last packet per station. A small microcontroller may hold only a
  few.
* **Loss.** If the receiver missed the reference, it cannot decode the new packet and the packet is
  lost on top of whatever was lost. With the previous packet as reference this is more likely than with
  a periodic full packet. Mitigations to try: carry a short check of the reference so the receiver
  knows whether it holds the right one; send a full (unprimed) packet every Nth time; refer to the
  last full packet rather than the previous one (the simulation shows no loss in doing so).
* **Digipeaters and gateways.** One that does not hold the reference cannot decode, so cannot repeat or
  gate the packet in readable form. An iGate that decodes can gate the expanded plain text.
* **Movers.** A moving station changes its position each time, so its packets cost more than a fixed
  station's.
* **Identification.** As in spec 07, the callsign is inside the coded bytes.

## Open issues

1. Previous packet or a keyframe as the reference, and how often to send a full packet.
2. How a receiver learns it is missing the reference (a short check value in the frame).
3. Per-station memory on small devices.
4. Whether the same idea helps the LoRa binding (spec 01); it should, but is not simulated here.
5. Real bytes from a codec, and a loss simulation, before anything is proposed to a standards body.
