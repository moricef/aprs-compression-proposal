# Over-the-air test checklist

Tests A-D cover the LoRa binding (two boards). Tests E-I, further down, cover the VHF/AX.25 binding
(a Digirig and a radio sending, an RTL-SDR receiving). They are independent; do either first.

Goal: show that a compressed frame sent by one radio is received and expanded to exactly the same
text by another, for a range of packet types, before proposing the code to anyone.

Status of the code under test: the firmware fork builds and the codec passes a PC round trip and
agrees byte-for-byte with the C reference. **Compressed frames have now crossed the air once, from an
ESP32 sender to a Direwolf receiver (24 October 2026, see "Result of the first over-the-air run" below).**
The checklist tests A-D (two boards) and E-I (VHF) have not been run as written.

## What you need

* Two boards with the same LoRa module and settings, a serial cable for each, and the firmware fork
  (`feature/order3-compression` of the iGate repo). Use spare units, not a live station.
* PlatformIO. Serial monitor at 115200 baud.
* The same frequency, spreading factor, bandwidth and coding rate on both. Short range, low power.

## Before touching hardware

1. `make test` in this repo: reference codec against the vectors (expect `26 vectors, 0 failures`).
2. In the firmware fork, build and run the host self-test list:

       c++ -std=c++17 -O2 -I lib/AprsCompress/src tools/aprs_compress/test_selftest_host.cpp \
           lib/AprsCompress/src/AprsCompress.cpp lib/AprsCompress/src/AprsCompressModel.cpp \
           lib/AprsCompress/src/AprsCompressSelfTest.cpp -o test_selftest_host && ./test_selftest_host

   Expect 14 lines ending `all ok`. Packet #13 stays plain by design (it is high-entropy text).

## Test A: self-test mode (fixed packets, automatic check)

Build two firmwares from the same source:

    # unit 1: sends a test packet every 20 s and checks what it receives
    PLATFORMIO_BUILD_FLAGS="-DAPRS_PPM_SELFTEST -DAPRS_PPM_SELFTEST_TX=20" pio run -e <your-env> -t upload
    # unit 2: only checks what it receives
    PLATFORMIO_BUILD_FLAGS="-DAPRS_PPM_SELFTEST" pio run -e <your-env> -t upload

Config on both (web page or `igate_conf.json`): your callsign set; `aprs_is.active` off; digipeater
off; `lora.rxActive` on; `lora.txActive` on for unit 1. Test packets are recognised by their ending
(`~NN`), checked, and then dropped. They never reach APRS-IS, the digipeater, KISS or MQTT.
The positions in them are made up; keep APRS-IS off anyway.

Frame types: with `compressTx` forced on, the sender alternates full and primed frames (`compressFullEvery` 2,
the default), because all test packets share the unit's callsign. A primed frame refers to the last full or
plain packet, which the receiver has only if it received it. If one frame is lost on the air, the primed frame
that refers to it is dropped (`Primed frame dropped: no matching reference`); that is a radio loss, not a bug.

Expected on unit 1: `SELFTEST TX #0 (compressed)`, `#1`, ... through #13, then the same 14 again
with `(plain)`. Expected on unit 2, for each: a line like (`compressed` or `primed` after the number)

    SELFTEST RX OK #3 compressed: 54 B on air, 77 B as plain frame | ok 4 (compressed 4, plain 0), bad 0

**Pass:** after two full passes (28 packets, about 10 minutes at 20 s), unit 2 reports `bad 0`, the
compressed lines show fewer bytes on air than plain, and packet #13 arrives as `plain`.

**Fail signs**
* `SELFTEST RX MISMATCH ...` - the text arrived but is not what was sent: a decode bug or the two
  units hold different model tables (check both were built from the same commit).
* Nothing received at all - radio settings differ, or units are too far apart.
* `Compressed frame: ... B on air` appears on receive but no `OK` line follows - the expanded text
  did not match; read the line printed just above it.
* A packet that exists on one side only is a radio loss, not a codec bug; count sent vs received.

## Test B: normal firmware, real beacons

Flash a normal build (no test flags) and set `"compressTx": true` on unit 1 only. Let it send its
normal beacon and a message. Compare, for several packets, the two serial logs:

* unit 1: `---> LoRa Packet Tx : <text>` and `Compressed frame: N B instead of M B`
* unit 2: `Compressed frame: N B on air, M B expanded` and `<--- LoRa Packet Rx ... : <text>`

