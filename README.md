# APRS-PPM: LoRa APRS packets in half the bytes

**The same APRS packet, sent in about half the bytes, and turned back into the identical text at the other end.**
It extends APRS and does not replace it: stations that don't know about it carry on exactly as before.

> An idea that is showing promise and needs more testing. It has been tried on three kinds of hardware over two links, and
> replayed on tens of thousands of captured packets. What is proven and what is not is listed
> [below](#what-this-does-not-show-yet).

## Why

A LoRa APRS packet is ordinary text, 40 to 100 bytes, sent very slowly on a channel everyone shares. At spreading
factor 12 a 57-byte beacon holds the channel for about 2.6 seconds. Fewer bytes means less time on the air, fewer
collisions, more stations on one channel, and longer battery life on a tracker.

## The idea

APRS text is very repetitive. Nearly every packet carries the same repeater path (`WIDE1-1`), the destination field names the
software (`APLRT1` for a LoRa tracker), a station repeats its own comment and symbol in every beacon, and the digits of a
position barely change from one beacon to the next. Sender and receiver share a small fixed table of "what usually comes next", so the
likely characters cost almost nothing and only the surprises cost bits. Nothing is learned on the air, and the table
ships with the software. The method is called **PPM**, *prediction by partial matching*, a text-compression technique
from 1984 (Cleary and Witten) whose PPMd variant is a standard method in 7-Zip and RAR archives. Here, each character is
predicted from the three before it.

Two kinds of compressed frame:

* **Full frame** (`3C FF 81`): the whole packet, compressed. Anyone with the table can read it. About **40 to 50% smaller**: 41% on stations the table had never seen, 47% over the full-day radio test below.
* **Primed frame** (`3C FF C1`): coded against the same station's last full packet, so only what *changed* is sent.
  About **55 to 70% smaller**. A receiver that missed the reference drops it and recovers at the next full frame. A check byte over the whole packet
  (added after an independent tester's findings, see [known issues](docs/known-issues.md)) stops a wrong reference from decoding to the wrong text.
  The primed figures on this page were measured before that byte; each primed frame is now one byte larger.

A station sends a full frame at least every second packet by default (a stationary one can send them less often), so a lost
frame costs little. If compression would not save anything, the plain frame goes out instead.

## Results

### On real radios (24 to 25 October 2026)

An ESP32 iGate, a LILYGO T-Deck tracker and a Raspberry Pi running Direwolf with an SX1262 LoRa HAT, 433.775 MHz,
SF12, 125 kHz. Details, with every frame, are in [docs/over-the-air-test.md](docs/over-the-air-test.md).

| Test | Result |
|---|---|
| ESP32 sends, Direwolf receives | **12 of 12** decoded exactly. An average-sized position beacon with a comment: **75 B plain, 38 B full, 14 B primed** |
| Direwolf sends, ESP32 receives | **5 of 5** exactly. The gateway's short station beacon: **57 B plain, 25 B full, 8 B primed** |
| T-Deck tracker sends, ESP32 receives | **6 of 6** exactly. A tracker beacon (40 B plain, no comment): full frames 42% smaller, primed frames **63 to 68% smaller** |
| Direwolf sends, T-Deck receives | **16 of 16** frames received; all 8 compressed ones expanded to exactly the text sent |
| T-Deck to a Direwolf iGate 2 miles away, every 15 minutes | Compressed beacon expanded and **passed to APRS-IS one second later as an ordinary packet**; nothing downstream needed to know |
| The same, for a full day (23 h 57 min, 80 beacons expanded of about 96 sent) | **51% fewer bytes on air** (1,736 B against 3,515 B). About 10 beacons were never heard, and 6 primed frames arrived but were dropped because their full frame was missed |

The tests used different packets, so compare plain with compressed within a row, not between rows: a shorter packet is smaller in every
format, whichever device sends it.

Airtime at SF12, from the LoRa airtime formula (not measured on air): the 57-byte beacon takes **2.6 s plain,
1.5 s as a full frame and 1.0 s as a primed frame**. The preamble does not shrink, so time saved is a little less
than bytes saved.

### On 1,037 live packets (replayed, 24 October 2026)

Fresh LoRa APRS-IS traffic captured read-only after both formats were built. Each packet is compressed and the
result decoded back; the average size is what is on the air.

```
plain                    ███████████████████████████████████ 70.7 B
APRS-PPM, one frame      █████████████████ 33.2 B            53% smaller
APRS-PPM, full + primed  ████████████████ 32.1 B             55% smaller
```

### On bigger sets

| Packets | Plain | Native frame | APRS-PPM, one frame | APRS-PPM, full + primed |
|---|---|---|---|---|
| 27,276 LoRa, a three-hour capture | 69.2 B | 50.4 B (27%) | 32.1 B (54%) | 24.4 B (65%) |
| 1,090 Mic-E packets, VHF | 61.1 B | 44.5 B (27%) | 49.7 B (19%) | 40.7 B (33%) |

92% of these 1,037 packets come from stations that were in the captures the table was trained on. The 87 from stations
it had never seen came out **44% smaller** as one full frame and 47% with priming, in line with the 41% held-out figure below.

The table was trained on 70% of the stations in a separate three-hour capture and tested on the other 30%, so the
earlier study's figures are on stations it had never seen: **41% fewer bytes** per frame without priming, and about
**55%** with a full frame every second packet ([docs/per-country-estimate.md](docs/per-country-estimate.md)).
On compact VHF Mic-E packets the saving is smaller; that is reported as is.

The "native frame" column is a community draft (draft 0), a binary layout for LoRa APRS with typed fields, packed callsigns and optional
receiver data. Its aims go beyond size, so it is shown as a yardstick, one packet at a time, and not as a rival: on Mic-E,
which is already compact, it is a little smaller than one APRS-PPM frame. The two are not exclusive, since its text part uses a
fixed code that this model could in principle replace (not tried). The comparison and its limits are in
[docs/alternatives-considered.md](docs/alternatives-considered.md).

## What this does not show yet

* **Needs more testing.** So far two links and three kinds of hardware: a LILYGO LoRa32 V2.1 (ESP32), a LILYGO T-Deck and a
  Raspberry Pi LoRa HAT running a branch of Direwolf. The longer link is about 2 miles, signal around -95 to -98 dBm at the
  gateway, one night, mild weather. Not yet tried: a busy channel, a really weak signal, other spreading factors, a stock
  iGate or tracker meeting a compressed frame.
* **Receivers without the code can't read compressed frames.** They hear and ignore them, so transmit is off by
  default and meant for places where every receiver that matters has the code. A compressed frame that an iGate
  with the code hears reaches APRS-IS as a normal packet.
* **One table for everyone.** It was trained on traffic from about 25 countries in proportion to their share of LoRa
  APRS, and the saving is similar across them: most countries land between about 35% and 47%, the US at 43%
  ([docs/per-country-estimate.md](docs/per-country-estimate.md)). The raw captures hold real callsigns and positions
  and are not published, only the aggregates.

## What exists

| Where | What |
|---|---|
| [ESP32 iGate firmware](https://github.com/radiohound/LoRa_APRS_iGate/tree/feature/order3-compression) | Receive always on, transmit opt-in |
| [Direwolf, LoRa HAT channel](https://github.com/radiohound/direwolf/tree/feature/lora-compression) | Receive always on, transmit opt-in; an iGate that expands compressed frames and gates them as normal packets |
| [T-Deck tracker firmware](https://github.com/radiohound/LoRa_APRS_Tracker-moricef/tree/ppm-compression) | Receive always on, transmit opt-in |
| [`spec/`](spec) and [`reference/`](reference) | The specifications, a plain-C codec, the frozen table and 129 worked examples (test vectors) to check an implementation against |

These are shared so people can try the idea, not as a request. No pull request will be opened unless a maintainer says
one would be welcome. They are not part of the Direwolf LoRa driver pull request (#638).

## Read more

* [This page as a 4-page PDF](docs/brochure.pdf), with charts, to read or hand around
* [Known issues and findings from testing](docs/known-issues.md): what other testers found, and what was fixed
* [Questions and answers](docs/faq.md): does it still work on normal LoRa APRS, what reaches APRS-IS, what a stock iGate does
* [How it works, in plain language](docs/how-it-works.md) and [why not 438 or an off-the-shelf compressor](docs/alternatives-considered.md)
* [The over-the-air test log](docs/over-the-air-test.md), with a section updated as the long run continues
* [Everything else: all specs, simulation tables, roadmap, VHF notes](DETAILS.md)

## Try it

    make test        # checks the reference codec against the test vectors

To try it on real radios, start with the [safe-testing page](https://github.com/radiohound/LoRa_APRS_iGate/blob/feature/order3-compression/tools/aprs_compress/EXPERIMENT.md)
for the ESP32 firmware. Questions, results and problems are welcome as issues here.

Licensed under the [MIT License](LICENSE).
