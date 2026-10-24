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

That is close to the LoRa figure (about 49%). Dropping the AX.25 addresses without compressing does not help: a plain text frame in the LoRa style (marker, the text packet, 2-byte checksum) averages 76.8 B on these packets, 4% larger than AX.25 (73.6 B), because text callsigns are longer than 7-byte addresses. A **hybrid** that keeps the source and destination
addresses (so the frame is still valid AX.25 with a readable callsign) and a marker PID, and
compresses the path and info, saves about 30% (51.5 B). It is a rough estimate: the compressed text
used placeholder callsigns.

Airtime at 1200 baud: 73.6 B is 0.49 s, 55.9 B (current method) is 0.37 s, 40.6 B (whole-packet)
is 0.27 s. TXDELAY and preamble do not shrink; if they take 0.3 s per transmission (an assumption,
settings vary), total airtime goes from 0.79 s to 0.67 s (15% less) with the current method and to
0.57 s (28% less) with whole-packet compression.

### Reliability: a shorter frame is more likely to arrive

A frame is accepted only if every bit survives (the 16-bit FCS drops it otherwise), so fewer bits
means fewer chances for noise. The FCS itself is the same strength at any length; the gain is the
reduced exposure. Rough estimate, assuming independent bit errors (real fading comes in bursts, which
a shorter frame also dodges more often):

| Bit error rate | Plain AX.25, 73.6 B arrives | Whole-packet, 40.6 B arrives |
|---|---|---|
| 1 in 10,000 | 94% | 97% |
| 1 in 1,000 | 56% | 72% |
| 3 in 1,000 | 17% | 38% |

The gain is largest on a weak link, where a frame about 45% shorter can roughly double the share that
arrives, and small on a strong one. A shorter frame also occupies the channel less, which lowers the
chance of a collision roughly in proportion. Compression does not make a frame tougher: one flipped
bit still fails the FCS. Error correction (FX.25, IL2P) is a separate gain that adds to it. These
are calculations, not measurements.

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
equipment never sees it (confirmed with stock Direwolf in simulation: 0 of 6 decoded, nothing logged,
nothing passed to KISS clients). It does not help reach (legacy digipeaters still cannot repeat the
frame) or station identification.

It is not a free choice, because many stations use a **hardware KISS TNC**. A KISS TNC computes and
checks the FCS itself: the host sends and receives frames without it. Such a TNC cannot send a
non-standard FCS, and it drops incoming frames whose FCS does not match, so a station behind one could
neither send nor receive the alternative-FCS frame. Only software modems (Direwolf, a sound-card
modem) can use it. So the baseline needs to work with the **standard FCS and a PID marker**, which
any TNC passes through. Tested with a running Direwolf (`docs/legacy-decoder-test.md`): such frames
are logged as junk and handed to KISS clients, but not gated to APRS-IS. The alternative FCS is an
option for stations where both ends use software modems.

## Checksum and error protection

Which checksum a compressed VHF frame carries decides who can send it, who can receive it, and what
legacy equipment does with it. Four options:

| Option | Hardware KISS TNCs | Legacy decoders see | Extra bytes |
|---|---|---|---|
| A. Standard AX.25 FCS plus a PID marker | Send and receive | Junk in the log and to KISS clients; not gated to APRS-IS | 0 |
| B. A different 16-bit checksum | Cannot send or receive | Nothing (not logged, not passed to KISS, not gated) | 0 |
| C. Standard FCS inside FX.25 error correction | Send and receive if the TNC passes it | The ordinary frame inside (to be tested with a compressed frame) | 16, 32 or 64 check bytes |
| D. A stronger checksum, for example CRC-32 | Cannot send or receive | Nothing | 2 |

What has been tested (Direwolf, simulated audio, `docs/legacy-decoder-test.md`): option A frames are
decoded by stock Direwolf as junk and handed to KISS clients but not sent to APRS-IS; option B frames
are not decoded at all. Options C and D were not tested.

Why a different checksum is not the baseline: a hardware KISS TNC computes and checks the FCS itself.
It cannot send a non-standard one and it drops incoming frames that do not match its own, so a
station behind one could neither send nor receive options B or D. Only software modems (Direwolf,
sound-card modems) can use them.

The checksum is not the weak point of reliability. A 16-bit FCS detects errors equally well at any
frame length; the benefit of a shorter frame is fewer bits to corrupt (see Reliability above). Error
correction, which recovers frames that are dropped today, would be a larger and separate gain.
FX.25 wraps an ordinary AX.25 frame in Reed-Solomon check bytes in a way that is meant to let legacy
receivers still decode the frame inside. That is from the FX.25 design as I remember it, and has not
been tried with a compressed frame. IL2P is a different frame format that legacy cannot decode.

