# Compression by country: estimate from one hour of capture

**Status: preliminary. Superseded when the 8-hour multi-country capture has been analysed.** One hour
(10:21-11:21 PDT, an evening in Europe) plus a 5-minute pilot; many countries are still small cells.
Earlier versions of this page used 5 and 20 minutes. Do not quote these as final numbers.

## Method

* Read-only APRS-IS captures, filtered by callsign prefix for about 25 countries, one connection:
  a 5-minute pilot plus the first hour of a longer run. 57,883 unique packets used: every LoRa packet
  (destination `APL*`) plus VHF packets an iGate heard over the air (`qAR`/`qAO`).
* Country assigned from the source callsign's prefix. Approximate: portable and travelling stations
  are misfiled.
* Train on 70% of stations (split by hash of the source callsign), test on the other 30%.
* **LoRa** figures are the whole LoRa frame, including the 3-byte header, compared with the plain
  frame. **VHF** figures are the whole AX.25 frame, with only the info field compressed and 7 bytes
  per address, 2 control/PID, 2 FCS and 2 bytes of compression overhead assumed (as in
  `spec/07-ax25-binding.md`).
* Cells with fewer than 40 test packets are left out.
* The raw captures contain real callsigns and positions and are not published. Only these aggregates are.

## Table size now limits the result

With four times the data of the previous run, the table at the old pruning level (min-count 8) no
longer fits the model format, whose symbol offsets are 16-bit. Pruning harder fits, but the saving
falls. Table sizes are the trainer's flash estimate; v1 is about 125 KB.

| Min-count | Table size | LoRa saved | VHF saved (whole frame) |
|---|---|---|---|
| 12 (smallest that fits the format) | 226 KB | 47% | 24% |
| 24 | 127 KB | 43% | 21% |

The ESP32 build had about 130 KB spare and the STM32 tracker about 148 KB, so only the smaller table
is realistic on those devices.

**More data did not raise the saving at a fixed table size.** The same test set (the hour-1 held-out
stations) scored against a table trained on the earlier 20 minutes of data (15,936 training packets):

| Training data | Table size | LoRa saved | VHF saved |
|---|---|---|---|
| 20 minutes | 57 KB | 40% | 18% |
| 20 minutes | 82 KB | 42% | 20% |
| 20 minutes | 106 KB | 44% | 21% |
| 1 hour | 127 KB | 43% | 21% |
| 20 minutes | 153 KB | 46% | 23% |
| 1 hour | 226 KB | 47% | 24% |

At about the same size the hour of data does no better than the 20 minutes. The saving is set by
how large a table you keep, and the extra data mostly makes the same table bigger. A better result
needs a different model (fields modelled separately, a larger context only where it pays), not just
more packets of the same kind. The format's 16-bit offsets also cap what can be stored.

The per-country tables below use the 127 KB table (min-count 24), the size that fits a device.

## Global table (127 KB), tested on stations it never saw

| Country | Mode | Packets | Plain B | Compressed B | Saved |
|---|---|---|---|---|---|
| US | VHF | 5,664 | 73.6 | 57.9 | 21% |
| Germany | VHF | 1,236 | 76.5 | 55.1 | 28% |
| Italy | VHF | 1,047 | 74.9 | 57.3 | 23% |
| Germany | LoRa | 872 | 67.9 | 35.1 | 48% |
| Canada | VHF | 721 | 68.5 | 57.1 | 17% |
| Italy | LoRa | 693 | 73.2 | 40.3 | 45% |
| Poland | LoRa | 622 | 80.4 | 44.2 | 45% |
| Spain | VHF | 617 | 70.8 | 60.6 | 14% |
| Spain | LoRa | 509 | 66.7 | 37.2 | 44% |
| Poland | VHF | 509 | 74.1 | 62.6 | 16% |
| UK | VHF | 400 | 68.0 | 52.8 | 22% |
| Australia | VHF | 386 | 90.6 | 71.0 | 22% |
| France | VHF | 357 | 81.9 | 62.1 | 24% |
| Austria | VHF | 326 | 72.7 | 54.2 | 25% |
| UK | LoRa | 324 | 63.7 | 39.3 | 38% |
| France | LoRa | 295 | 74.1 | 39.8 | 46% |
| Czechia | LoRa | 277 | 57.5 | 34.4 | 40% |
| Czechia | VHF | 226 | 62.8 | 52.6 | 16% |
| Netherlands | VHF | 205 | 84.4 | 65.4 | 23% |
| Chile | LoRa | 201 | 74.5 | 46.0 | 38% |
| Switzerland | VHF | 200 | 72.4 | 54.7 | 24% |
| US | LoRa | 191 | 76.1 | 42.1 | 45% |
| Norway | VHF | 155 | 73.0 | 60.7 | 17% |
| Japan | VHF | 138 | 59.5 | 49.7 | 16% |
| Brazil | VHF | 137 | 75.0 | 63.2 | 16% |
| Mexico | LoRa | 120 | 55.4 | 35.7 | 36% |
| Russia | VHF | 110 | 86.2 | 68.3 | 21% |
| Belgium | VHF | 109 | 47.4 | 43.7 | 8% |
| Austria | LoRa | 105 | 144.2 | 120.4 | 16% |
| Sweden | VHF | 102 | 75.7 | 58.0 | 23% |
| Mexico | VHF | 93 | 80.1 | 60.9 | 24% |
| Denmark | VHF | 80 | 69.2 | 59.9 | 13% |
| Norway | LoRa | 77 | 58.6 | 36.8 | 37% |
| Russia | LoRa | 70 | 66.3 | 44.7 | 33% |
| Sweden | LoRa | 68 | 62.1 | 35.3 | 43% |
| NewZealand | VHF | 65 | 73.7 | 60.7 | 18% |
| Switzerland | LoRa | 61 | 87.4 | 44.3 | 49% |
| Netherlands | LoRa | 49 | 78.0 | 45.1 | 42% |
| Finland | VHF | 49 | 59.7 | 52.8 | 12% |
| **All** | **LoRa** | 4,656 | 72.0 | 41.1 | 43% |
| **All** | **VHF** | 12,988 | 73.8 | 58.1 | 21% |

