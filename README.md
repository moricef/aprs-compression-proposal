# APRS-PPM: LoRa APRS packets in half the bytes

**The same APRS packet, sent in about half the bytes, and turned back into the identical text at the other end.**
It extends APRS and does not replace it: stations that don't know about it carry on exactly as before.

> One person's proposal. Not a standard, and nobody has been asked to adopt it. It has been tried on a few real
> radios over one short link, and replayed on tens of thousands of captured packets. What is proven and what is
> not is listed [below](#what-this-does-not-show-yet).

## Why

A LoRa APRS packet is ordinary text, 40 to 100 bytes, sent very slowly on a channel everyone shares. At spreading
factor 12 a 57-byte beacon holds the channel for about 2.6 seconds. Fewer bytes means less time on the air, fewer
collisions, more stations on one channel, and longer battery life on a tracker.

## The idea

APRS text is predictable: after `WID` comes `E`, after `APL` comes `R` or `T`, and a station repeats its own comment,
symbol and path in every beacon. Sender and receiver share a small fixed table of "what usually comes next", so the
likely characters cost almost nothing and only the surprises cost bits. Nothing is learned on the air, and the table
ships with the software.

Two kinds of compressed frame:

* **Full frame** (`3C FF 81`): the whole packet, compressed. Anyone with the table can read it. About **40% smaller**.
* **Primed frame** (`3C FF C1`): coded against the same station's last full packet, so only what *changed* is sent.
  About **55 to 70% smaller**. A receiver that missed the reference drops it and recovers at the next full frame.

A station sends a full frame at least every second packet by default (a stationary one can send them less often), so a lost
frame costs little. If compression would not save anything, the plain frame goes out instead.

## Results

### On real radios (24 to 25 October 2026)

An ESP32 iGate, a LILYGO T-Deck tracker and a Raspberry Pi running Direwolf with an SX1262 LoRa HAT, 433.775 MHz,
SF12, 125 kHz. Details, with every frame, are in [docs/over-the-air-test.md](docs/over-the-air-test.md).

| Test | Result |
|---|---|
| ESP32 sends, Direwolf receives | **12 of 12** decoded exactly. Average position beacon: **75 B plain, 38 B full, 14 B primed** |
| Direwolf sends, ESP32 receives | **5 of 5** exactly. **57 B plain, 25 B full, 8 B primed** |
| T-Deck tracker sends, ESP32 receives | **6 of 6** exactly. Full frames 42% smaller, primed frames **63 to 68% smaller** |
| Direwolf sends, T-Deck receives | **16 of 16** frames received; all 8 compressed ones expanded to exactly the text sent |
| T-Deck to a Direwolf iGate on a hill, 2 miles, every 15 minutes | Compressed beacon expanded and **passed to APRS-IS one second later as an ordinary packet**; nothing downstream needed to know |
| The same, overnight (10 h 37 min, 40 beacons received of about 43 sent) | **50% fewer bytes on air** (844 B against 1,705 B). Three beacons did not arrive; the gateway logged one primed frame dropped for lack of its full frame |

Airtime at SF12, from the LoRa airtime formula (not measured on air): the 57-byte beacon takes **2.6 s plain,
1.5 s as a full frame and 1.0 s as a primed frame**. The preamble does not shrink, so time saved is a little less
than bytes saved.

### On 1,037 live packets (replayed, 24 October 2026)

Fresh LoRa APRS-IS traffic captured read-only after both formats were built. Each packet is compressed and the
result decoded back; the average size is what is on the air.

```
plain                    ███████████████████████████████████ 70.7 B
a binary "native" frame  ██████████████████████████ 51.8 B   27% smaller
APRS-PPM, one frame      █████████████████ 33.2 B            53% smaller
APRS-PPM, full + primed  ████████████████ 32.1 B             55% smaller
```

The "native" frame is a different community draft, a binary layout designed around the APRS fields; see
[docs/alternatives-considered.md](docs/alternatives-considered.md) for the comparison and its limits.

### On bigger sets

| Packets | Plain | Native frame | APRS-PPM, one frame | APRS-PPM, full + primed |
|---|---|---|---|---|
| 27,276 LoRa, a three-hour capture | 69.2 B | 50.4 B (27%) | 32.1 B (54%) | 24.4 B (65%) |
| 1,090 Mic-E packets, VHF | 61.1 B | 44.5 B (27%) | 49.7 B (19%) | 40.7 B (33%) |

The table was trained on 70% of the stations in a separate three-hour capture and tested on the other 30%, so the
earlier study's figures are on stations it had never seen: **41% fewer bytes** per frame without priming, and about
**55%** with a full frame every second packet ([docs/per-country-estimate.md](docs/per-country-estimate.md)).
On compact VHF Mic-E packets the saving is smaller and the native frame does better for single frames; that is
reported as is.

## What this does not show yet

* **One link.** About 2 miles, signal around -95 to -98 dBm at the gateway, a few hours to one night, mild weather.
  Not tested: a busy channel, collisions, a really weak signal, other boards, other spreading factors, a stock iGate
  or tracker meeting a compressed frame.
* **Receivers without the code can't read compressed frames.** They hear and ignore them. Transmit is therefore
  off by default and is meant for places where every receiver that matters has the code. A compressed frame
  that an iGate with the code hears reaches APRS-IS as a normal packet.
* **A lost full frame also costs the primed frame after it.** That is the price of priming, and why a full frame
  is sent at least every second packet.
* **No plain-frame control** ran alongside the overnight test, so it does not say whether the same three beacons
  would have been lost uncompressed.
* **Replays use one table, trained on one evening of mostly European data.** The saving varies by device type
  and country ([docs/per-country-estimate.md](docs/per-country-estimate.md)). The raw captures hold real
  callsigns and positions and are not published, only the aggregates.

## What exists

| Where | What |
|---|---|
| [ESP32 iGate firmware](https://github.com/radiohound/LoRa_APRS_iGate/tree/feature/order3-compression) | Receive always on, transmit opt-in |
| [Direwolf, LoRa HAT channel](https://github.com/radiohound/direwolf/tree/feature/lora-compression) | Receive always on, transmit opt-in; an iGate that expands compressed frames and gates them as normal packets |
| [T-Deck tracker firmware](https://github.com/radiohound/LoRa_APRS_Tracker-moricef/tree/ppm-compression) | Receive always on, transmit opt-in |
| [`spec/`](spec) and [`reference/`](reference) | The specifications, a plain-C codec, the frozen table and 26 + 103 test vectors |

These are shown as an idea that works, not as a request. No pull request will be opened unless a maintainer says
one would be welcome. They are not part of the Direwolf LoRa driver pull request (#638).

## Read more

* [How it works, in plain language](docs/how-it-works.md) and [why not 438 or an off-the-shelf compressor](docs/alternatives-considered.md)
* [The over-the-air test log](docs/over-the-air-test.md), with a section updated as the long run continues
* [Everything else: all specs, simulation tables, roadmap, VHF notes](DETAILS.md)

## Try it

    make test        # checks the reference codec against the test vectors

To try it on real radios, start with the [safe-testing page](https://github.com/radiohound/LoRa_APRS_iGate/blob/feature/order3-compression/tools/aprs_compress/EXPERIMENT.md)
for the ESP32 firmware. Questions, results and problems are welcome as issues here.

Licensed under the [MIT License](LICENSE).
