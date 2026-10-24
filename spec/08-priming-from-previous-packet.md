# Proposal draft 08 - Priming the model with a station's previous packet

| | |
|---|---|
| Status | **Idea with a simulation. Not a standard, not implemented, not tested on air.** |
| Depends on | 01 Compressed frames |
| Changes to existing APRS | None to plain packets. A new frame variant; needs state at both ends |
| Evidence | `tools/delta_sim.py` (ideal-cost simulation on an hour of APRS-IS capture) and `tools/receiver_sim.py` (does the receiver hold the reference) |

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

### A full frame every Nth transmission

A stationary station can send a full (unprimed, spec 01) frame every Nth transmission and prime the
others with the latest full frame, not the previous packet. Same data and table; w=512:

| Full frame every | Overall saving (full frames at the static cost) | Primed packets alone |
|---|---|---|
| 3 | 57% | 69% |
| 4 | 60% | 70% |
| 8 | 63% | 70% |
| never (previous packet as the reference) | 62% | 66% |

The overall figure is lower than priming from the previous packet only for small N, but a missed
packet then costs at most the packets until the next full frame, and missing a primed packet does not
affect the next one.

### Stationary and moving stations

Stations were classified from the position reports in the same capture: **stationary** is at least 3
position reports all within 0.002 degrees (about 200 m); **moving** is at least 3 with a spread of
0.005 degrees or more. 2,312 stations were stationary and 192 moving; the other 5,973 (fewer than 3
position reports, Mic-E only, objects, or somewhere in between) are left out of these two rows.
Previous packet as the reference, w=512; the full-frame columns are overall savings.

| Stations | Packets sampled | Static table | Primed, previous packet | Overall (first-heard at the static cost) | Full frame every 3 / 4 / 8 (overall) |
|---|---|---|---|---|---|
| All | 3,000 | 41% | 66% | 62% | 57% / 60% / 63% |
| Stationary | 3,000 | 43% | 68% | 66% | 60% / 63% / 67% |
| Moving | 1,302 | 43% | 70% | 69% | not run |

Stationary stations do a little better than the all-station average, but moving stations did not do
worse: they scored slightly higher. A moving station's position changes by a few characters, which is
cheap, and its comment and symbol repeat. The stationary group includes iGates and digipeaters with
long comments, which lowers its plain-frame saving. So the position change is not what limits the
gain; the sample of moving stations is small (192 stations), and the classification is rough.

## Does the receiver hold the reference? (moving stations)

A receiver can use only a reference packet it heard, and a moving station is heard by a given iGate
only part of the time. `tools/receiver_sim.py` rebuilds each moving station's transmissions from the
captured packets (one element per transmission, with the iGates that heard it, taken from the
q-construct) and asks, for every (receiving iGate, moving station) pair: of the packets this iGate
heard, in how many did it also hold the reference? 173 moving stations with at least 4 transmissions,
489 (iGate, station) pairs, 3,473 packets heard. A first packet, or a full frame, is always decodable.

| Receivers grouped by | Pairs | Packets heard | Previous packet | 2 back | 3 back | Full frame every 4 | Full frame every 8 |
|---|---|---|---|---|---|---|---|
| All pairs | 489 | 3,473 | 73% | 75% | 75% | 78% | 74% |
| Hears at least 90% of the station's packets | 76 | 1,595 | **99%** | 99% | 99% | 99% | 99% |
| Hears 50-90% | 82 | 824 | 75% | 77% | 75% | 79% | 78% |
| Hears under 50% | 331 | 1,054 | **33%** | 36% | 37% | 45% | 33% |
| Busiest quarter of iGates (by stations heard) | 316 | 1,895 | 71% | 71% | 72% | 76% | 71% |
| Quietest quarter of iGates | 34 | 658 | 78% | 86% | 80% | 84% | 81% |

* The expectation held: a receiver that hears most of a moving station's packets decodes nearly all
  of them (99%), while one at the edge of its range, which hears under half, decodes a third. Most
  pairs (331 of 489) are edge cases, though they account for fewer of the packets heard (1,054 of
  3,473), because a well-placed iGate hears many more.
* Asking for an older reference (2 or 3 back) or a full frame every 4 helps the edge receivers only a
  little (36% to 45%). A receiver that hears a quarter of the packets simply does not have the
  reference most of the time.
* Grouping iGates by how many stations they heard does not separate "hilltop" from "low-lying" well
  (the busiest quarter did slightly worse): that is a poor proxy for height, and the iGates' real
  elevations are not in the data.
* This is an **upper bound**. A transmission that no iGate heard is invisible, so the real gaps are
  larger and the real decodable shares are lower.

What it costs, for moving stations (from the simulation above: static 42.9 B per packet, primed with
the previous packet 22.5 B): a primed packet is only worth sending if enough receivers can decode
it. The break-even decodable share is about 52% (22.5 / 0.52 is 42.9 B per decodable packet). At the
overall 73% a delivered packet costs about 31 B instead of 43 B; for the well-placed receivers (99%)
about 23 B; for edge receivers (33%) about 68 B, which is worse than sending the full frame.

Consequence: for moving stations, priming from earlier packets is a good deal for the iGates that
matter most (the ones that hear a station well) and a loss for marginal ones. A moving station that
sends only primed packets would not appear at all to receivers at the edge of its range. A design
has to keep a self-contained (full) frame frequent enough for them, or let the sender fall back to a
full frame in conditions where it expects marginal reception (for example when moving fast, or
after a long gap).

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
* **Movers.** I expected a moving station's packets to cost more, since its position changes each time.
  The simulation did not show it (see "Stationary and moving stations").
* **Identification.** As in spec 07, the callsign is inside the coded bytes.

## Open issues

1. Previous packet or a keyframe as the reference, and how often to send a full packet.
2. How a receiver learns it is missing the reference (a short check value in the frame).
3. Per-station memory on small devices.
4. Whether the same idea helps the LoRa binding (spec 01); it should, but is not simulated here.
5. Real bytes from a codec, and a loss simulation, before anything is proposed to a standards body.