* LoRa and VHF both land a few points below the 20-minute run, because of the harder pruning.
* VHF ranges from the high teens to the high twenties by country, LoRa from the high thirties to the
  low fifties. Differences of a few points are within what these sample sizes produce.

## Does the vote matter? Traffic-weighted against capped

Training capped at 600 packets per country (11,405 training packets), same held-out stations and
table size:

| Country | Mode | Packets | Traffic-weighted | Capped at 600 |
|---|---|---|---|---|
| US | VHF | 5,664 | 21% | 14% |
| Germany | VHF | 1,236 | 28% | 22% |
| Italy | VHF | 1,047 | 23% | 18% |
| Germany | LoRa | 872 | 48% | 43% |
| Canada | VHF | 721 | 17% | 11% |
| Italy | LoRa | 693 | 45% | 37% |
| Poland | LoRa | 622 | 45% | 41% |
| Poland | VHF | 509 | 16% | 11% |
| Australia | VHF | 386 | 22% | 14% |
| US | LoRa | 191 | 45% | 38% |
| **All** | LoRa | 4,656 | 43% | 39% |
| **All** | VHF | 12,988 | 21% | 15% |

Capping costs about 4 points overall (LoRa 43% to 39%, VHF 21% to 15%). Training on
pooled traffic, so that the busiest countries count most, gives the larger total saving.

## A country the table has never seen

Trained on the other countries, tested on every packet of the held-out country:

| Country | Mode | Packets | Saved (country unseen) | Saved (global table) |
|---|---|---|---|---|
| US | VHF | 18,964 | 14% | 21% |
| Germany | VHF | 5,214 | 23% | 28% |
| Germany | LoRa | 3,495 | 43% | 48% |
| Italy | VHF | 2,867 | 19% | 23% |
| Canada | VHF | 2,472 | 15% | 17% |
| Italy | LoRa | 2,060 | 36% | 45% |
| Poland | LoRa | 1,867 | 38% | 45% |
| Poland | VHF | 1,852 | 16% | 16% |
| Spain | LoRa | 1,645 | 39% | 44% |
| Austria | VHF | 1,308 | 24% | 25% |
| France | LoRa | 1,241 | 42% | 46% |
| France | VHF | 1,212 | 21% | 24% |
| UK | VHF | 1,170 | 20% | 22% |
| Spain | VHF | 969 | 16% | 14% |
| Australia | VHF | 955 | 20% | 22% |
| UK | LoRa | 879 | 39% | 38% |
| Norway | VHF | 731 | 17% | 17% |
| US | LoRa | 696 | 38% | 45% |
| Czechia | VHF | 647 | 21% | 16% |
| Czechia | LoRa | 602 | 40% | 40% |
| Japan | VHF | 549 | 17% | 16% |
| Netherlands | VHF | 547 | 20% | 23% |
| Switzerland | VHF | 536 | 23% | 24% |
| Brazil | VHF | 416 | 12% | 16% |
| Sweden | VHF | 408 | 22% | 23% |
| Netherlands | LoRa | 406 | 41% | 42% |
| Austria | LoRa | 388 | 28% | 16% |
| Mexico | LoRa | 319 | 35% | 36% |
| Finland | VHF | 286 | 16% | 12% |
| Belgium | VHF | 285 | 12% | 8% |
| Mexico | VHF | 230 | 17% | 24% |
| Norway | LoRa | 222 | 39% | 37% |
| Sweden | LoRa | 214 | 40% | 43% |
| Switzerland | LoRa | 191 | 47% | 49% |
| Belgium | LoRa | 167 | 49% | – |
| Australia | LoRa | 79 | 38% | – |
| **All** | LoRa | 14,563 | 39% | 43% |
| **All** | VHF | 41,618 | 17% | 21% |

