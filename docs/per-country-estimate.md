# Compression by country: estimate from 20 minutes of capture

**Status: preliminary. Superseded when the 8-hour multi-country capture has been analysed.** Still a
short window, a single evening in Europe (10:21-10:36 PDT), and small cells for many countries. An
earlier version of this page used only a 5-minute pilot; this one adds 15 minutes more. Do not quote
these as final numbers.

## Method

* Read-only APRS-IS captures, filtered by callsign prefix for about 25 countries, one connection:
  a 5-minute pilot plus the first 15 minutes of a longer run. 22,763 unique packets used: every
  LoRa packet (destination `APL*`) plus VHF packets an iGate heard over the air (`qAR`/`qAO`).
* Country assigned from the source callsign's prefix. Approximate: portable and travelling stations
  are misfiled.
* Train on 70% of stations (split by hash of the source callsign), test on the other 30%.
  Pruning at min-count 8. Min-count 4, used on the smaller pilot, no longer fits the format at this
  data size (the symbol offsets are 16-bit).
* **LoRa** figures are the whole LoRa frame, including the 3-byte header, compared with the plain
  frame. **VHF** figures are the whole AX.25 frame, with only the info field compressed and 7 bytes
  per address, 2 control/PID, 2 FCS and 2 bytes of compression overhead assumed (as in
  `spec/07-ax25-binding.md`).
* Cells with fewer than 40 test packets are left out. Many are still small (tens to a few hundred).
* The raw captures contain real callsigns and positions and are not published. Only these aggregates are.

## Global table, tested on stations it never saw

| Country | Mode | Packets | Plain B | Compressed B | Saved |
|---|---|---|---|---|---|
| US | VHF | 2,160 | 73.2 | 55.7 | 24% |
| Germany | VHF | 455 | 76.7 | 53.7 | 30% |
| Germany | LoRa | 387 | 70.4 | 32.0 | 55% |
| Italy | VHF | 361 | 76.8 | 56.1 | 27% |
| Italy | LoRa | 295 | 69.7 | 36.3 | 48% |
| Canada | VHF | 261 | 68.0 | 54.6 | 20% |
| Poland | VHF | 215 | 73.5 | 61.1 | 17% |
| Spain | LoRa | 194 | 64.2 | 33.9 | 47% |
| Poland | LoRa | 188 | 82.1 | 41.4 | 50% |
| UK | VHF | 179 | 68.7 | 51.2 | 25% |
| Spain | VHF | 167 | 62.3 | 51.1 | 18% |
| France | VHF | 158 | 82.7 | 61.1 | 26% |
| UK | LoRa | 149 | 63.5 | 35.4 | 44% |
| Australia | VHF | 144 | 87.6 | 66.3 | 24% |
| France | LoRa | 136 | 69.4 | 35.7 | 49% |
| Austria | VHF | 130 | 70.2 | 51.3 | 27% |
| US | LoRa | 85 | 71.3 | 37.0 | 48% |
| Switzerland | VHF | 85 | 72.8 | 53.1 | 27% |
| Czechia | VHF | 81 | 66.0 | 55.0 | 17% |
| Czechia | LoRa | 76 | 63.9 | 34.3 | 46% |
| Chile | LoRa | 72 | 72.8 | 42.2 | 42% |
| Netherlands | VHF | 72 | 88.1 | 66.5 | 25% |
| Brazil | VHF | 65 | 81.2 | 65.7 | 19% |
| Russia | VHF | 60 | 87.2 | 63.9 | 27% |
| Japan | VHF | 57 | 61.7 | 48.9 | 21% |
| Norway | VHF | 52 | 71.0 | 56.8 | 20% |
| Norway | LoRa | 50 | 53.8 | 32.1 | 40% |
| Sweden | VHF | 48 | 75.0 | 57.5 | 23% |
| Mexico | VHF | 46 | 80.2 | 56.6 | 29% |
| **All** | **LoRa** | 1,879 | 69.9 | 35.6 | 49% |
| **All** | **VHF** | 4,948 | 73.6 | 55.9 | 24% |

* LoRa is near 49%. VHF is near 24% of the whole frame, because the address fields cannot be
  compressed.
* VHF ranges from 17% to 30% and LoRa from 40% to 55% across countries; differences of a few points
  are within what these sample sizes produce. Do not read rankings into them yet.
* Mexico and Japan have a VHF figure but on 46 and 57 packets; neither has LoRa traffic.
* More data helped: the same analysis on the 5-minute pilot alone, at the same min-count, gave 45%
  (LoRa) and 20% (VHF).

## Does the vote matter? Traffic-weighted against capped

Training capped at 600 packets per country (7,875 training packets), same held-out stations:

| Country | Mode | Packets | Traffic-weighted | Capped at 600 |
|---|---|---|---|---|
| US | VHF | 2,160 | 24% | 20% |
| Germany | VHF | 455 | 30% | 27% |
| Germany | LoRa | 387 | 55% | 52% |
| Italy | VHF | 361 | 27% | 25% |
| Italy | LoRa | 295 | 48% | 45% |
| Canada | VHF | 261 | 20% | 17% |
| Poland | VHF | 215 | 17% | 15% |
| Poland | LoRa | 188 | 50% | 47% |
| Australia | VHF | 144 | 24% | 21% |
| US | LoRa | 85 | 48% | 45% |
| **All** | LoRa | 1,879 | 49% | 47% |
| **All** | VHF | 4,948 | 24% | 21% |

Capping costs 2 to 3 points overall, mostly because US VHF drops from 24% to 20%. Training on
pooled traffic, so that the busiest countries count most, gives the larger total saving.

## A country the table has never seen

Trained on the other countries, tested on every packet of the held-out country:

