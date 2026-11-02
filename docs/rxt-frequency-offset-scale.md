# RXT frequency offset: a logarithmic scale (draft proposal)

> **DRAFT, by K6ATV, for discussion. Not sent to anyone.** The RXT tuple was created by Jon (N7UV) and its scales are F4MLV's (`moricef/LoRa_APRS_iGate`, branch `rxt`). Nothing here changes
> LACF until they, and the people who run RXT firmware, agree. The measured range (section 5) is still to be filled in from the collector.

## 1. The problem

The third byte of an RXT tuple is the **frequency offset**: how far the sender's carrier was from the receiver's, in Hz as the radio reports it. Today it is one byte (0 to 89, the RXT character minus 33) on a
quadratic scale: index 45 is 0 Hz and the ends are about **-2,500 Hz** (index 0) and **+2,390 Hz** (index 89). Everything beyond the ends reads as the end.

Offsets beyond 2.5 kHz are ordinary. They come from the crystals of two boards, which are specified in parts per million of the carrier:

| Carrier | 10 ppm | 20 ppm |
|---|---|---|
| 433.775 MHz | 4.3 kHz | 8.7 kHz |
| 915 MHz | 9.2 kHz | 18.3 kHz |

On our own boards (433.775 MHz, SF12, 125 kHz, collector rows of 2 November 2026): the LoRa32 hears the T-Deck at about **-3.3 kHz** and the hill at about **-3.2 kHz**; the hill hears the LoRa32 at
about **+3.0 kHz** (the same pair, with the sign reversed); and the LoRa32 hears another of our stations at about -2.1 kHz. The first three are past the end of the scale, so the field holds no information
about them: it says "at least 2.4 kHz". The same collector rows show where the offset comes from: the hill hears the T-Deck at only -51 to -62 Hz, so those two boards agree and the **LoRa32 is about 3 kHz (7 ppm) away from both**. It is one board's crystal, not
every pair. That board is a LILYGO LoRa32 V2.1, a type that is common for iGates, so other stations with such boards would probably saturate too (not measured on other boards). The offset of a given sender and receiver is mostly a steady bias of their two
crystals, so the value would be most useful for finding a board whose crystal has drifted or is off, and that is exactly where the scale saturates.

## 2. The proposal

Keep one byte and 90 values. Replace the quadratic scale by a **logarithmic** one around the same centre, so that the steps grow with the offset and the range is several times wider:

    u        = index - 45, limited to -44 .. +44        (index 0 reads as -44, as index 1 does)
    offset   = sign(u) * A * (B^|u| - 1)                Hz           A = 753 Hz, B = 1.0664
    index    = 45 + sign(h) * round( ln(|h| / A + 1) / ln B ),  limited to 1 .. 89,  for an offset of h Hz

The first step is 50 Hz, each step is 6.6% larger than the one before, and the range is **+-11,990 Hz**. The relative precision is about 7% everywhere: coarse enough to cover a crystal offset at 915 MHz,
fine enough to tell a 3 kHz offset from a 4 kHz one.

| Offset | Now: index | Now: reads | Proposed: index | Proposed: reads | Proposed: step there |
|---|---|---|---|---|---|
|     0 Hz | 45 |     0 Hz | 45 |     0 Hz |  50 Hz |
|    50 Hz | 51 |    44 Hz | 46 |    50 Hz |  53 Hz |
|   100 Hz | 54 |   100 Hz | 47 |   103 Hz |  57 Hz |
|   300 Hz | 61 |   316 Hz | 50 |   285 Hz |  69 Hz |
|  1000 Hz | 73 |   968 Hz | 58 |   984 Hz | 115 Hz |
|  2000 Hz | 85 |  1975 Hz | 65 |  1971 Hz | 181 Hz |
|  2500 Hz | 89 |  2390 Hz | 68 |  2550 Hz | 219 Hz |
|  3000 Hz | 89 |  2390 Hz | 70 |  3004 Hz | 249 Hz |
|  3300 Hz | 89 |  2390 Hz | 71 |  3253 Hz | 266 Hz |
|  5000 Hz | 89 |  2390 Hz | 77 |  5139 Hz | 391 Hz |
|  9000 Hz | 89 |  2390 Hz | 85 |  9101 Hz | 654 Hz |
| 12000 Hz | 89 |  2390 Hz | 89 | 11990 Hz | 793 Hz |

The scale is symmetric (the quadratic one is not: its index 0 is -2,500 Hz and its index 89 is +2,390 Hz).

## 3. What it changes

