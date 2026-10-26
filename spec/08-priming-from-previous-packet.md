# Proposal draft 08 - Priming the model with a station's previous packet

| | |
|---|---|
| Status | **Not a standard. Implemented in the ESP32 firmware fork and a Direwolf branch; tested over the air once at short range (24 Oct 2026, `docs/over-the-air-test.md`); lost-frame behaviour not yet seen on air.** |
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

## Wire format (reference codec)

**Changed 26 October 2026, not compatible with the first layout.** The first layout had a 6-byte header with no text check byte (a primed frame could decode to the wrong text from a stale reference that shared the 8-bit reference check). The layout below adds the text check at byte 5. The frame mark is still `3C FF C1`, so builds from before this change must be upgraded together with the rest; see `docs/known-issues.md`.

Implemented in `reference/aprs_ppm.c`; `make test` checks it against `test/vectors_primed_v1.txt`.

    offset  size  meaning
    0       1     0x3C
    1       1     0xFF
    2       1     0xC0 | model version (1..63). Bit 7 = compressed, bit 6 = primed. Model v1 -> 0xC1
    3       1     station id = CRC-8 of the source callsign (the text before the first '>')
    4       1     reference check = CRC-8 (polynomial 0x07) of the whole reference text
    5       1     text check = CRC-8 (polynomial 0x1D) of the whole original text
    6       1     L = length in bytes of the original text, 1..255
    7       n     range-coded bytes

CRC-8 is polynomial 0x07, initial value 0, no reflection, no final xor. A full (unprimed) frame is the spec 01
frame `3C FF (0x80|version) L coded`; a plain frame is `3C FF 01` + text. A primed frame costs 2 bytes more
than a full one (3 bytes more) because of the station id, the reference check and the text check.

**Model.** Same table as spec 01. For each coded byte and each order k = 3..0, every position j >= k in the
reference text whose preceding k bytes equal the current context adds `APRS_PPM_PRIME_WEIGHT` (512) to the
count of the byte at j, on top of the table's count for that context. The merged counts then go through the
unchanged PPM method C blend and range coder. With no reference this reduces to spec 01 exactly.

**Reference.** The reference for a station is the text of its **last full or plain packet**, not simply its
previous packet. A primed frame never replaces the reference, so losing a primed frame does not break the
next one; only losing the full frame does, and then the primed frames that refer to it are dropped until the
next full frame arrives.

**Receiver.** Keep, for each source, the text of the last plain or full packet received. On a primed frame,
look up the stored packet whose station id matches and whose CRC-8 equals the reference check. If there is
none, drop the frame: the packet is lost, as it would be for any receiver that did not hear it. After
decoding, the CRC-8 of the decoded source callsign must equal the station id, and the text check (polynomial
0x1D) of the whole decoded text must equal byte 5; if either differs, drop the frame. The text check is what
catches a reference that has the same 8-bit reference check as the real one but different bytes (a receiver that
missed an update), and a frame cut short. It uses a different polynomial from the reference check on purpose, see
[`docs/known-issues.md`](../docs/known-issues.md).

**Sender.** Keep, for each source, the text of the last full or plain packet transmitted and how many primed
frames have gone since. Send a full frame at least every Nth transmission for that source, and otherwise a
primed frame referring to the stored text; send a plain frame if neither is smaller than plain.

### Measured with the real codec

3,000 consecutive packet pairs from 536 LoRa stations that model v1's training data did not contain (the
three-hour capture; reference = the station's previous different packet, which is what a full frame every 2nd
transmission gives), model v1, real frame bytes
including the 6-byte primed header of the first layout (the current layout has 7 bytes, so add 1 byte to each primed figure; see `docs/known-issues.md`):

| | Avg frame | Saved vs plain (60.1 B) |
|---|---|---|
| Full frame (spec 01) | 33.1 B | 45% |
| Primed with the previous packet | 21.2 B | **65%** |

No pair needed the plain fallback. A full frame every 2nd transmission would average about 27 B (55%); every
4th, about 24 B (60%). These agree with the simulation below (73% on primed packets before the 2-byte
header).

## Simulation result

