# How the compression works, why it was chosen, and how it was trained

Plain-language companion to `spec/01-compressed-frames.md` (which has the exact algorithm).

## The problem

A LoRa APRS packet is ordinary text, 40 to 100 bytes, sent at a few hundred bits per second. At
spreading factor 12 a 57-byte beacon takes about 2.6 seconds on the air, and the channel is shared.
Fewer bytes means less airtime, fewer collisions and less battery.

## The idea in one paragraph

Text is predictable. After `WID` the next letter is almost always `E`. After `APL` it is `R` or `T`.
After `3400.0` comes another digit. If a sender and receiver both hold the same table of "what
usually follows these three characters", the sender can spend very few bits on the likely
continuation and more only on the surprising ones. The receiver runs the same predictions and
reverses the process. Nothing about the packet itself has to be learned, because the learning
happened ahead of time, once, and is shipped as a fixed table.

## The parts

1. **A predictor (order-3 context model).** For each byte, look at the up to three bytes before it
   in the same packet and ask the table how often each possible next byte followed that context. If
   the table has never seen that context, fall back to the last two bytes, then one, then none. This
   fallback and the weighting between orders is the PPM scheme (prediction by partial matching, "method C"
   escape weights). It produces a probability for each of the 256 byte values.
2. **An entropy coder (range coder).** It turns "this byte had probability p" into about `-log2(p)`
   bits, and packs those fractional bits together, so a very predictable byte costs well under one
   bit. This is the same family as arithmetic coding.
3. **Integer arithmetic only.** The probabilities and the coder use integers, never floating point,
   so a small ESP32, a Pi and a PC all compute exactly the same numbers. Any difference between sender
   and receiver would corrupt the output, so this is a requirement, not an optimisation.
4. **A frame around it.** A three-byte header that old firmware ignores, a length byte, then the coded
   bytes. If compression would not make the frame smaller, the plain frame is sent instead.

## Why not an off-the-shelf compressor?

The first thing tried was the obvious: zlib, LZMA and bzip2 on real LoRa APRS packets. They do almost
nothing at this size. Adaptive compressors learn from the data as they go, and a 70-byte packet is
over before there is anything to learn from. Their headers and checksums can even make the packet
bigger. Raw deflate with no header saved about 2% at best.

What does work is moving the learning out of the packet, into a shared table. Measured on packets
from stations that were not used in training (70% of the stations to train, 30% to test), using
an early LoRa-only corpus of about 1,400 packets:

| Method | Saved |
|---|---|
| zlib, LZMA, bzip2 | about nothing; sometimes larger |
| Static Huffman code (single-byte statistics) | 26% |
| Deflate with a dictionary trained on APRS text | 37% |
| Context model, order 1 | 44% |
| Context model, order 2 | 55% |
| **Context model, order 3** | **58%** |
| Context model, order 4 | 57% |

Those context-model numbers are the ideal cost, with 1 byte added for the coder, measured before any
real format overhead existed. The shipped format pays for a header and length byte and prunes the
table to fit in flash, so it saves less (see "What it achieves" below).

**Why order 3.** Order 2 to 3 still gains a few points. Order 4 gains nothing (it dilutes the
statistics on a corpus this small) while the table keeps growing. So order 3 is where the return runs
out. A larger training corpus might move this.

**Other ideas looked at.** SMAZ2 (a small-string compressor with a built-in dictionary), a Meshtastic
forum suggestion for a static Huffman dictionary, PSK31's Varicode and PACTOR's Huffman coding (both
fixed per-character codes), and V.42bis / MNP5 from old modems (adaptive, so poor on short packets).
I did not find an APRS-specific scheme that uses a trained context model. That is a statement about
what I could find, not proof that none exists.

## How it was trained

**Data.** Real packets read from APRS-IS (the internet backbone of APRS) with a read-only login, over
two days in October 2026. The capture script is `tools/capture_aprsis.py`.

| Source | How selected | Used for |
|---|---|---|
| LoRa APRS | Software device ID in the destination field (`APL*`, `APLRT*`, `APLRG*`, `APLT*`); an initial short capture plus a longer one of about an hour | LoRa-style packets |
| VHF APRS | Packets from stations within 250 km (first capture) and within 2,500 km (second) of a point; only packets an iGate heard over the air (`qAR`/`qAO`) were kept | Messages, objects, status, weather, Mic-E and other packet types |

