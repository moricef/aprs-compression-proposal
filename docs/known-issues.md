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
The layout change is a different primed header under the same `3C FF C1` mark. The three firmware branches (ESP32 iGate, T-Deck tracker, Dire Wolf) were updated to it on 26 October 2026. The ESP32 iGate and Dire Wolf codecs pass the new vectors in their host tests (103 primed vectors; Dire Wolf also the unknown-frame test). The T-Deck branch carries the byte-identical codec file of the iGate branch. **None of the three has been built into firmware or run on the air since the change** (no firmware build was possible where this was done), so treat the new layout as untested on real radios until it has been.

### 2. A cut-short frame decoded to some text (fixed for primed frames; open for full frames)

The decoder pads missing bytes with zeros, so a frame cut down to its length still decodes to some text. The LoRa CRC
protects against this on the air, but the decoder did not itself reject such frames. For primed frames the text check
now rejects them: of 263 primed frames cut by 1 to 3 bytes, none decoded to wrong text. Full frames (`3C FF 81`)
have no check byte, and adding one would cost a byte on every full frame, so they still rely on the LoRa CRC. Open:
whether a decoder should also re-encode its output and reject frames that differ from it, which costs nothing on air
but would reject frames from an encoder that is valid but not byte-identical to this one. Not tried.

### 3. Receivers must drop unknown `3C FF` frames (spec changed)

A Linux tracker displayed compressed frames as garbage text, so "existing firmware drops them" is not true everywhere
(spec 01 had said so, after checking one firmware's source). The spec now says a receiver MUST drop any `3C FF` frame
whose third byte it does not know. Firmware that does not follow it will show compressed frames as garbage until it is
changed, which is another reason transmit is off by default. The ESP32 iGate, T-Deck tracker and Dire Wolf branches now drop unknown `3C FF` types.

### 4. Cost in flash

The model is about 125 KB of flash. The iGate build with the decoder is at 96.2% of flash on a TTGO LoRa32, which is
tight, and the T-Deck tracker build is at 72.5%. This limits which boards can carry it.

## What the tests confirmed

* The standalone codec, with no changes, expanded every compressed frame received to the exact original text, on
  two devices he built it into; a position beacon with comment went from 100 bytes to 51, a Mic-E position from 47 to 37.
  Nine beacons on one link, primed frames off, so this is a small trial.
* Offline, on 26,303 packets heard in the tester's area and 990 from another network, the standalone mode cut the average frame
  size by 47.7% and 50.0%, against 32.8% and 25.0% for the native frame of the other draft. This is the same
  direction as the held-out 41% here, on traffic the table was not trained on.
* A combination (native frame for the structured fields, this model for the free text) came out between the two, so
  the two are not exclusive, but the combination did not beat standalone on that data.

## Still open

* Whether a two-byte text check is worth the extra byte.
* The re-encode check for full frames.