**Pass:** the transmitted text and the received text are identical, character for character, and N
is smaller than M. Also check the packet shows up wherever the receiver forwards it (map, web page,
APRS-IS if you enable it for this step).

## Test C: weak signal

Reduce power or add distance until some packets fail. Corrupted frames should be dropped by the
LoRa CRC (`CRC error!`), never shown as garbage text. If a compressed frame ever produces text that
is not what was sent, that is a bug in the codec's handling of bad input and needs a report.

## Test D: mixed network

Leave a third unit on stock firmware (or the fork with `compressTx` off) listening. It should log
nothing for compressed frames and keep working with plain ones. This is the "safe to leave on" claim.

## VHF / AX.25 binding (tests E-I)

Goal: show that an AX.25 UI frame with a compressed info field, sent by a real radio, is decoded by an
independent receiver to exactly the text that was sent, that ordinary plain frames are unaffected,
and that a stock decoder ignores the compressed frames harmlessly.

Status: **the Direwolf code for this does not exist yet.** Spec 07 describes the binding but leaves
the marker undecided (a distinct PID or bytes at the start of the info field). Choose one and build
the Direwolf branch before running these; write the choice into spec 07 and record it in the results.
Do not use K6ATV-10 ("DireRasp") or any live station for this.

### What you need

* A Pi (spare, not the live iGate) running the Direwolf branch with the info-field compression.
* A Digirig Mobile with the cable for the radio, and a VHF radio. Direwolf `ADEVICE` points at the
  Digirig's sound device and PTT is keyed through the Digirig (usually the serial RTS line; confirm
  against the Digirig and Direwolf documentation).
* A receiver that is **independent of the transmitting Pi**: an RTL-SDR into a second Direwolf
  (possibly the stock build, possibly the branch). An example receive chain, to be adjusted against
  the Direwolf docs:

      rtl_fm -f <freq>M -s 24000 - | direwolf -c rx.conf -r 24000 -D 1 -

* A test frequency that is **not 144.390 MHz**, or a dummy load with low power. Transmit your own
  callsign; keep APRS-IS (IGATE) off on both ends; send few packets. Identify as the rules require.

### Before any RF (test E: software only)

1. `make test` in this repo (expect `26 vectors, 0 failures`).
2. With the branch built, generate packets to a WAV file with `gen_packets` and decode them with `atest`
   (both are in the Direwolf build). Use the 14 self-test templates plus a few plain packets.
