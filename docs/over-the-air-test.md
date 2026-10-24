# Over-the-air test checklist

Tests A-D cover the LoRa binding (two boards). Tests E-I, further down, cover the VHF/AX.25 binding
(a Digirig and a radio sending, an RTL-SDR receiving). They are independent; do either first.

Goal: show that a compressed frame sent by one radio is received and expanded to exactly the same
text by another, for a range of packet types, before proposing the code to anyone.

Status of the code under test: the firmware fork builds and the codec passes a PC round trip and
agrees byte-for-byte with the C reference. **No compressed frame has crossed the air yet.**

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

Expected on unit 1: `SELFTEST TX #0 (compressed)`, `#1`, ... through #13, then the same 14 again
with `(plain)`. Expected on unit 2, for each: a line like

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

## Record results

| Date | Units / frequency / SF (or radio, baud, marker) | Test | Packets sent | OK | Mismatch | Notes |
|---|---|---|---|---|---|---|
|  |  |  |  |  |  |  |

## When it passes

LoRa: open a PR to richonguzman/LoRa_APRS_iGate: receive always on, transmit off by default, the test
mode as a build option, and a link to this repo for the format details. Record the results above
and mention them in the PR.

VHF: the Direwolf change is a separate PR after #638 lands, raised with the maintainer first, and it
should not be mixed into #638. Link this repo and the results.