A new country starts about 4 points behind, not at zero (all LoRa 39% against 43%, all VHF 17% against 21%).

## Why the countries differ: device type

The country gap is mostly a device-mix effect. LoRa packets only, tested on held-out stations with the
**226 KB table (min-count 12)**, grouped by the first five characters of the destination (tocall); the
per-country tables above use the 127 KB table, so the percentages here are higher. The description
comes from the APRS device registry (`aprsorg/aprs-deviceid`, maintained by Hessu, OH7LZB, CC BY-SA
2.0); registry entries are six characters with `?` wildcards, so a five-character group can span
several entries.

| Device ID | Packets | Avg plain | Saved | Registry entry (vendor and model) | Class |
|---|---|---|---|---|---|
| APLHM | 72 | 86 B | 66% | `APLHM?`: IW1CGW, LoRa Meteostation | wx |
| APLG0 | 157 | 84 B | 61% | `APLG??`: OE5BPA, LoRa Gateway/Digipeater | digi |
| APLC1 | 169 | 54 B | 56% | `APLC??`: DL3DCW, APRScube | none listed |
| APLRG | 2,096 | 79 B | 51% | `APLRG?`: CA2RXU, ESP32 LoRa iGate | igate |
| APLT0 | 66 | 74 B | 51% | `APLT??`: OE5BPA, LoRa Tracker | tracker |
| APLOX | 167 | 78 B | 50% | `APLO??`: SQ9MDD, LoRa KISS TNC/Tracker | tracker |
| APLRF | 435 | 88 B | 49% | `APLRF?`: SQ2CPA, LoRa APRS | none listed |
| APLRT | 1,180 | 46 B | 33% | `APLRT?`: CA2RXU, ESP32 LoRa Tracker | tracker |
| APLWS | 73 | 175 B | 15% | not in the registry list I fetched | – |
| APLET | 88 | 49 B | 12% | `APLETK`: DL5TKL, T-Echo | tracker |

The UK looks low on LoRa because 81 of the 88 T-Echo packets are from the UK; Germany has 104 of the
157 APLG0 gateway packets. Within a device type the countries are close. APLWS is 73 packets from
Austria, averaging 175 bytes; the cause of its low saving was not examined.

## Already-compressed beacons compress least

All LoRa packets in the data (not only the test split), by position format in the info field:

| Device | Packets | Position format | Avg total | Avg comment | Saved (226 KB table) |
|---|---|---|---|---|---|
| APLRT (ESP32 LoRa Tracker) | 3,494 | 3,476 APRS-compressed (base-91), 18 other | 44 B | 5 B | 33% |
| APLET (T-Echo) | 129 | 121 compressed, 8 uncompressed | 45 B | 11 B | 12% |
| APLG0 (LoRa Gateway/Digipeater) | 525 | 522 uncompressed (digit string), 3 compressed | 79 B | 36 B | 61% |
| APLT0 (OE5BPA LoRa Tracker) | 776 | 713 uncompressed, 63 compressed | 89 B | 45 B | 51% |
| APLRG (ESP32 LoRa iGate) | 6,638 | 6,201 compressed, 220 uncompressed, 7 Mic-E, 210 other | 75 B | 38 B | 51% |

Trackers that already use APRS's own compact base-91 position and a short comment leave the header,
the callsign and a near-random position, which no predictor can shrink much, so the percentage is
lower. It is still about 14 bytes of a 44-byte packet. Comment length is a larger lever than the
position encoding (see `docs/rationale.md`, airtime table).

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

* At a table size that fits a small device (about 127 KB), LoRa is near 43% and VHF near 21% of the
  whole frame. A 226 KB table reaches 47% and 24%. The 49% and 24% from the 20-minute run were at a
  153 KB table and flatter test data.
* More data of the same kind did not help at a fixed table size. The limits now are the table
  format and the model, not the corpus size.
* The country-to-country spread is smaller than the effect of table size and device mix.
* A new country starts a few points behind; capping the vote costs about 4 points overall.
* Still missing: confidence intervals; countries with too few packets; non-ASCII encodings by country
  (Japan, Russia); a traffic-weighted airtime-saved-per-day figure; the full 8-hour run (all times of day).

## Next

Re-run on the full 8-hour capture, add airtime saved per day and intervals, and replace this page.