Simulations on a three-hour multi-country APRS-IS capture (135,299 unique packets; see
`docs/per-country-estimate.md`), 144 KB table (min-count 48), 3,000 packets sampled at random from held-out
stations for each run. The frame is the LoRa-style whole-packet frame of spec 07 (coded bytes plus 2 for
version and length plus 2 for FCS) for VHF, and the LoRa frame (3C FF, version, length) for LoRa,
each compared with today's plain frame. Consecutive identical packets from one station are removed, so a
beacon heard twice by different iGates is not counted as a free repeat.

VHF, all stations: 2,768 of the 3,000 packets had an earlier packet from the same station; 232 were the
first heard. Plain AX.25 averages 80.2 B.

| Method | Avg frame | Saved vs plain AX.25 |
|---|---|---|
| Static table (spec 01), same packets | 50.0 B | 38% |
| Primed with the **previous** packet, w=32 | 29.7 B | 63% |
| Primed with the previous packet, w=128 | 27.3 B | 66% |
| Primed with the previous packet, w=512 | 25.6 B | 68% |
| Primed with the packet four back, w=512 | 24.3 B | 70% |

Counting the first-heard packets at the static cost, the overall saving is about **66%** with the previous
packet and about 67% with the packet four back. For LoRa, priming with the previous packet saves 73% on the
primed packets (static table 39%) and **70% overall**.

### A full frame every Nth transmission

A station can send a full (unprimed, spec 01) frame every Nth transmission and prime the others with the
latest full frame, not the previous packet. VHF, all stations, w=512:

| Full frame every | Overall saving (full frames at the static cost) | Primed packets alone |
|---|---|---|
| 2 | 52% | 69% |
| 3 | 58% | 70% |
| 4 | 60% | 69% |
| 8 | 64% | 69% |
| never (previous packet as the reference) | 66% | 68% |

For LoRa: a full frame every 2nd transmission gives 55% overall and every 4th gives 63%. The overall figure
is lower than priming from the previous packet only for small N, but a missed packet then costs at most
the packets until the next full frame, and missing a primed packet does not affect the next one.

### Stationary and moving stations

Stations were classified from the position reports in the same capture: **stationary** is at least 3
position reports all within 0.002 degrees (about 200 m); **moving** is at least 3 with a spread of 0.005
degrees or more. 4,016 stations were stationary and 355 moving; the other 5,424 (fewer than 3 position
reports, Mic-E only, objects, or somewhere in between) are left out of these rows. w=512.

| Stations | Static table | Primed, previous packet | Overall (first-heard at the static cost) | Full frame every 2 / 3 / 4 / 8 (overall) |
|---|---|---|---|---|
| All | 38% | 68% | 66% | 52% / 58% / 60% / 64% |
| Stationary | 40% | 63% | 62% | not run / 58% / 60% / 64% |
| Moving | 39% | 68% | 67% | 53% / 58% / 60% / not run |

Moving stations do not do worse than stationary ones; here they do slightly better. A moving station's
position changes by a few characters, which is cheap, while its comment and symbol repeat, and the
stationary group includes iGates and digipeaters with long comments. So the position change is not what
limits the gain. The moving group is small (355 stations) and the classification is rough.

## Does the receiver hold the reference? (moving stations)

A receiver can use only a reference packet it heard, and a moving station is heard by a given iGate only
part of the time. `tools/receiver_sim.py` rebuilds each moving station's transmissions from the captured
packets (one element per transmission, with the iGates that heard it, taken from the q-construct) and
asks, for every (receiving iGate, moving station) pair: of the packets this iGate heard, in how many did
it also hold the reference? 322 moving stations with at least 4 transmissions, 1,077 (iGate, station) pairs,
10,169 packets heard. A first packet, or a full frame, is always decodable.

| Receivers grouped by | Pairs | Packets heard | Previous packet | Full frame every 2 | every 3 | every 4 | every 8 |
|---|---|---|---|---|---|---|---|
| All pairs | 1,077 | 10,169 | 74% | 87% | 82% | 79% | 74% |
| Hears at least 90% of the station's packets | 129 | 4,614 | **99%** | 100% | 100% | 100% | 99% |
| Hears 50-90% | 147 | 2,009 | 75% | 87% | 82% | 80% | 75% |
| Hears under 50% | 801 | 3,546 | **41%** | 70% | 59% | 51% | 40% |
| Busiest quarter of iGates (by stations heard) | 712 | 5,353 | 68% | 84% | 78% | 74% | 68% |
| Quietest quarter of iGates | 51 | 1,546 | 84% | 92% | 90% | 88% | 86% |

