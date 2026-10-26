# Known issues and findings from testing

Findings from people who tested the code, and what was done about them. Each is listed with its status.

## From an independent tester (26 October 2026)

A tester with a LoRa APRS network of their own built the standalone codec, unmodified, into a T-Deck tracker and an iGate on a TTGO LoRa32 and tested
it on the air at SF12 on 433.775 MHz and offline. They kept the primed frame off, for the first reason below.

### 1. A primed frame could decode to the wrong text (fixed in the reference codec)

A primed frame is coded against the sender's last full packet, and the receiver picks its stored reference with an
8-bit CRC. A receiver that missed a reference update still holds the old packet; if that old packet has the same 8-bit
check as the real one, the decoder ran with the wrong reference. The only later check covered the source callsign, so
it could return success with the wrong text from a frame that arrived intact.

They measured this on 1,316 collision cases built from synthetic packets: 619 decoded to the wrong text. The same
test here on 2,056 cases gave 655 (32%). The reference check is one chance in 256 to collide, so in real use the rate
is a few tenths of a percent of the events where a reference update was missed. A wrong position reaching APRS-IS is
a bad failure even when it is rare.

**Fix.** The primed header gains a **text check**, one byte, a CRC-8 of the whole original text, checked after
decoding ([spec 08](../spec/08-priming-from-previous-packet.md)). It uses a different polynomial (0x1D) from the
reference check (0x07). The first attempt reused 0x07 and let 36 of 2,060 collisions through, because a stale
reference that collides on 0x07 tends to decode to text that differs from the true text by a pattern the same CRC
cannot see. With 0x1D the same test lets **6 of 2,060** through (0.3%), down from about 32%, and `make test` now runs
it (`build/ppm_tool collide`). It is a residual, not zero: a one-byte check lets about 1 in 256 of the remaining wrong
decodes through. A two-byte check would reduce that further at a cost of one more byte per primed frame.

**This breaks version 1 primed frames.** The new header is not compatible with the first one (7 bytes, was 6), and the model version in the frame mark (`C1`) did not change. Anyone running an early build must rebuild and reflash all devices together; a primed frame between an old and a new build is almost always dropped, and about 1 in 256 could decode wrongly (the weakness this fix closes). Full and plain frames are unaffected. The primed mode had only been run on our own test devices, so the number of early adopters is small.

**Cost.** One byte per primed frame, so about 2 points less saved on primed frames: the average primed frame in the
spec 08 measurement goes from 21.2 B (65% smaller) to about 22.2 B (63%). The figures elsewhere in this repository
for primed frames were measured with the earlier 6-byte header and are about 1 byte per primed frame too optimistic.
The layout change is a different primed header under the same `3C FF C1` mark. The three firmware branches (ESP32 iGate, T-Deck tracker, Dire Wolf) were updated to it on 26 October 2026, and **APRS-PPM 2.0.0** marks the break (Dire Wolf has a tagged pre-release and a start-up log line that names it).

**What has been tested since the change**

* **Host tests.** The codecs of all three pass the new vectors (103 primed vectors; the T-Deck branch has no test of its own, so the iGate's host test was run against the T-Deck's copy of the codec, which is byte-identical). Dire Wolf also has the unknown-frame test.
* **Builds.** The T-Deck firmware and the ESP32 iGate firmware (TTGO LoRa32 V2.1 target) both build with the change. The T-Deck was flashed with it and its settings were checked afterwards. The iGate firmware was built but not run.
* **Over the air, 26 October 2026.** A T-Deck running the 2.0.0 firmware was received by a Raspberry Pi with an SX1262 LoRa HAT running the 2.0.0 Dire Wolf build, at -98 dBm and an SNR of -6 to -7 dB, about 2 miles away. A full frame of 23 B (against 40 B as a plain frame, 43% smaller) arrived first, then a primed frame with the new header of 20 B (50% smaller, with the position having moved between the two). Both passed the check, were expanded and went on as ordinary packets. **Two frames on one link**, so this needs more testing.
* **Not yet done:** the new layout between the T-Deck and an ESP32 iGate, other spreading factors, a busy channel, and the ESP32 iGate firmware running with the change.