**Preparation (`tools/train_model.py`).**
* Each packet is reduced to what goes over the air: `SRC>DEST,PATH:info`, with the APRS-IS
  q-construct and anything after it removed (APRS-IS adds those; the radio never carried them).
* Identical packet strings are kept once. Packets longer than 255 bytes are dropped.
* Result: **14,186 unique packets**, mixed LoRa and VHF.

**Counting.** For every byte position in every packet, the trainer records which byte followed which
preceding 0, 1, 2 and 3 bytes of the same packet (positions near the start use fewer bytes of
context). The result is four tables of counts.

**Pruning.** Rare contexts are dropped (`--min-count 8`: an entry must have been seen at least eight
times). This is the size/ratio dial. Counts are then scaled into one byte each, per context, keeping
their proportions. The output is a small sorted table (about 125 KB in flash for model v1) written as a C file, with a SHA-256 so
every implementation can check it holds the same bytes.

**Honest evaluation.** Quoting a compression ratio on packets the model was trained on is
meaningless, because it has memorised them. All the ratios above use **held-out stations**: the data is split by a hash of the station's callsign, 70% of the stations to train
and 30% to test, so the test packets are from stations the model has never seen. Model v1 itself is
trained on everything; its true performance is the held-out figure.

## What it achieves

Held-out results on the shipped format (frame sizes include the 3-byte header):

| `--min-count` | Table size (70% data) | Saved |
|---|---|---|
| 4 | 179 KB | 52% |
| 6 | 121 KB | 50% |
| **8 (v1)** | **94 KB at 70% data; 125 KB for v1 on all data** | **48%** |
| 12 | 64 KB | 45% |
| 20 | 39 KB | 42% |

Typical result for a LoRa packet: about 76 bytes down to about 39. On the VHF AX.25 framing, where
only the info field is compressible, the whole frame shrinks by about 23% (draft 07).

## Why it is a fixed table and not a learning compressor

* The packet is too short to learn from.
* Both ends must hold the identical table, or the output is garbage. Learning on the fly would make
  the two tables drift apart if one packet were lost.
* A fixed table is small, fast and needs no memory of previous packets.

The cost: the table is part of the spec. It is frozen, versioned and hashed, and a new table is a new
version.

## Limits and honest caveats

* **Small, short-window corpus.** About 14,000 unique packets from a few hours over two days. A bigger,
  longer, more varied capture would improve the ratio and make the pruning less lossy.
* **Bias toward common firmware.** A station whose position changes counts as many distinct packets,
  and a few popular firmwares dominate the text. The model is best at what is most common and worse at
  unusual packets (it falls back to nearly plain, and never expands a packet because the sender then
  sends it plain).
* **The corpus is mostly non-US and one short window.** LoRa packets were selected by device type, not place, so they come from all over (mostly Europe). VHF packets were selected within 2,500 km of central US. By callsign prefix, a larger version of the corpus (22,364 unique packets) was about 88% non-US and 12% US. See `docs/per-country-estimate.md` for how much that matters.
* **Not tested over the air yet.** The codec is tested on a PC (round trip on every packet, fuzzed with
  mutated and truncated packets, and two independent implementations agree byte for byte).
* **The table holds statistics from real packets.** With the pruning used, a one-off station's string is
  not stored, but common callsigns or comment fragments seen many times can appear in it. All of the
  source data was already public on APRS-IS.

## Making it better later

* More data: longer captures, other regions, other firmwares. A table trained on many times more
  packets can be larger and still fit if pruned harder.
* Separate models for fields (the callsign, the position digits, the comment), since they behave
  differently.
* Priming the context at the start of the info field on VHF framing.
* A second table version once real use shows what is common.

## Reproduce

    python3 tools/capture_aprsis.py --call YOURCALL --filter 'u/APL*' --seconds 3000 --out lora.txt
    python3 tools/capture_aprsis.py --call YOURCALL --filter 'r/LAT/LON/250' --seconds 600 --out vhf.txt
    python3 tools/train_model.py --lora lora.txt --vhf vhf.txt --min-count 8 --holdout 0.3 \
        --test-out heldout.txt --out /tmp/model_test.c
    # build the tool against the new table and measure on heldout.txt:
    cc -std=c99 -O2 -Ireference test/ppm_tool.c reference/aprs_ppm.c /tmp/model_test.c -o /tmp/tool
    /tmp/tool stats < heldout.txt

Your numbers will differ with your data. The raw captures contain real callsigns and positions, so
they are not published here.