* A receiver that hears most of a moving station's packets decodes nearly all of them (99%), while one at
  the edge of its range, which hears under half, decodes 41%. Most pairs (801 of 1,077) are edge cases,
  though they account for fewer of the packets heard (3,546 of 10,169), because a well-placed iGate hears
  many more.
* A full frame every 2nd transmission lifts the edge receivers from 41% to 70%; every 4th only to 51%.
* Grouping iGates by how many stations they heard does not separate "hilltop" from "low-lying" well (the
  busiest quarter did worse): that is a poor proxy for height, and the iGates' real elevations are not in
  the data.
* **These percentages are not reliable, and the direction of the error is unknown.** APRS-IS servers drop
  duplicate packets, so the captured feed shows a transmission only from the first iGate that delivered
  it. Most transmissions here show exactly one iGate. An iGate that heard a packet but lost the race to
  deliver it is counted as not having heard it, so every receiver's coverage, and with it the decodable
  share, is **understated**. Working the other way, a transmission that no iGate delivered is invisible,
  which **overstates** coverage. The two errors do not cancel in any known way. What does hold is the
  arithmetic: a receiver decodes a primed packet only if it holds the reference, so the decodable share
  follows how much of the station's traffic the receiver hears. The measured shares need real receiver
  logs (see below).

### Suggested default for moving stations: a full frame every 2nd transmission

Moving stations, previous-packet reference against a full frame every 2, 3 or 4 transmissions
(`tools/delta_sim.py --subset moving`, `tools/receiver_sim.py`). Savings are overall, against the plain
AX.25 frame (about 75 B); the decodable shares share the APRS-IS dedup caveat above, so read them as
relative, not absolute:

| Scheme | Overall saving | Avg frame | Decodable, all pairs | Decodable, hears under 50% |
|---|---|---|---|---|
| Static frame every time (spec 01) | 39% | 45.7 B | 100% | 100% |
| Primed with the previous packet | 67% | 24.7 B | 74% | 41% |
| **Full frame every 2** | **53%** | 35.2 B | **87%** | **70%** |
| Full frame every 3 | 58% | 31.4 B | 82% | 59% |
| Full frame every 4 | 60% | 29.9 B | 79% | 51% |

Per packet a receiver can decode, a full frame every 2 costs about 40 B (35.2 / 0.87), against about 33 B for
priming every packet (24.7 / 0.74) and 46 B for a full frame every time. So priming with a full frame every
2 is a middle path: it gives up 14 points of saving (53% against 67%) to make the packets decodable for about
87% of receivers instead of 74%, and for edge receivers about 70% instead of 41%.

The interval does not need to be agreed in advance: each frame says whether it is a full frame or primed, so
a sender can pick any interval and change it. Reasonable defaults to propose: every 2 for moving stations,
every 4 to 8 for stationary ones, and a full frame after a gap in transmissions or when the sender knows
reception is marginal.

### What data would settle it

* Raw receive logs from several receivers that hear the same moving stations, with timestamps,
  before APRS-IS deduplication (for example Direwolf logs from volunteers' stations). The feed here
  has neither timestamps nor the duplicates.
* A propagation model with known iGate and station positions, which would give coverage without a
  receiver log.
* A receiver of our own (an RTL-SDR with Direwolf) shows what one location hears, not several.

## Limits of the simulation

* Ideal arithmetic-coding length from a Python version of the PPM blend, not real bytes from a codec.
* Traffic-weighted: frequent beaconers appear more often, as on the air.
* The receiver is assumed to hold the reference. Loss is not simulated.
* A beacon heard by two iGates with a different path can still look like a new packet, so some gain
  may be overstated; the result does not depend on a clock, because the captures carry no timestamps.
* Three hours of one evening in Europe, one table, 3,000 sampled packets per run (so a point or two of the differences is sampling noise).

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
