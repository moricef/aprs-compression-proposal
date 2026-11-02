# LACF test results, 31 October to 2 November 2026

Measurements from running LACF (spec 11, kinds 2 and 3) on real boards, written down so they are not lost in terminal output. Every number comes from a log or a
build report; the source is named next to it. Where a figure is from memory of an earlier check and was not repeated, it is listed under "Not re-verified".
These are single runs on one set of boards, not a survey. Callsigns are the author's own stations (K6ATV-4 T-Deck, K6ATV-10 hill Dire Wolf, K6ATV-14 LoRa32 iGate).

## 1. Frame sizes on the air

"Plain" is the size of the same packet as an ordinary LoRa APRS text frame, as each program reports it (the iGate adds the 3-byte `3C FF 01` header to the text; Dire Wolf's
"instead of" figure adds its 3-byte preamble to the text length; I did not check how the T-Deck counts it). Saving is (plain - LACF) / plain.

| When | Sender, receiver | Packet | LACF | Plain | Saving | Source |
|---|---|---|---|---|---|---|
| 1 Nov 16:23 | hill Dire Wolf, sending | beacon | 39 B | 57 B | 32% | hill journal, `TX hybrid frame: 39 B instead of 57 B` (also at 15:28 and 15:58) |
| 1 Nov | LoRa32 iGate, sending | its own beacon | 29 B | 50 B | 42% | serial log, `LACF frame: 29 B instead of 50 B` |
| 1 Nov, after the browser Update | T-Deck, sending | its own beacon with a comment | 58 B | 89 B | 35% | serial log, `Hybrid Tx 58 bytes (text frame 89 bytes)` |
| 1 Nov, hill receiving | frames from the other stations | various | 25 B / 29 B / 58 B | 40 B / 50 B / 89 B | 38% / 42% / 35% | hill journal, `hybrid frame: N B on air, M B as a plain frame` (9 frames between 15:26 and 16:25) |
| 31 Oct 17:47 | LoRa32 iGate sending, T-Deck receiving (5 frames, all decoded) | 3 with position (kind 3), 2 without (kind 2) | 52 / 37 / 58 (with RXT block) / 52 / 39 B | 97 / 70 / 97 / 97 / 70 B | 46% / 47% / 40% / 46% / 44% | `hybrid-onair-lora32.txt` and `hybrid-onair-tdeck.txt` (the T-Deck's `Hybrid frame N bytes, M expanded` matches each send) |

What this says and does not say: LACF frames were 32 to 47% smaller than the same packet as plain text, on a handful of beacons and status packets. The size depends on the
packet (a beacon with a long comment gains less than a bare position), so these are examples, not an average. The longer-run estimate for real traffic is in `hybrid-savings-and-timing.md`.

## 2. Firmware size

PlatformIO's own report at the end of each build. The app slot is 1,966,080 bytes on the LoRa32 and the ESP32-C3 build, 3,145,728 bytes on the T-Deck.

| Firmware | Flash used | RAM used | Source |
|---|---|---|---|
| Ricardo Guzman's iGate `main` (1591d9f), LoRa32 | 1,602,737 B (81.5%) | 56,376 B (17.2%) | fresh clone build, 1 Nov |
| the same plus only the `btStop()` removal (pull request #476) | 1,474,561 B (75.0%) | 52,548 B (16.0%) | build of `main` with that one patch, 1 Nov |
| LACF iGate `88964d7`, LoRa32 | 1,797,625 B (91.4%) | 68,588 B (20.9%) | rebuild of the published commit, 1 Nov |
| LACF iGate `88964d7`, ESP32-C3 (single core) | 1,782,746 B (90.7%) | 61,252 B (18.7%) | rebuild, 1 Nov |
| LACF tracker `be49724`, T-Deck Plus | 2,453,273 B (78.0%) | 228,848 B (69.8%) | build log of the published tracker, 1 Nov |

* The published iGate update file is 1,804,336 bytes (91.8% of the slot), a little larger than the PlatformIO figure because the file includes the image header. The two numbers
  describe the same program. A clean rebuild of the same commit does not give identical bytes, so a rebuild may differ from the published file; that is why the published
  file was verified by flashing it and reading it back, not by rebuilding.
* The `btStop()` removal alone frees about 128 KB of flash (6.5 points of the slot) and 3.8 KB of RAM on the LoRa32. The LACF iGate is about 330 KB larger than `main` with
  that removal: the compression models, the LACF frame code, the MQTT event feed and the other additions of the branch. About 160 KB of the slot is left.
* Frame-code speed and stack use (LoRa32 and T-Deck) are in `hybrid-savings-and-timing.md`, not repeated here.
* These are the 1 November builds. The earlier whole-text compressed format was removed from both firmwares on 2 November 2026; the sizes of those builds, against the originals, are in section 7.

## 3. iGate main loop with and without the fixes

LILYGO LoRa32 V2.1, WiFi and APRS-IS connected. Each firmware was built with a small temporary printout of the longest main-loop pass in each minute and the number of passes
(not part of any pull request). The first minute after boot is slower for every build; the figures below are for steady minutes unless a first-minute value is given. Logs: `T1` to `T5` in the job's test folder, 1 Nov.

| Case | Longest pass in a steady minute | Passes per minute |
|---|---|---|
| Ricardo's `main`, MQTT off | 1,035 to 1,036 ms | about 1,650 |
| `main` plus the APRS-IS read fix (read to `\n`, not `\r`) | 35 ms | 1,824 |
| `main`, MQTT broker unreachable (packets dropped) | 4,043 to 4,044 ms (7,511 ms in the first minute) | 16 to 19; 63 connect attempts in 215 s |
| `main` plus the background MQTT connect, same broker | about 1,033 to 1,035 ms (the APRS-IS stall above) | 1,656 to 1,732 (1,311 in the first minute); 4 connect attempts in 240 s (back-off 15, 30, 60, 120 s) |

With the broker reachable, the background connect completes and subscribes as before.

Settings-file damage, same board (`main` against the safe-save branch, which was not sent as a pull request):

| Damaged file | `main` | safe-save branch |
|---|---|---|
| empty | NOCALL-10, no WiFi networks, access-point mode; file rewritten with defaults | files left untouched, runs on defaults in memory |
| cut after 150 bytes | callsign lost (NOCALL-10), networks kept; file rewritten | left untouched |
| cut at half (1,058 of 2,117 bytes) | callsign and both networks kept | not tested |

## 4. Conformance and test counts

| Check | Result |
|---|---|
| Encode vectors (`vectors_hb_v1.txt`) against Dire Wolf's frame code | before the update 8 of 553 failed; after 0 of 553 (16 of the vectors are packets that must not encode) |
| Rejection vectors (`vectors_hb_reject_v1.txt`) | before 3 of 31 wrongly accepted; after 0 |
| Relay vectors, first hop and later hops | 298 and 199, 0 failed (unchanged) |
| Fault-injection comparison C against Python, `xcheck_hb.py test/hb_packets.txt 150 test/vectors_hb_relay_v1.txt` | 142,282 frames compared, as run during the review |
| iGate host tests | `test/lacf` 277 checks, `test/configstore` 123, `test/mqttdefaults` 10 cases, `test/events` passes; 0 failures |
| Dire Wolf `lorahybrid_test` | 322 checks, 0 failed (11 fail on the code before the WIDE update) |
| Browser Update button | tested on the LoRa32 and the T-Deck: program identical to the published file, settings unchanged |

## 4b. Held-out stations, current encoder (1 November 2026)

The held-out set of the earlier size study was rebuilt with `tools/hybrid/prep.py --test-buckets 3` from the 24-hour APRS-IS capture (93,145 packets from 1,669 stations; the same counts as before) and encoded with the current reference encoder (`tools/hb`, spec 11 frame with `V` and the CRC-16, table set 1, no RXT). The packets hold real callsigns and are not published; only the totals are.

| Measure | Result |
|---|---|
| Packets encodable as LACF | 92,832 of 93,145 (313 would be sent as plain frames) |
| Kind 3 (position block) / kind 2 | 77,193 / 15,639 |
| Mean plain frame (3-byte prefix plus text) | 81.3 B |
| Mean LACF frame (plain where not encodable) | 47.2 B |
| Bytes saved | **41.9%** (42.1% on the encodable packets alone) |
| The harness figure for the same packets (without `V` and CRC-16) | 46.2% |

The 4.3-point difference to the harness figure is what spec 11 estimated for the 3 bytes of `V` and CRC-16. The tables were trained only on the other stations of the same capture (callsign-hash buckets 3 to 9; the test stations are buckets 0 to 2), so these stations were not in training (`tools/hybrid/tables/README.md`).

## 5. Not re-verified

These were observed earlier in this work and noted from the collector's reports. They were not repeated and no log is attached here.

* The LoRa32 on the rooftop antenna hears the hill at about -89 dBm (SNR +11 dB), against about -103 dBm (SNR -17 dB) before; the noise floor was about -118 dBm.
* The hill hears the LoRa32's LACF frames at about -69 dBm.

## 6. Not measured

* A frame with a non-compliant WIDE path arriving over the air. The vectors cover it; none has been heard on the hill since the update.
* Broken (CRC-failed) frames arriving at the collector after the forwarder stopped sending their raw bytes.
* Frame-code timing and stack use for the T-Deck with a WiFi client connection and APRS-IS traffic.
* Battery or power draw of any LACF build.

## 7. 2 November 2026: the earlier whole-text format removed

No station sends the earlier whole-text frames (`3C FF 81`, `3C FF C1`) any more, so the iGate and the T-Deck tracker no longer read or send them: the codec and its packet stores were deleted (iGate commits `6c9ac2d` and `68e1770`, tracker `01d9cb2`). Settings files are unchanged: `compressTx` and `compressFullEvery` are still read and written, and do nothing. A frame of that kind now falls into the "unknown `3C FF` type" branch and is dropped. The builds below were flashed to the boards on 2 November (program only, settings kept) and are the files on the firmware page.

### Sizes against the originals

PlatformIO's own report. Flash is the program in its slot, RAM is the fixed RAM; RAM does not include memory taken while running (the LACF frame code takes a 16 KB task stack from the heap at its first use). The originals were built the same way: Ricardo's iGate `main` (1591d9f, fresh clone build of 1 November) and Fabrice's tracker `devel` (83dfb4a, unmodified, built 2 November).

| Board | Build | Flash | RAM |
|---|---|---|---|
| LoRa32 V2.1 (slot 1,966,080 B, RAM 327,680 B) | original | 1,602,737 B (81.5%) | 56,376 B (17.2%) |
| | this build (`68e1770`) | 1,664,817 B (84.7%) | 53,860 B (16.4%) |
| | this build against the original | 62,080 B more (+3.9%) | 2,516 B less (-4.5%) |
| | room left | 301,263 B (15.3%) | 273,820 B (83.6%) |
| T-Deck Plus (slot 3,145,728 B, RAM 327,680 B) | original | 2,151,949 B (68.4%) | 213,216 B (65.1%) |
| | this build (`01d9cb2`) | 2,321,553 B (73.8%) | 214,136 B (65.3%) |
| | this build against the original | 169,604 B more (+7.9%) | 920 B more (+0.4%) |
| | room left | 824,175 B (26.2%) | 113,544 B (34.7%) |

Against the 1 November builds of section 2: the LoRa32 build is 132,808 B smaller in flash (91.4% to 84.7%) and uses 14,728 B less RAM; the T-Deck build is 131,720 B smaller and uses 14,712 B less RAM. The ESP32-C3 iGate build (`ESP32C3_SuperMini_DIY_LoRa_SX1278`) is 1,654,510 B (84.2%) and 46,468 B of RAM, from 1,782,746 B (90.7%) and 61,252 B. So the iGate needs less RAM than the original, but not less flash; the tracker needs more of both than the build it is based on.

### On the LoRa32, with WiFi, APRS-IS and MQTT connected

Source: three serial captures of 1.5 to 8 minutes, 2 November. Opening the serial port resets the board, so each capture begins with a boot and a beacon.

| Measure | Result |
|---|---|
| Longest main-loop pass, first minute after boot | 2,695, 2,585 and 2,564 ms (1,372, 1,213 and 1,369 passes) |
| Longest main-loop pass, five steady minutes | 48, 109, 108, 118 and 133 ms; 1,676 to 1,717 passes a minute |
| Own beacon | `LACF frame: 29 B instead of 50 B`, once at each boot (three captures); the hill Dire Wolf heard four of its beacons as `hybrid`, 29 B, between 14:12 and 14:29 UTC |
| Frames received and gated to APRS-IS, no failures logged | 3 standard LoRa APRS frames (K6ATV-4 at 14:29:48, K6ATV-22 at 14:31:56, K6ATV-9 via K6ATV-22 at 14:32:39 UTC) and the test below |

### Receive test, 8 frames (2 November, 06:43 to 06:46 PST)

K6ATV-4 (T-Deck) sent four standard LoRa APRS frames and then four LACF frames; the LoRa32 was listening on its serial log. All eight were decoded to the packet text and uploaded to APRS-IS; none was logged as dropped.

| Frame | Time (PST) | RSSI / SNR |
|---|---|---|
| standard LoRa APRS | 06:43:56, 06:44:09, 06:44:21, 06:44:31 | -81 to -98 dBm / 10.0 to 13.75 dB |
| LACF 58 B on air, 89 B expanded | 06:45:16 | -76 dBm / 14.25 dB |
| LACF 25 B on air, 40 B expanded | 06:45:30, 06:45:43, 06:45:52 | -88 to -91 dBm / 11.75 to 15.0 dB |

The serial log prints a format line only for an LACF frame; a frame with no such line is a standard LoRa APRS frame (the collector calls that class `plain`). That this is true was confirmed for the first such frame of the day (14:29:48 UTC, 40 B), which the collector stored as class `plain`; the four test frames were not looked up in the collector.

### Host tests

`test/lacf` 304 checks, `test/configstore` 123, `test/mqttdefaults` 10 cases, `test/events` passes; 0 failures. The iGate builds for the LoRa32 and the ESP32-C3.

### Not measured

* An earlier-format frame arriving on the air: nobody sends one, so the drop path is covered by the host tests only.
* The T-Deck's Web-Conf access point: after the 2 November build (the plain `WiFi.mode(WIFI_AP)` call, and the earlier whole-text codec removed) the author joined it many times with no failure (the number was not counted). The same joins failed on the 1 November build, and an unmodified upstream build joined too; memory is the likely cause but is not proved. Treated as closed.
* The browser Update button on these builds (tested on the 1 November files).

## 8. 2 November 2026: the relay with RXT on the air

The relay build (iGate `27dbbfb`, tracker `d9c4f5b`) was flashed to the LoRa32 and the T-Deck (program only, settings kept) and run on the air at 433.775 MHz, SF12, a few metres apart. Times are PST. Source: serial logs of both boards. The iGate's digipeater was set to WIDE2 + WIDE1, the T-Deck's repeater to the WIDE1-1 fill-in alias, Tx frame format LACF on both.

| Time | Origin | Relay | Relay heard it at | Relayed frame | Time to hop |
|---|---|---|---|---|---|
| 08:23:55 | T-Deck beacon, 58 B, `WIDE1-1` | LoRa32 | RSSI -71 dBm, SNR 8.5 dB | **67 B** (+9) | 3,467 ms |
| 08:24:24 | T-Deck beacon, 25 B | LoRa32 | RSSI -81, SNR 12.75 | **34 B** (+9) | 2,157 ms |
| 08:29:03 | T-Deck beacon, 58 B | LoRa32 | RSSI -81, SNR 12.75 | **67 B** (+9) | 3,338 ms |
| 08:30:00 | LoRa32 beacon, 29 B | T-Deck | not logged by the T-Deck (the LoRa32 heard the relay at RSSI -81 dBm, SNR 10.75) | **38 B** (+9) | 2,588 ms |

* **+9 bytes** is the path (`WIDE1-1`, one byte, becomes `K6ATV-14*` or `K6ATV-4*`, five bytes: +4) and the first tuple (4 bytes and the mask byte: +5), as the spec says. A relayed standard text frame grows by 2 bytes; this one is 67 B against 91 B as text.
* **Each board decoded the other's relay.** The LoRa32 logged `LACF frame: 38 B on air, 51 B expanded (1 RXT tuple)`; the T-Deck logged `Hybrid frame 67 bytes, 91 expanded` and showed the tuple as the text trailer `{LRk!@}`.
* **The values carried through.** For the 08:29:03 relay the trailer decodes to RSSI -81 dBm, SNR 13.0 dB, frequency offset -2,500 Hz and a time to hop of about 3,233 ms; the LoRa32 had logged RSSI -81 dBm, SNR 12.75 dB (0.5 dB steps), a frequency error of -3,324 Hz and a time to hop of 3,338 ms (the scale is exponential: index 31 reads 3,233 ms). The frequency offset sits at the end of its scale: ordinary crystal offsets on these boards (-3.2 to -3.4 kHz) are past it (see [`rxt-frequency-offset-scale.md`](rxt-frequency-offset-scale.md)).
* **No loop:** the LoRa32 did not relay its own beacon after the T-Deck's relay of it, and the T-Deck's relay of the LoRa32's frame was not repeated.
* **Host tests of the same code:** the iGate wrapper test (604 checks) and the tracker's googletest suite (11 cases) include a sample of the spec relay vectors; the reference passes all 298 + 199 + 8 in C and in Python.

### What the hill (Dire Wolf, K6ATV-10) logged for the same window

Source: the collector database, read-only query of 2 November 2026, 08:20 to 08:35 PST (16:20 to 16:35 UTC).

| Relay | Relayed frame at the hill | Logged by the hill |
|---|---|---|
| LoRa32, 08:23:55 | 67 B | **not logged** |
| LoRa32, 08:24:25 | 34 B, path `K6ATV-14*`, RSSI -67 dBm, SNR 6.0 dB | yes (08:24:27) |
| LoRa32, 08:26:54 | 67 B, RSSI -68, SNR 6.0 | yes (08:26:57) |
| LoRa32, 08:27:16 | 67 B, RSSI -67, SNR 5.2 | yes (08:27:19) |
| LoRa32, 08:29:03 | 67 B, RSSI -68, SNR 4.8 | yes (08:29:06) |
| T-Deck, 08:30:00 | 38 B, path `K6ATV-4*`, RSSI -96, SNR 0.5 | yes (08:30:02) |

* **The hill decoded every relay it heard**: class `hybrid`, outcome `ok`, 34, 67 and 38 bytes on the air, and the plain frame it would have been (42, 91 and 51 bytes). That is 5 of the 6 relays; the first LoRa32 relay is not in the log (cause not found: the hill heard the T-Deck's direct beacons at only -95 to -98 dBm, SNR from 1 to -11 dB, so it may simply have been at the edge, or the frame collided).
* **The relay brought the T-Deck's beacons to the hill at a useful level**: the LoRa32's relays arrived at -67 dBm, the T-Deck's own frames at -95 to -98 dBm.
* **Standard frame relayed as LACF:** at 08:31:59 the LoRa32 relayed a standard text beacon of K6ATV-22 as an LACF frame of 32 bytes (48 as text). It carries no tuple: a frame that was not LACF has none to extend. The hill decoded it but logged it as `parse_failed`, because the source's SSID is 22 and AX.25 cannot hold an SSID above 15 (the known Dire Wolf limit), not because of the relay.
* **Frequency offsets, three boards:** the hill hears the T-Deck at -51 to -62 Hz, the LoRa32 hears the T-Deck at -3.2 to -3.3 kHz, and the hill hears the LoRa32 at +2.8 to +3.0 kHz. So the hill and the T-Deck agree within 60 Hz and the LoRa32 is about 3 kHz (7 ppm at 433.775 MHz) away from both: the offset belongs to that board's crystal, not to every pair.

### Not measured

* **A frame through two relays.** One hop was run in each direction.
* **A packet near the 255-byte limit on the air:** the rule that leaves the tuple out is tested on the host only.
* **Busy channel, other spreading factors, other boards.**