3. **Pass:** every compressed packet decodes to the original info text; packets that were plain stay
   plain; the compressed info field is shorter than the plain one for all but the high-entropy
   template (#13), which must stay plain.

### Test F: transmit with a radio, receive on the SDR

Send the same set through the Digirig and radio at low power. On the receiver, log each decoded
frame's info text.

**Pass:** for every transmitted packet the received text equals the sent text, character for
character, and the airtime (frame length at 1200 baud) is shorter than for the plain version of the
same packet. Count sent versus received: a packet missing on one side is a radio loss, not a codec
bug.

### Test G: plain frames are untouched

Send ordinary plain APRS frames (compression off, and compression on for packets that do not get
smaller). **Pass:** they decode exactly as before and look like normal APRS to the receiver.

### Test H: what a stock decoder does with a compressed frame

(A no-radio version with Direwolf `atest` is in `docs/legacy-decoder-test.md`; this is the same question on real audio and other decoders.)

Run an unmodified Direwolf (and any other decoder you have, such as APRSdroid or aprx) on the
receiving side while compressed frames are sent. Note, per decoder: does it print the frame, show
garbage, treat it as a malformed packet, or ignore it; and does it try to gate it to anything.
**Pass:** none of them crash, and none put a compressed frame on APRS-IS as if it were readable text.
This result decides the marker: if a stock decoder misreads the in-band marker, prefer the PID.

### Test I: weak signal

Reduce power or add distance until some frames fail the AX.25 checksum. **Pass:** failures are
dropped by the FCS and never shown as text. A compressed frame that passes the FCS but decodes to
the wrong text is a codec bug; report it with the bytes.

Record each run in the table below, with the marker used, the radio, the baud rate and the receiver.

## Result of the first over-the-air run (24 October 2026)

This was not tests A-D as written (no second ESP32). It used a different pairing: an ESP32 sender with
the iGate firmware fork, and a Raspberry Pi running the Direwolf branch with the LoRa HAT driver and the
compressed-frame receive code, on a hill site about 2 miles away with line of sight. It answers the same
question as test B: does a compressed frame sent by one radio arrive and expand to exactly the same text?

**Setup.** Sender: TTGO LoRa32 V2.1-class board (433 MHz, hand-soldered SMA connector, large
outdoor antenna), 433.775 MHz, SF12, BW 125 kHz, CR 4/5, 20 dBm, sync word 0x12. Receiver: a Raspberry Pi
with an SX1262 LoRa HAT running a separate build of the Direwolf branch (`feature/lora-compression`, built in
its own folder), same radio settings. The test sender used a build option (`APRS_PPM_SELFTEST`
with `APRS_PPM_ONESHOT` or `APRS_PPM_POSTEST`) that sends a fixed list of packets with a 30 s spacing
and then stops.

**Precautions, because the receiver belongs to a live iGate.** The live Direwolf service was stopped for
the length of each run (10 minutes) and restarted automatically afterwards; the test configuration had
the iGate server login switched off; every test packet carried `RFONLY` in its path; the first run used
status packets with no position at all; and the position run sent compressed frames only (a plain
position would be readable by every LoRa iGate in range). A bridge that forwards unparsed LoRa lines to a public map was
switched off for the first transmissions. All positions are made up (about 34 N 118 W).

**Run 1: status packets** (`K6ATV-14>APLRG1,RFONLY:>PPM link test N`, 16 packets, 4 plain, 8 compressed, 4 plain).
Every packet decoded to the exact text sent. Primed frames were 9-10 B on air against 42 B as a plain frame,
full frames 29 B. Signal -68 to -73 dBm, SNR 4-9 dB.

**Run 2: average-sized position packets.** The size of the test packet came from the capture of real LoRa APRS
traffic (25,403 position packets in 3 hours): comment mean 30.6 characters, median 27, 14 % with no comment,
payload mean 45.8 B. Test packet: an uncompressed position with a 29-character comment and a battery reading that
changes every packet (`K6ATV-14>APLRG1,RFONLY:=3412.34N/11823.45W>LoRa APRS tracker, batt 3.92V`, 75 B as a plain
frame including the 3-byte LoRa header). 12 packets, 30 s apart, positions moving slowly, compressed only
(full and primed alternating, `compressFullEvery` 2).

| Frame type | Packets | On air (B) | Plain frame (B) | Smaller than plain |
|---|---|---|---|---|
| Full | 6 | 38, 39, 38, 39, 38, 36 (average 38.0) | 75 | 49 % |
| Primed | 6 | 14, 13, 13, 15, 14, 14 (average 13.8) | 75 | 82 % |
| Alternating pair | 12 | average 25.9 | 75 | 65 % |

All 12 decoded to exactly the text sent (position digits and battery value included). The receiver's
reported on-air size equalled the sender's computed size for every packet. Signal -68 to -72 dBm,
SNR about 5-6 dB. The 12 packets used 311 B on air against 900 B as plain frames.

**What a stock decoder did.** Before the new receive code was installed, the unmodified Direwolf on the
same Pi logged each compressed frame as `loraspi: failed to parse: <binary bytes>  RSSI=... SNR=...` and carried on. It did not
crash and did not gate the frame. It only logs: any tool that reads those log lines and forwards them
(a public LoRa map feed in this case) needs to reject binary lines; we did not test that tool.
A side effect worth knowing: a compressed frame holds no callsign text, so searching a log for the sender's
callsign does not find it; search for `failed to parse` and the time instead.

**Not covered yet.**
* One sender, one receiver, a strong clean link. No weak-signal run (test C), no lost-frame run, so the
  "primed frame dropped when its reference was lost" path has not been seen on air.
* No second ESP32 receiver (tests A and B as written), no mixed-network run (test D).
* Packets were fixed, made-up text, not live traffic from several stations; a primed frame depends on the
  previous packet of the same station, so the 82 % figure for primed frames is the best case for
  near-identical consecutive packets. The measurement on real packets was 21 B primed, 33 B full against
  60 B plain (66 % overall in simulation).
* The VHF binding (tests E-I) is untouched.

The second session (below) closes the gap on the Direwolf transmit side and on a different receiver implementation;
the others in this list remain open.

## Result of the second over-the-air session: Direwolf transmits, ESP32 receives (24 October 2026, evening)

The first session sent from an ESP32 to Direwolf. This one reverses the direction: the Direwolf LoRa driver
(branch `feature/lora-compression`, built on the Pi) transmits compressed frames, and an ESP32 running the iGate
firmware fork decodes them. The two programs share no code and were written from the same format description,
so agreement here is a check of the specification as well as of the code.

**Setup.** Transmitter: the same Raspberry Pi and SX1262 HAT as before, 433.775 MHz, SF12, BW 125 kHz, CR 4/5,
test configuration with `LORACOMPRESS 2` and the station beacon every 2 minutes (the live service was stopped for the
10-minute run and restarted by the script). Receiver: the same TTGO LoRa32 as before, normal firmware (no test flags),
`compressTx` off, transmit off, CRC checking on. The packet is a standard LoRa APRS station beacon of 54 characters
(`<callsign>>APDW19:!<position>&<callsign> LoRa APRS`), 57 B as a plain frame including the 3-byte LoRa header.

| Time | Frame | On air | Plain would be | Decoded on the ESP32 | RSSI / SNR |
|---|---|---|---|---|---|
| 16:39:14 | full | 25 B | 57 B | identical text | -89 dBm / 11.0 dB |
| 16:41:13 | primed | 8 B | 57 B | identical text | -96 dBm / 10.0 dB |
| 16:43:14 | full | 25 B | 57 B | identical text | -89 dBm / 11.0 dB |
| 16:45:13 | primed | 8 B | 57 B | identical text | -96 dBm / 10.5 dB |
| 16:47:14 | full | 25 B | 57 B | identical text | -89 dBm / 10.8 dB |

All 5 beacons decoded, with no errors. Full frames saved 56 %, primed frames 86 %, an alternating pair 71 %. The
beacon is identical every time, which is the best case for priming; the position-packet run above (moving
tracker, 29-character comment: full 49 %, primed 82 %, pair 65 %) is the more typical figure.

**A bug found by this run (and fixed): the Direwolf SX1262 driver transmitted with the LoRa payload CRC off.** Before
the test build, the ESP32 logged the plain hill beacon twice (16:18:23 and 16:30:54) as
`failed, code -24`, which is RadioLib's `LORA_HEADER_DAMAGED`. RadioLib returns that when the receiver has CRC
checking on and the packet header says there is no CRC (`SX127x.cpp`, "CRC is disabled according to packet header
and enabled according to user, most likely damaged packet header"). The ESP32 iGate firmware sets `setCRC(true)`,
so it dropped every beacon from that driver. A LILYGO T-Deck, a normal LoRa APRS receiver, decoded the same
beacon, so the problem is not universal. Receiving was not affected: the SX1262 follows the CRC flag in each
received header. The cause was one byte in the SX1262 packet parameters (two places in `loraspi.c`). Turning the
CRC on fixed it, in the plain case as well as the compressed one, and the fix is in the Direwolf pull request (commit
`ccc0f57`). The unmodified live binary still has the bug: when the service was restarted at the end of the run it
beaconed again at 16:49 and the ESP32 logged `failed, code -24` once more.

**Also checked in this session**
* `LORARXGAIN` (SX1262 receive gain: powersave, boosted, or alternating by UTC hour) works on the hardware; the noise-floor
  log line shows the mode in use (`rxgain=boosted` in an odd UTC hour).
* The ESP32 build is 88.3 % full on its 1.97 MB application partition (about 6 points of that is the 125 KB model).

## Result of the third over-the-air session: a T-Deck tracker, and the hill station as a gateway (24 October 2026, late evening)

The first two sessions used the ESP32 iGate firmware and Direwolf. This one adds a third program, the LilyGo T-Deck
Plus (ESP32-S3, SX1262, 433 MHz), running a port of the compression into the LoRa APRS Tracker firmware of a third party
(the port is a one-commit change: the codec library, a frame reader and writer in the radio code, and two settings).
It also upgrades the hill station to the Direwolf build with the compressed-frame code.

**Receive.** An ESP32 iGate sender (the first session's board) sent the 16-packet sequence of status packets with
`RFONLY` in the path (4 plain, 8 compressed, 4 plain). The T-Deck received **16 of 16** and expanded all 8 compressed
frames to exactly the text sent, with none dropped: 4 full frames of 29 B and 4 primed frames of 9 to 10 B, against 42
to 43 B as plain frames. These are the same sizes the Direwolf build reported for the same packets.

**Transmit.** With `compressTx` switched on in the T-Deck's settings, the tracker's real beacon code sent its start-up
beacon and five beacons from its beacon button. The ESP32 iGate firmware (CRC checking on, transmit off) decoded all 6
exactly:

| Beacon | Frame | On air | Plain would be | RSSI / SNR at the ESP32 |
|---|---|---|---|---|
| start-up (with comment) | full | 31 B | 75 B | -74 dBm / 13.3 dB |
| button 1 | primed | 15 B | 40 B | -78 dBm / 15.5 dB |
| button 2 | full | 23 B | 40 B | -75 dBm / 14.3 dB |
| button 3 | primed | 13 B | 40 B | -79 dBm / 14.0 dB |
| button 4 | full | 24 B | 40 B | -74 dBm / 14.3 dB |
| button 5 | primed | 15 B | 40 B | -80 dBm / 12.3 dB |

Full and primed frames alternated as designed, none were dropped, and the tracker's frames are 42 % (full) to 63 to
68 % (primed) smaller than the plain position beacon. The hill station, still on the old Direwolf, heard all six at
-95 to -98 dBm (SNR -4 to -7 dB) about 2 miles away with the T-Deck's small antenna, and could only log each as an
unreadable frame, as expected of a receiver without the code.

**The hill as a gateway.** The hill station was then switched to the new Direwolf build (compressed-frame receive;
its own transmissions stay plain). A further compressed beacon from the T-Deck (23 B on air, 40 B as text) was
expanded by the hill station and passed to APRS-IS **one second later** as an ordinary packet, with the hill station's
callsign in the `qAO` part of the path. A read-only APRS-IS login seen from outside showed it as a normal tracker packet.
So a tracker that compresses and one iGate that expands are enough: nothing downstream needs to know. This was
seen once; a long-running test with the T-Deck compressing and the hill upgraded is now under way.

**Also confirmed on the live hill station.** Before the upgrade the ESP32 rejected the hill's own plain beacon
(`failed, code -24`, header damaged, because the old driver transmitted without the payload CRC); three minutes after
the upgrade the same ESP32 decoded the hill's beacon (RSSI -88 dBm, SNR 12 dB).

**Not covered yet:** a long-running comparison; lost frames and the primed-frame-drop path on a marginal link (the path
was marginal for the hill, but every frame was received); other boards and other spreading factors; what other
trackers and iGates do with a compressed frame; and the VHF binding.

## Long-running test: a T-Deck beaconing compressed to the hill gateway (started 24 October 2026)

After the third session the T-Deck tracker was left with compressed transmit switched on, and the hill station on the new
Direwolf build (reads plain and compressed frames, sends plain, passes what it hears to APRS-IS). The T-Deck sends its
beacon every 15 minutes by itself. This section is updated as the test continues; the figures come from the gateway's own
log, which counts every frame it receives.

**First 1 hour 45 minutes (19:54 to 21:39 local time, 8 beacons):**

| Time | Frame | On air | Plain would be |
|---|---|---|---|
| 19:54:40 | full | 23 B | 40 B |
| 20:09:42 | primed | 19 B | 40 B |
| 20:24:44 | full | 23 B | 40 B |
| 20:39:46 | primed | 19 B | 40 B |
| 20:54:49 | full (the beacon that carries the comment) | 33 B | 75 B |
| 21:09:51 | primed | 19 B | 40 B |
| 21:24:53 | full | 23 B | 40 B |
| 21:39:55 | primed | 14 B | 40 B |

* **8 beacons received out of 8.** The gaps between them are 902 to 903 seconds every time (the 15-minute timer plus the
  transmit time), so none was missed: a missed beacon would show as a gap of about 30 minutes.
* **173 B on air against 355 B as plain frames: 51 % smaller.** Full frames averaged 25.5 B (48 % smaller), primed frames
  17.8 B (56 % smaller). Full and primed frames alternated as designed.
* **Each beacon reached APRS-IS in the same second it was received,** as an ordinary packet with the gateway's callsign in
  the `qAO` part of the path (checked for the 19:54 and 21:39 beacons with a read-only APRS-IS login).
* **A primed frame dropped, as designed.** An ESP32 listener restarted a minute before the 21:39 beacon had not heard the
  full frame it refers to, so it dropped the primed frame (`Primed frame dropped: no matching reference`). The gateway, which
  had heard the full frame, decoded it. The listener recovers at the next full frame.

**Overnight, 10 hours 37 minutes (19:54 on 24 October to 06:31 on 25 October, 40 beacons received):** the gateway's own
summary of its log.

* **40 beacons received. About 43 were due** (10 h 37 min at one beacon per 902 s), so about 93 % arrived and 3 did not.
* **50 % smaller:** 844 B on air against 1,705 B as plain frames (861 B saved). Full frames (21) averaged 24.2 B
  (46 % smaller); primed frames (19) averaged 17.7 B (56 % smaller).
* **The gaps:** median 902 s; two gaps over 1.5 times the median, the longest 2,707 s (three beacon intervals, so two
  beacons in a row are missing). The gateway also logged **one primed frame dropped for lack of a reference.**
* **Reading of the missing ones (an inference, not something the log states):** a lost full frame also costs the primed
  frame after it, because that one is coded against the full one. A primed frame that was heard but dropped fits the
  longest gap: the full frame before it was not heard, so the primed one could not be decoded, and the next full frame
  recovered. That would be two of the three missing beacons. The third is a single gap with no cause recorded.
* **No plain control ran in parallel,** so this does not show whether the same three would have been lost as plain
  frames. A lost full frame costing two beacons instead of one is a real cost of the priming design; it is why a full frame
  is sent at least every second packet.

**A full day, 23 hours 57 minutes (19:54 on 24 October to 19:51 on 25 October, 80 beacons expanded):** the gateway's own
summary of its log, covering the overnight run above and the following day.

* **80 beacons expanded and gated. About 96 were due** (one every 15 minutes). Another **6 primed frames arrived and were
  dropped** because their full frame had not been heard, so about 86 were heard and about 10 never were.
* **51 % smaller:** 1,736 B on air against 3,515 B as plain frames. Full frames (43) averaged 24.9 B (47 % smaller);
  primed frames (37) averaged 18.0 B (55 % smaller).
* **The gaps:** median 902 s, 10 gaps over 1.5 times the median, the longest 4,510 s (five beacon intervals).
* **The daytime was rougher than the night.** From 06:46 on 25 October the collector (the MQTT server that receives the
  hill's event log) shows 45 compressed frames heard in 13.5 hours with 5 primed frames dropped, against 1 dropped overnight.
  The hill's noise floor averaged -92.7 dBm in that period, with 35 CRC failures. Whether that follows the gateway's
  gain setting, which alternates by hour, has not been checked.
* **A lost full frame costs the primed frame after it,** which the dropped frames above show. That is why a full frame
  is sent at least every second packet.

**What this does not show:** one link, a signal at the gateway of about -95 to -98 dBm (measured in the earlier session),
no change in weather or season, and one day. Primed frames here are larger than in the earlier
button-press runs (14 to 19 B against 13 to 15 B) because the beacons are 15 minutes apart and the reported position changes
between them.

## Record results

| Date | Units / frequency / SF (or radio, baud, marker) | Test | Packets sent | OK | Mismatch | Notes |
|---|---|---|---|---|---|---|
| 2026-10-24 | ESP32 sender to Pi (Direwolf branch), 433.775 MHz SF12 BW125, 2 mi LOS | status packets, 4 plain + 8 compressed + 4 plain | 16 | 16 | 0 | primed 9-10 B vs 42 B plain |
| 2026-10-24 | same | average position + 29-char comment, compressed only | 12 | 12 | 0 | full 38.0 B, primed 13.8 B, pair 25.9 B vs 75 B (65 %) |
| 2026-10-24 | Direwolf (Pi, SX1262) to ESP32 iGate firmware, same radio settings | station beacon, full and primed | 5 | 5 | 0 | full 25 B, primed 8 B vs 57 B plain; CRC-off transmit bug found and fixed first |
| 2026-10-24 | ESP32 iGate firmware to T-Deck tracker (SX1262), same radio settings | status packets, 8 compressed | 16 | 16 | 0 | full 29 B, primed 9-10 B vs 42-43 B plain |
| 2026-10-24 | T-Deck tracker to ESP32 iGate firmware, real beacon code | position beacons, full and primed | 6 | 6 | 0 | full 23-24 B, primed 13-15 B vs 40 B (start-up beacon 31 B vs 75 B) |
| 2026-10-24 | T-Deck to hill station running Direwolf, then to APRS-IS | one compressed beacon | 1 | 1 | 0 | expanded and gated as a normal packet 1 s later |
| 2026-10-24, 19:54-21:39 | T-Deck beacon timer (15 min) to the hill gateway, long-running | position beacons, full and primed | 8 | 8 | 0 | 173 B vs 355 B plain (51 %); first hours of a continuing test |
|  |  |  |  |  |  |  |

## When it passes

LoRa: open a PR to richonguzman/LoRa_APRS_iGate: receive always on, transmit off by default, the test
mode as a build option, and a link to this repo for the format details. Record the results above
and mention them in the PR.

VHF: the Direwolf change is a separate PR after #638 lands, raised with the maintainer first, and it
should not be mixed into #638. Link this repo and the results.