| Country | Mode | Packets | Saved (country unseen) | Saved (global table, in training mix) |
|---|---|---|---|---|
| US | VHF | 7,155 | 17% | 24% |
| Germany | VHF | 1,949 | 25% | 30% |
| Germany | LoRa | 1,614 | 48% | 55% |
| Italy | VHF | 1,117 | 21% | 27% |
| Canada | VHF | 912 | 19% | 20% |
| Italy | LoRa | 843 | 40% | 48% |
| Poland | VHF | 757 | 18% | 17% |
| Poland | LoRa | 657 | 42% | 50% |
| Spain | LoRa | 615 | 44% | 47% |
| France | LoRa | 546 | 45% | 49% |
| UK | VHF | 508 | 23% | 25% |
| France | VHF | 504 | 23% | 26% |
| Austria | VHF | 465 | 28% | 27% |
| UK | LoRa | 429 | 44% | 44% |
| Australia | VHF | 391 | 22% | 24% |
| Spain | VHF | 324 | 19% | 18% |
| US | LoRa | 291 | 44% | 48% |
| Norway | VHF | 235 | 19% | 20% |
| Czechia | VHF | 223 | 23% | 17% |
| Czechia | LoRa | 198 | 47% | 46% |
| Netherlands | VHF | 179 | 22% | 25% |
| Netherlands | LoRa | 163 | 48% | – |
| Austria | LoRa | 139 | 47% | – |
| Norway | LoRa | 130 | 43% | 40% |
| **All** | LoRa | 5,673 | 45% | 49% |
| **All** | VHF | 14,719 | 20% | 24% |

A new country starts about 4 points behind, not at zero (all LoRa 45% against 49%, all VHF 20%
against 24%).

## Why the countries differ: device type

The country gap is mostly a device-mix effect. LoRa packets only, global table, tested on held-out
stations, grouped by the first five characters of the destination (tocall). The description comes
from the APRS device registry (`aprsorg/aprs-deviceid`, maintained by Hessu, OH7LZB, CC BY-SA 2.0);
registry entries are six characters with `?` wildcards, so a five-character group can span several
entries.

| Device ID | Packets | Avg plain | Saved | Registry entry (vendor and model) | Class |
|---|---|---|---|---|---|
| APLG0 | 143 | 85 B | 64% | `APLG??`: OE5BPA, LoRa Gateway/Digipeater | digi |
| APLHM | 23 | 86 B | 60% | `APLHM?`: IW1CGW, LoRa Meteostation | wx |
| APLHI | 23 | 75 B | 56% | `APLHI?`: IW1CGW, LoRa IGate/Digipeater/Telemetry | digi |
| APLRG | 961 | 74 B | 51% | `APLRG?`: CA2RXU, ESP32 LoRa iGate | igate |
| APLC1 | 72 | 54 B | 49% | `APLC??`: DL3DCW, APRScube | none listed |
| APLOX | 59 | 78 B | 49% | `APLO??`: SQ9MDD, LoRa KISS TNC/Tracker | tracker |
| APLRF | 144 | 85 B | 47% | `APLRF?`: SQ2CPA, LoRa APRS | none listed |
| APLT0 | 30 | 67 B | 43% | `APLT??`: OE5BPA, LoRa Tracker | tracker |
| APLRT | 352 | 46 B | 32% | `APLRT?`: CA2RXU, ESP32 LoRa Tracker | tracker |
| APLET | 27 | 49 B | 13% | `APLETK`: DL5TKL, T-Echo | tracker |

The UK looks low on LoRa (44% against Germany's 55%) because its sample has many short tracker
beacons (23 of the 27 T-Echo packets are from the UK) and Germany's has long gateway packets (93 of
the 143 APLG0). Within a device type the countries are close. APLET is only 27 packets.

## Already-compressed beacons compress least

All LoRa packets in the data (not only the test split), by position format in the info field:

| Device | Packets | Position format | Avg total | Avg comment | Saved |
|---|---|---|---|---|---|
| APLRT (ESP32 LoRa Tracker) | 1,123 | 1,113 APRS-compressed (base-91), 10 other | 45 B | 6 B | 32% |
| APLET (T-Echo) | 28 | 26 compressed, 2 uncompressed | 47 B | 14 B | 13% |
| APLG0 (LoRa Gateway/Digipeater) | 474 | 471 uncompressed (digit string), 3 compressed | 79 B | 36 B | 64% |
| APLT0 (OE5BPA LoRa Tracker) | 255 | 225 uncompressed, 30 compressed | 87 B | 44 B | 43% |
| APLRG (ESP32 LoRa iGate) | 3,180 | 2,976 compressed, 130 uncompressed, 4 Mic-E, 70 other | 71 B | 34 B | 51% |

Trackers that already use APRS's own compact base-91 position and a short comment leave the header,
the callsign and a near-random position, which no predictor can shrink much, so the percentage is
lower. It is still about 14 bytes of a 45-byte packet. Comment length is a larger lever than the
position encoding (see `docs/rationale.md`, airtime table). The T-Echo figure rests on few packets.

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

* LoRa is near 49% and VHF near 24% of the whole frame, with a country-to-country spread that the
  current sample sizes cannot separate from noise.
* More data raised both figures by about 4 points; the table is data-limited, not size-limited.
* The weighting question (busiest countries count most) costs the quiet countries little at this
  size, and capping costs the total 2 to 3 points.
* Still missing: confidence intervals; countries with too few packets (Korea, Ukraine, South Africa,
  New Zealand, Argentina, Denmark, Finland); non-ASCII encodings by country (Japan, Russia); a
  traffic-weighted airtime-saved-per-day figure; the full 8-hour run (all times of day).

## Next

Re-run on the full 8-hour capture, add airtime saved per day and intervals, and replace this page.