**Proposal:**
1. Baseline: option A. It passes through every TNC, is not gated by a Direwolf iGate, and costs no
   bytes.
2. Optional: FX.25 on top for reliability, once tested with a compressed frame.
3. Option B only for a pair of software modems that want legacy equipment not to see the frame at all.

Open: whether legacy equipment other than Direwolf mishandles a PID 0xF1 frame; whether hardware TNCs
pass FX.25 frames to the host unchanged; how often the junk in a legacy log is a nuisance.

## Coexistence with legacy stations, and channel load

On a shared VHF channel some stations will send plain frames and some compressed ones. The question is
whether that makes the channel more crowded. A compressed frame is shorter than the plain frame it
replaces, so on its own it takes less airtime. The channel gets busier only if a station sends the
same information more than once.

**Rule: do not send both.** A station MUST NOT send a plain frame and a compressed frame carrying the
same information as a matter of course. As arithmetic, using the VHF estimates above (73.8 B frame,
1200 baud, 0.3 s assumed setup): a plain frame takes about 0.79 s and an info-only compressed frame
about 0.69 s (13% less). If a fraction `a` of stations adopted info-only compression, total load would
be about `1 - 0.13a` if they send compressed only, and `1 + 0.87a` if each sent both. These are
calculations, not measurements. Sending both is worse than not adopting at all.

**What a sender does instead**
* It sends plain by default, and compressed only for receivers it has reason to believe can decode
  (spec 02: listen-and-switch, or a capability announcement).
* If it is unsure, it sends plain. It may send one plain frame every Nth transmission as a
  safety net; that is a small, bounded cost.
* A frame that receivers did not acknowledge (messages) falls back to plain.

**A tension between digipeating and gating.** From Direwolf's source (`direwolf.c`): the APRS
digipeater is called only for a frame that is an APRS UI frame (control 3, PID 0xF0), and the iGate
forwards only the same kind. So a frame with a different PID, which is how the marker is designed to
keep legacy iGates from gating junk, is also not repeated by that digipeater. Marking with the PID
protects APRS-IS and loses legacy digipeating. Keeping PID 0xF0 and putting the marker in the info
field gets digipeating and loses the protection, unless the marker begins with something iGates
discard (Direwolf's iGate drops a leading `?`, a generic query; whether other iGates and the APRS-IS
servers do is untested). This was read from the code; an actual digipeater run was attempted and
failed to repeat even a plain control frame, so it is not a test result.

**Which form to use in a mixed area**
| Form | Legacy digipeaters | Legacy iGates | Use |
|---|---|---|---|
| Info field only (AX.25 header and path kept) with a PID marker | **Direwolf's APRS digipeater will not repeat it** (see below); other digipeaters untested | Do not gate it (tested with Direwolf) | Reaches direct range and upgraded digipeaters only |
| Hybrid (source and destination kept, path inside the coded bytes) | Nothing to repeat: it has no digipeater address | Do not gate it | Direct range only |
| Whole packet, LoRa-style | Cannot repeat it | Do not see it | Direct range to an upgraded receiver, or an area where the digipeaters have been upgraded |
| Info field only with PID 0xF0 and a marker inside the info | Expected to repeat it | **Would gate it to APRS-IS as junk** unless the marker is something iGates reject | The marker has to be chosen carefully (see below) |

A whole-packet frame in a mixed area reaches only stations within direct range that implement it. A
station that wants wider coverage may be tempted to send plain too, which is the problem above, so the
whole-packet form should not be the mixed-area default.

**Gating.** A compressed frame reaches APRS-IS only if an upgraded iGate in range decodes it and gates
the expanded plain text. If none is in range the packet does not reach the internet, and the station's
only options are to send plain or to go unseen. An area benefits from the format in proportion to how
many iGates upgrade, so iGates are the place to start.

**Collisions and carrier detect.** A legacy radio's carrier detect responds to any signal, so the new
frames defer and collide with legacy traffic as ordinary packets do; shorter frames reduce the chance
of a collision with them. Legacy decoders that do hear a compressed frame with a standard checksum log
it as junk (`docs/legacy-decoder-test.md`); that is log noise, not channel load.

**Open**
* Hardware digipeaters and other software digipeaters (aprx, soundmodem): do they repeat a frame with a PID other than 0xF0? Direwolf's APRS digipeater does not; the others are untested.
* How quickly iGates and digipeaters upgrade, and so how long mixed operation lasts.
* Whether a regional trial (an area that adopts together) shows a drop in channel use. That is the
  measurement the companion repo `aprs-smarts` plans.

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
