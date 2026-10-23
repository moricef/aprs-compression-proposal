# 05 - Reach-aware repeating

**Status: idea, not specified or implemented.**

MeshCore gives the repeating job to repeaters, which are usually high, fixed sites. What makes a
good repeater is not its height but its **reach**: how far, and in which directions, it actually
hears and is heard. Reach can be measured by the station itself instead of configured.

## Measuring reach

Every packet a station hears carries (or can carry) what is needed:

* **Where the sender is.** Position packets give coordinates, so distance to each directly heard
  station is computable.
* **How well it was heard.** RSSI and SNR, which the radio reports. The margin above the
  spreading factor's decode limit says how much more distance was available.
* **How hard the sender was transmitting.** APRS already defines extension fields for this:
  `PHGphgd` (power, antenna height, gain, directivity) and `RNGrrrr` (stated range in miles). Few
  LoRa trackers include them. Putting transmit power and antenna gain into beacons is what makes
  link-budget estimates possible.

A station keeps, over a window (days), for **directly heard** stations only (not packets that
arrived via a digipeater): the distance distribution, per compass sector, and an estimate of reach
such as a high percentile of distance weighted by margin. Self-reported positions can be wrong
(bad GPS), so use robust statistics and ignore one-off outliers.

Claimed reach (PHG/RNG) and measured reach are different things; a station should publish the
measured one, and may publish both.

## What it is for

1. **Choosing who repeats.** A station whose measured reach is large relative to its neighbours is
   a good wide-area repeater; a station with small reach need not repeat at all. This replaces a
   hand-set "I am a digipeater" with something observed.
2. **Ordering repeats.** Combine with SNR-weighted delay (03): among stations that heard a packet,
   those with the greatest reach that heard it weakly (the sender is at their edge) repeat first,
   and others cancel when they hear it repeated.
3. **Fixing the one-way problem.** A tracker that hears an iGate does not know the iGate hears it.
   If the iGate's beacon states its transmit power (and antenna gain), the tracker knows both
   powers and its own received margin, so it can estimate its uplink margin:
   `uplink margin ~ downlink margin - (iGate power - own power)`, adjusted for antenna gain and
   receiver sensitivity differences. That is a far better signal for "can I switch to compressed
   frames?" (02) or "do I need to raise power / spreading factor?" than "I heard it".
4. **Advertising.** An iGate or digipeater puts its measured reach (range, and sectors if worth the
   bytes) in its capability beacon so trackers and other repeaters can use it.

## Cautions

* Reach is not a circle. Terrain makes it directional, which is why sectors may matter.
* A station only learns about stations that transmit. Silent areas look like zero reach.
* Measurement needs enough traffic; a new or quiet station has no estimate yet and should fall
  back to configuration.
* Gaming: a station can claim PHG/RNG it does not have. Locally measured values cannot be faked
  from outside, which is a reason to prefer them.

## Data that could calibrate this

Drive-test and balloon-flight logs with positions and RSSI/SNR at known distances exist for several
spreading factors; they would show how well margin predicts range before anything is built.
