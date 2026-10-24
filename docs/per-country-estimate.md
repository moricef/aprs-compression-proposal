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

## Why the countries differ: device type

The country gap is mostly a device-mix effect. Same pilot data, LoRa packets only, global table,
tested on held-out stations, grouped by the first five characters of the destination (tocall). The
description comes from the APRS device registry (`aprsorg/aprs-deviceid`, maintained by Hessu,
OH7LZB, CC BY-SA 2.0); registry entries are six characters with `?` wildcards, so a five-character
group can span several entries.

| Device ID | Packets | Avg plain | Saved | Registry entry (vendor and model) | Class |
|---|---|---|---|---|---|
| APLG0 | 47 | 84 B | 65% | `APLG??`: OE5BPA, LoRa Gateway/Digipeater | digi |
| APLC1 | 19 | 57 B | 53% | `APLC??`: DL3DCW, APRScube | none listed |
| APLRG | 261 | 72 B | 51% | `APLRG?`: CA2RXU, ESP32 LoRa iGate | igate |
| APLRT | 96 | 45 B | 31% | `APLRT?`: CA2RXU, ESP32 LoRa Tracker | tracker |
| APLET | 10 | 50 B | 13% | `APLETK`: DL5TKL, T-Echo | tracker |

Also in the data: APLRF (`APLRF?`, SQ2CPA, LoRa APRS), APLOX (`APLO??`, SQ9MDD, LoRa KISS
TNC/Tracker), APLT0 (`APLT??`, OE5BPA, LoRa Tracker).

The UK looks low (41% against Germany's 56%) because its sample has more short tracker beacons
(16 APLRT and 7 APLET of 54) and Germany's has long gateway packets (31 APLG0 of 114).
Within a device type the countries are close. Samples are small; APLET is only 10 packets.

## Already-compressed beacons compress least

All LoRa packets in the pilot (not only the test split), by position format in the info field:

| Device | Packets | Position format | Avg total | Avg comment | Saved |
|---|---|---|---|---|---|
| APLRT (ESP32 LoRa Tracker) | 277 | 274 APRS-compressed (base-91), 3 other | 45 B | 6 B | 31% |
| APLET (T-Echo) | 10 | all 10 APRS-compressed | 47 B | 13 B | 13% |
| APLG0 (LoRa Gateway/Digipeater) | 150 | all 150 uncompressed (digit string) | 80 B | 37 B | 65% |
| APLRG (ESP32 LoRa iGate) | 898 | 823 compressed, 46 uncompressed, 29 other | 70 B | 33 B | 51% |

Trackers already use APRS's own compact base-91 position and send short comments. What is left is
the header, the callsign and a near-random position, which no predictor can shrink much, so the
percentage is lower. It is still about 14 bytes of a 45-byte packet. Comment length is a larger
lever than the position encoding (see `docs/rationale.md`, airtime table). The T-Echo figure rests
on 10 packets and may move a lot.

## Earlier region test (larger corpus, US against non-US)

Before the multi-country pilot, the same question was tried on a larger corpus: 22,364 unique
packets from the earlier captures, 2,560 US and 19,804 non-US by callsign prefix (LoRa from all
over, VHF from within 2,500 km of central US). Min-count 8, trained on 70% of stations and tested
on the other 30%. This is not the set model v1 was trained on (14,186 packets); that set could not
be reproduced exactly from the saved files. Figures are the percentage of the plain LoRa frame saved.

| Model trained on | Tested on US (817 packets) | Tested on non-US (6,263 packets) |
|---|---|---|
| Everything (15,284 packets) | 42% | 50% |
| US only (1,743 packets) | 42% | 29% |
| Non-US only (13,541 packets) | 31% | 50% |

* The table trained on everything did as well as a regional table in each region: US-only on US
  was 42% (same as the mixed table), non-US-only on non-US was 50% (same). At this data size the
  global table gives up nothing against a regional one.
* The wrong region costs a lot: 29% and 31% against 42% and 50%.
* The mixed table does worse on US (42%) than non-US (50%). US is a small share of the training
  data and US packets are more VHF-style with different structure; a US-only table trained on the
  same few US packets did no better. The limit is data, not the table being global.
* The US test set is only 817 packets and the US-only model has 1,743 training packets, so the
  numbers on that side carry real uncertainty.

### Special characters

Packets containing any byte above 0x7F (for example UTF-8 umlauts) still saved 46% against 49%
for ASCII-only packets, using the table trained on everything. Only 140 test packets had such
bytes. The special characters themselves are not well predicted, but they are a small part of the
packet and the rest compresses as usual.

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