### 2. A cut-short frame decoded to some text (fixed for primed frames; open for full frames)

The decoder pads missing bytes with zeros, so a frame cut down to its length still decodes to some text. The LoRa CRC
protects against this on the air, but the decoder did not itself reject such frames. For primed frames the text check
now rejects them: of 263 primed frames cut by 1 to 3 bytes, none decoded to wrong text. Full frames (`3C FF 81`)
have no check byte, and adding one would cost a byte on every full frame, so they still rely on the LoRa CRC. Open: a **round-trip check**, where
the receiver decodes the frame, compresses the text again and drops the frame if the result differs from the bytes it
received. It adds nothing on the air and has not been tried. It would require every sender to produce exactly the same
bytes for the same text, or its good frames would be dropped. And it is not free on the receiver: on a LILYGO LoRa32
(ESP32-PICO-D4, 240 MHz, codec alone, no WiFi) one decode takes 12 to 29 ms (0.3 ms per character), so a round trip
takes about 25 to 50 ms per received full frame. That is small against seconds between frames at SF12 and not small at
SF7. The LoRa CRC already covers full frames on the air, so this is currently not recommended.

### 3. Receivers must drop unknown `3C FF` frames (spec changed)

A Linux tracker displayed compressed frames as garbage text, so "existing firmware drops them" is not true everywhere
(spec 01 had said so, after checking one firmware's source). The spec now says a receiver MUST drop any `3C FF` frame
whose third byte it does not know. Firmware that does not follow it will show compressed frames as garbage until it is
changed, which is another reason transmit is off by default. The ESP32 iGate, T-Deck tracker and Dire Wolf branches now drop unknown `3C FF` types.

### 4. Cost in flash

The model is about 125 KB of flash. The iGate build with the decoder is at 96.2% of flash on a TTGO LoRa32, which is
tight, and the T-Deck tracker build is at 72.5%. This limits which boards can carry it.

Looking at where the flash goes in the ESP32 iGate branch (TTGO LoRa32 V2.1 target, 1,736,645 B, 88.3% of the 1.875 MB
application slot): embedded web pages and images about 278 KB, the iGate's own code 131 KB, the PPM model and codec
127 KB, and the Bluetooth controller about 123 KB. Nothing in the iGate uses Bluetooth. The cause is a `btStop()` call in
`wifi_utils.cpp`, which makes the Arduino core link its Bluetooth support and keep the controller's RAM reserved. In a
scratch copy with that one call removed the build was 1,608,377 B (81.8%), about 128 KB smaller, and static RAM was
3.8 KB lower. **Only built, not run on a board**, and the free heap was not measured, so this is a lead and not a result.

## What the tests confirmed

* The standalone codec, with no changes, expanded every compressed frame received to the exact original text, on
  two devices it was built into; a position beacon with comment went from 100 bytes to 51 or 52, a Mic-E position from
  47 to 37 to 40. Eighteen beacons on one link in the second run, primed frames off, so this is a small trial.
* Offline, on 26,303 packets heard in the tester's area and 990 from another network, the standalone mode cut the average frame
  size by 47.7% and 50.0%, against 32.8% and 25.0% for the native frame of the other draft. This is the same
  direction as the held-out 41% here, on traffic the table was not trained on.
* A combination (native frame for the structured fields, this model for the free text) came out between the two, so
  the two are not exclusive, but the combination did not beat standalone on that data.

## Still open

* Whether a two-byte text check is worth the extra byte.
* The round-trip check for full frames (costs time and a rule, see above).
* Run the new layout between more devices and links, and run the ESP32 iGate firmware with it.
* Whether removing the `btStop()` call is safe on a board (not tried).