* **Frame size and format: nothing.** The tuple stays 4 bytes and the byte stays 0 to 89, so every frame, vector, range check and the RXT mask are unchanged. Only what the third byte *means* changes.
* **Code:** the scale functions (`rxt_scale.h` in the iGate and tracker libraries) and the table in `docs/lacf-byte-reference.md`. Nothing in the frame code (`hb.c`).
* **Nobody that we know reads the value yet:** the iGate and the tracker write it only in the relay change that is not yet released; Dire Wolf counts the tuples; the collector does not decode it. That is why now is the cheap time to change it.

## 4. Compatibility, and the options

The catch is the text form. F4MLV's RXT v2 trailer in a text packet carries the same four values as one character each (the same 0 to 89), and the tracker converts between the trailer and the binary tuple
by copying the character, with no change of scale. If LACF alone used a different scale, a value would be misread when a packet crosses between the two forms.

| Option | What it means | Cost |
|---|---|---|
| **A. Change the scale for everyone** (recommended) | RXT v2 text and LACF use the logarithmic scale | needs F4MLV and Jon, and every firmware that writes the field; today that is a few test builds |
| B. LACF only, convert at the boundary | the binary tuple uses the new scale, the text trailer keeps the old one; the tracker converts when it moves a value between them | lossy at the ends, two scales to explain, and F4MLV's code is not ours to change |
| C. Leave it | the scale saturates at +-2.4 kHz; document it | the field stays blind for ordinary crystal offsets |

A fourth idea, **D**, is to give the offset in parts per million of the carrier instead of Hz, so one scale serves 433 and 915 MHz alike (12 kHz is 28 ppm at 433 MHz and 13 ppm at 915 MHz). A receiver knows its own
carrier, so it can turn ppm back into Hz. It is tidier, and it is a bigger change to the meaning of the field than A.

## 5. Measured range

The collector keeps the offset of every decoded reception. The query [`tools/fo-distribution.py`](../tools/fo-distribution.py) (read only, run on the collector with `ssh droplet 'python3 -' < tools/fo-distribution.py`)
was run on 2 November 2026. **The data are 433.775 MHz only**: there are no 868 or 915 MHz receptions in the collector.

| Measure | Result |
|---|---|
| Decoded receptions with an offset, period | 770, 30 October to 2 November 2026 (UTC), one band (434 MHz), 6 of our own and other stations as senders |
| Share beyond 2.5 kHz / 3.5 kHz / 5 kHz / 10 kHz | **42.3%** (326 of 770) / 9.6% / **0%** / 0% |
| Median, 95th, 99th, largest | 2,188 Hz, 3,585 Hz, 3,918 Hz, **4,249 Hz** |
| Offset in ppm: median, 99th, largest | 5 ppm, 9 ppm, 10 ppm |
| Pairs (sender and receiver, 5 receptions or more) | 11; median |offset| 2,126 Hz, largest 3,424 Hz; **4 of 11 are beyond 2.5 kHz** |
| Scatter within a pair (median absolute deviation) | median 108 Hz, largest 304 Hz |

What the numbers say:

* **Almost half of the receptions sit at the end of today's scale** (42.3% beyond 2.5 kHz), and nothing in this data is beyond 4.3 kHz.
* **The offsets belong to boards, not to the network.** The LoRa32 (K6ATV-14) is 3.3 to 3.4 kHz away from the T-Deck and the hill (-3,424 and -3,350 Hz as receiver, +3,065 Hz as sender), two more of our stations are about 2.1 kHz away from it, and the T-Deck, the hill and a third station that both of them hear agree within 100 Hz (-94, -74 and +38 Hz). Three boards of six are 2 to 3.4 kHz off.
* **Offsets are steady:** within a pair the scatter is about 100 to 300 Hz, so a scale with steps of 7% (about 250 Hz at 3 kHz) loses little.
* **Range for this band:** everything fits in +-5 kHz (10 ppm of 433.775 MHz is 4.3 kHz). **For 868 and 915 MHz there is no data**: the same 10 ppm is 8.7 and 9.2 kHz, 20 ppm is 17 and 18 kHz.

## 6. To decide

1. The range and the first step. The data fit two choices: **(a)** keep F4MLV's quadratic scale and widen it to +-5 kHz (one constant; all 770 receptions fit, a step of about 175 Hz at 3 kHz), which fits this band and would saturate for crystal boards at 868 or 915 MHz; **(b)** the logarithmic scale above, +-12 kHz, which also covers 915 MHz at 10 ppm, or option D (ppm of the carrier), which serves every band with one scale. The 12 kHz in section 2 is for 915 MHz, not for what we measured.
2. Option A, B, C or D. A and D need F4MLV and Jon; K6ATV would raise it with them, not this document.
3. If A or D: the same change in F4MLV's `rxt` branch, in the byte reference and in the iGate and tracker `rxt_scale.h`, in one step, so the scales never differ on the air.
