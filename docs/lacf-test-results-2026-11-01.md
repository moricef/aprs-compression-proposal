# LACF test results, 31 October to 1 November 2026

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
