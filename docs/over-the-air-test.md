# Over-the-air test checklist

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

## Record results

| Date | Units / frequency / SF | Test | Packets sent | OK | Mismatch | Notes |
|---|---|---|---|---|---|---|
|  |  |  |  |  |  |  |

## When it passes

Open a PR to richonguzman/LoRa_APRS_iGate: receive always on, transmit off by default, the test
mode as a build option, and a link to this repo for the format details. Record the results above
and mention them in the PR.
