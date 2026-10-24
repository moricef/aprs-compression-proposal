# Compression by country: rough estimate (pilot data)

**Status: rough. Superseded when the longer multi-country capture has been analysed.** Small cells
(tens to a few hundred packets), one 5-minute capture, no confidence intervals yet. Do not quote
these as final numbers.

## Method

* One 5-minute read-only APRS-IS capture, filtered by callsign prefix for about 25 countries, one
  connection. 18,392 unique packets; 6,314 used: every LoRa packet (destination `APL*`) plus VHF
  packets an iGate heard over the air (`qAR`/`qAO`).
* Country assigned from the source callsign's prefix. Approximate: portable and travelling stations
  are misfiled.
* Train on 70% of stations (split by hash of the source callsign), test on the other 30%.
  Table pruned at min-count 4, because the training set is small.
* **LoRa** figures are the whole LoRa frame, including the 3-byte header, compared with the plain
  frame. **VHF** figures are the whole AX.25 frame, with only the info field compressed and 7 bytes
  per address, 2 control/PID, 2 FCS and 2 bytes of compression overhead assumed (as in
  `spec/07-ax25-binding.md`).
* Cells with fewer than 40 test packets are left out. Mexico, Japan, Brazil, Australia and others
  are in the capture but too thin so far.
* The raw capture contains real callsigns and positions and is not published. Only these aggregates are.

## Global table, tested on stations it never saw

| Country | Mode | Packets | Plain B | Compressed B | Saved |
|---|---|---|---|---|---|
| Germany | LoRa | 114 | 68.4 | 29.8 | 56% |
| Spain | LoRa | 46 | 66.3 | 33.2 | 50% |
| Italy | LoRa | 80 | 64.9 | 34.5 | 47% |
| UK | LoRa | 54 | 57.1 | 33.4 | 41% |
| **All** | **LoRa** | **506** | **68.7** | **34.7** | **49%** |
| Germany | VHF | 158 | 76.9 | 53.9 | 30% |
| France | VHF | 58 | 82.6 | 58.6 | 29% |
| Italy | VHF | 90 | 75.6 | 56.0 | 26% |
| UK | VHF | 58 | 68.4 | 51.5 | 25% |
| US | VHF | 624 | 72.4 | 55.1 | 24% |
| Canada | VHF | 63 | 71.2 | 55.8 | 22% |
| Spain | VHF | 66 | 62.0 | 50.8 | 18% |
| Poland | VHF | 90 | 76.0 | 63.0 | 17% |
| **All** | **VHF** | **1,463** | **73.1** | **55.5** | **24%** |

## Does the vote matter? Traffic-weighted against capped

Training was capped at 600 packets per country (3,332 training packets) and tested on the same
held-out stations. Almost nothing changed: US VHF 22% (24% uncapped), all LoRa 49%, all VHF 23%
(24%). At this data size the weighting makes little difference.

## A country the table has never seen

Trained on the other countries, tested on every packet of the held-out country:

| Country | Mode | Packets | Saved | (global table, above) |
|---|---|---|---|---|
| US | VHF | 2,030 | 18% | 24% |
| Germany | VHF | 575 | 25% | 30% |
| Germany | LoRa | 443 | 49% | 56% |
| Italy | VHF | 299 | 22% | 26% |
| Italy | LoRa | 229 | 42% | 47% |
| Poland | VHF | 207 | 19% | 17% |
| Poland | LoRa | 180 | 42% | not shown |
| US | LoRa | 84 | 47% | not shown |

A new country starts a few points behind (about 4-7 points here), not at zero.

## Reading it

* LoRa is near 49% for the countries with enough data. VHF is near 24% of the whole frame, because
  the address fields cannot be compressed.
* The spread between countries (VHF 17-30%) is within what small samples produce; do not read
  rankings into it yet.
* Still missing: Mexico, Japan, Brazil, Australia and other countries; non-ASCII comment
  encodings; a traffic-weighted airtime-saved-per-day figure; confidence intervals.

## Next

Re-run on the 8-hour multi-country capture, add the missing countries, add airtime saved per day
and intervals, and replace this page.
