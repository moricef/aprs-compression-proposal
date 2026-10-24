# Compression by country: estimate from a three-hour capture

**Status: preliminary.** A three-hour read-only capture (10:21-13:21 PDT, an evening in Europe) plus a
5-minute pilot: **135,299 unique packets**. Every table on this page was rebuilt on all of it. Many
countries are still small cells, and the capture covers one evening in Europe only. Earlier versions of
this page used 5 minutes, 20 minutes and one hour. Do not quote these as final numbers.

## Method

* Read-only APRS-IS captures, filtered by callsign prefix for about 25 countries, one connection: a
  5-minute pilot plus three hourly files. 135,299 unique packets used: every LoRa packet (destination
  `APL*`) plus VHF packets an iGate heard over the air (`qAR`/`qAO`).
* Country assigned from the source callsign's prefix. Approximate: portable and travelling stations
  are misfiled.
* Train on 70% of stations (split by hash of the source callsign; 93,586 training packets), test on the
  other 30% (41,713 packets, 10,841 LoRa and 30,872 VHF).
* **LoRa** figures are the whole LoRa frame, including the 3-byte header, compared with the plain
  frame. **VHF** figures are the whole AX.25 frame, with only the info field compressed and 7 bytes
  per address, 2 control/PID, 2 FCS and 2 bytes of compression overhead assumed (as in
  `spec/07-ax25-binding.md`).
* Cells with fewer than 40 test packets are left out.
* The raw captures contain real callsigns and positions and are not published. Only these aggregates are.

## Table size now limits the result

With this much data only min-count 32 or higher fits the model format (its symbol offsets are 16-bit).
Pruning harder keeps the table small but costs saving. Table sizes are the trainer's flash estimate; v1 is
about 125 KB.

| Min-count | Table size | LoRa saved | VHF saved (whole frame) |
|---|---|---|---|
| 32 (smallest that fits) | 202 KB | 43% | 22% |
| 48 | 144 KB | 41% | 20% |

The ESP32 build had about 130 KB spare and the STM32 tracker about 148 KB, so the 144 KB table is the one
that is realistic on those devices. The per-country tables below use it.

**More data did not raise the saving at a fixed table size.** The same 41,713 test packets (stations held
out of every model) scored against tables trained on the first hour (40,239 packets) and on all three
hours (93,586 packets):

| Trained on | Min-count | Table size | LoRa saved | VHF saved |
|---|---|---|---|---|
| 1 hour | 12 | 226 KB | 46% | 23% |
| 1 hour | 16 | 178 KB | 44% | 22% |
| 1 hour | 20 | 148 KB | 43% | 21% |
| 1 hour | 24 | 127 KB | 42% | 21% |
| 1 hour | 32 | 99 KB | 40% | 19% |
| 1 hour | 48 | 69 KB | 38% | 17% |
| 3 hours | 32 | 202 KB | 43% | 22% |
| 3 hours | 40 | 168 KB | 42% | 21% |
| 3 hours | 48 | 144 KB | 41% | 20% |

At about the same table size the three hours do no better than the first hour, and slightly worse: 144 KB
from three hours gives 41% and 20%, against 43% and 21% for 148 KB from one hour. With three times the data
the format's 16-bit offsets only fit at min-count 32 or more, so the extra data forces harder pruning and
gains nothing. The saving is set by how large a table you keep. A better result needs a different model
(fields modelled separately, a larger context only where it pays) or a more compact table format, not more
packets of the same kind.

## Global table (144 KB), tested on stations it never saw

| Country | Mode | Packets | Plain B | Compressed B | Saved |
|---|---|---|---|---|---|
| US | VHF | 14,043 | 73.4 | 58.6 | 20% |
| Germany | VHF | 3,339 | 75.6 | 57.0 | 25% |
| Italy | VHF | 2,271 | 74.8 | 58.1 | 22% |
| Germany | LoRa | 1,914 | 67.0 | 35.7 | 47% |
| Canada | VHF | 1,812 | 68.6 | 58.3 | 15% |
| Italy | LoRa | 1,794 | 72.6 | 41.1 | 43% |
| Poland | LoRa | 1,633 | 81.6 | 45.4 | 44% |
| Spain | LoRa | 1,181 | 67.7 | 38.2 | 44% |
| Poland | VHF | 1,101 | 73.7 | 63.9 | 13% |
| Spain | VHF | 1,039 | 66.2 | 57.5 | 13% |
| Australia | VHF | 1,010 | 91.4 | 72.8 | 20% |
| UK | VHF | 769 | 69.0 | 53.8 | 22% |
| France | VHF | 762 | 81.0 | 61.7 | 24% |
| UK | LoRa | 735 | 64.6 | 41.2 | 36% |
| Austria | VHF | 715 | 73.6 | 54.1 | 26% |
| France | LoRa | 652 | 77.6 | 42.9 | 45% |
| Switzerland | VHF | 553 | 73.0 | 56.3 | 23% |
| Czechia | LoRa | 513 | 61.3 | 36.5 | 41% |
| Netherlands | VHF | 482 | 86.0 | 67.7 | 21% |
| Chile | LoRa | 456 | 75.6 | 46.4 | 39% |
| US | LoRa | 440 | 76.4 | 43.2 | 43% |
| Norway | VHF | 411 | 74.2 | 62.3 | 16% |
| Czechia | VHF | 367 | 67.5 | 54.1 | 20% |
| Mexico | LoRa | 340 | 54.5 | 35.2 | 35% |
| Belgium | VHF | 331 | 47.5 | 43.8 | 8% |
| Brazil | VHF | 299 | 66.5 | 58.9 | 12% |
| Japan | VHF | 291 | 55.4 | 48.6 | 12% |
| Mexico | VHF | 266 | 73.1 | 60.1 | 18% |
| Austria | LoRa | 251 | 154.2 | 137.9 | 11% |
| Sweden | VHF | 223 | 75.0 | 58.0 | 23% |
| Russia | VHF | 202 | 84.0 | 65.4 | 22% |
| Russia | LoRa | 199 | 58.3 | 44.4 | 24% |
| Denmark | VHF | 172 | 67.7 | 58.8 | 13% |
| Sweden | LoRa | 150 | 63.4 | 36.2 | 43% |
| Norway | LoRa | 142 | 58.7 | 36.0 | 39% |
| NewZealand | VHF | 137 | 74.4 | 60.6 | 19% |
| Switzerland | LoRa | 110 | 92.5 | 49.7 | 46% |
| Netherlands | LoRa | 92 | 80.9 | 46.9 | 42% |
| Argentina | VHF | 89 | 76.3 | 68.2 | 11% |
| Finland | VHF | 78 | 58.2 | 53.6 | 8% |
| Chile | VHF | 73 | 81.5 | 72.1 | 12% |
| Belgium | LoRa | 54 | 86.6 | 40.0 | 54% |
| Denmark | LoRa | 49 | 58.8 | 38.0 | 35% |
| **All** | **LoRa** | 10,841 | 73.0 | 42.8 | 41% |
| **All** | **VHF** | 30,872 | 73.5 | 58.8 | 20% |

* LoRa is near 41% and VHF near 20% of the whole frame at this table size.
* VHF ranges from the high teens to the high twenties by country and LoRa from the high thirties to the
  low fifties. Differences of a few points are within what these sample sizes produce.
* Mexico, Japan, Brazil and others have a figure now, though many cells are still a few hundred packets.

## Does the vote matter? Traffic-weighted against capped

Training capped at 600 packets per country (14,561 training packets), same held-out stations and table size:

| Country | Mode | Packets | Traffic-weighted | Capped at 600 |
|---|---|---|---|---|
| US | VHF | 14,043 | 20% | 11% |
| Germany | VHF | 3,339 | 25% | 16% |
| Italy | VHF | 2,271 | 22% | 13% |
| Germany | LoRa | 1,914 | 47% | 38% |
| Canada | VHF | 1,812 | 15% | 8% |
| Italy | LoRa | 1,794 | 43% | 32% |
| Poland | LoRa | 1,633 | 44% | 38% |
| Spain | LoRa | 1,181 | 44% | 37% |
| Poland | VHF | 1,101 | 13% | 7% |
| Spain | VHF | 1,039 | 13% | 6% |
| Australia | VHF | 1,010 | 20% | 12% |
| US | LoRa | 440 | 43% | 34% |
| **All** | LoRa | 10,841 | 41% | 34% |
| **All** | VHF | 30,872 | 20% | 12% |

Capping costs more than it did on less data: LoRa 41% to 34% and VHF 20% to 12%. A cap of 600 per country leaves 14,561
of 93,586 training packets, so the capped table sees much less. Training on pooled traffic, so that the
busiest countries count most, gives the larger total saving.

## A country the table has never seen

Trained on the other countries, tested on every packet of the held-out country:

| Country | Mode | Packets | Saved (country unseen) | Saved (global table) |
|---|---|---|---|---|
| US | VHF | 46,570 | 12% | 20% |
| Germany | VHF | 12,763 | 21% | 25% |
| Germany | LoRa | 7,242 | 42% | 47% |
| Italy | VHF | 6,171 | 18% | 22% |
| Canada | VHF | 6,116 | 14% | 15% |
| Italy | LoRa | 5,054 | 34% | 43% |
| Poland | LoRa | 4,727 | 36% | 44% |
| Poland | VHF | 3,993 | 14% | 13% |
| Spain | LoRa | 3,699 | 38% | 44% |
| Austria | VHF | 2,747 | 24% | 26% |
| France | VHF | 2,714 | 21% | 24% |
| France | LoRa | 2,377 | 40% | 45% |
| Spain | VHF | 2,299 | 16% | 13% |
| Australia | VHF | 2,285 | 19% | 20% |
| UK | VHF | 2,209 | 20% | 22% |
| UK | LoRa | 1,913 | 36% | 36% |
| US | LoRa | 1,698 | 37% | 43% |
| Norway | VHF | 1,615 | 16% | 16% |
| Czechia | VHF | 1,407 | 22% | 20% |
| Netherlands | VHF | 1,275 | 20% | 21% |
| Czechia | LoRa | 1,268 | 39% | 41% |
| Brazil | VHF | 1,261 | 8% | 12% |
| Switzerland | VHF | 1,258 | 22% | 23% |
| Japan | VHF | 1,203 | 16% | 12% |
| Mexico | LoRa | 971 | 32% | 35% |
| Sweden | VHF | 959 | 22% | 23% |
| Chile | LoRa | 948 | 33% | 39% |
| Austria | LoRa | 811 | 23% | 11% |
| Netherlands | LoRa | 770 | 39% | 42% |
| Belgium | VHF | 647 | 15% | 8% |
| Finland | VHF | 579 | 15% | 8% |
| Sweden | LoRa | 562 | 38% | 43% |
| Argentina | VHF | 551 | 13% | 11% |
| Mexico | VHF | 550 | 15% | 18% |
| NewZealand | VHF | 539 | 20% | 19% |
| Denmark | VHF | 438 | 16% | 13% |
| Norway | LoRa | 417 | 37% | 39% |
| Russia | VHF | 388 | 18% | 22% |
| Switzerland | LoRa | 382 | 45% | 46% |
| Russia | LoRa | 340 | 26% | 24% |
| Belgium | LoRa | 329 | 49% | 54% |
| Denmark | LoRa | 190 | 38% | 35% |
| Chile | VHF | 173 | 10% | 12% |
| Australia | LoRa | 170 | 35% | – |
| Argentina | LoRa | 164 | 41% | – |
| Finland | LoRa | 92 | 38% | – |
| Canada | LoRa | 87 | 39% | – |
| Brazil | LoRa | 40 | 41% | – |
| **All** | LoRa | 34,272 | 37% | 41% |
| **All** | VHF | 100,710 | 16% | 20% |

A new country starts about 4 points behind, not at zero: all LoRa 37% against 41%, all VHF 16% against 20%.

## Why the countries differ: device type

The country gap is mostly a device-mix effect. LoRa packets only, tested on held-out stations with the
144 KB table, grouped by the first five characters of the destination (tocall). The description comes from
the APRS device registry (`aprsorg/aprs-deviceid`, maintained by Hessu, OH7LZB, CC BY-SA 2.0); registry
entries are six characters with `?` wildcards, so a five-character group can span several entries.

| Device ID | Packets | Avg plain | Saved | Registry entry (vendor and model) | Class |
|---|---|---|---|---|---|
| APLHM | 175 | 87 B | 61% | `APLHM?`: IW1CGW, LoRa Meteostation | wx |
| APLG0 | 165 | 84 B | 55% | `APLG??`: OE5BPA, LoRa Gateway/Digipeater | digi |
| APLC1 | 369 | 54 B | 50% | `APLC??`: DL3DCW, APRScube | none listed |
| APLT0 | 197 | 73 B | 48% | `APLT??`: OE5BPA, LoRa Tracker | tracker |
| APLRG | 4,625 | 82 B | 47% | `APLRG?`: CA2RXU, ESP32 LoRa iGate | igate |
| APLRF | 1,193 | 87 B | 44% | `APLRF?`: SQ2CPA, LoRa APRS | none listed |
| APLOX | 402 | 81 B | 44% | `APLO??`: SQ9MDD, LoRa KISS TNC/Tracker | tracker |
| APLRT | 2,940 | 46 B | 29% | `APLRT?`: CA2RXU, ESP32 LoRa Tracker | tracker |
| APLET | 225 | 49 B | 7% | `APLETK`: DL5TKL, T-Echo | tracker |
| APLWS | 201 | 175 B | 7% | not in the registry list I fetched | – |

The UK looks low on LoRa because 210 of the 225 T-Echo packets are from the UK; Germany has 107 of the 165
APLG0 gateway packets. Within a device type the countries are close. APLWS is 201 packets from Austria,
averaging 175 bytes; the cause of its low saving was not examined.

## Already-compressed beacons compress least

All LoRa packets in the data (not only the test split), by position format in the info field:

| Device | Packets | Position format | Avg total | Avg comment | Saved (144 KB table) |
|---|---|---|---|---|---|
| APLRT (ESP32 LoRa Tracker) | 8,358 | 8,310 APRS-compressed (base-91), 48 other | 45 B | 6 B | 29% |
| APLET (T-Echo) | 301 | 278 compressed, 19 uncompressed, 4 other | 45 B | 12 B | 7% |
| APLG0 (LoRa Gateway/Digipeater) | 609 | 606 uncompressed (digit string), 3 compressed | 78 B | 36 B | 55% |
| APLT0 (OE5BPA LoRa Tracker) | 1,881 | 1,654 uncompressed, 227 compressed | 88 B | 45 B | 48% |
| APLRG (ESP32 LoRa iGate) | 14,203 | 13,276 compressed, 374 uncompressed, 36 Mic-E, 517 other | 78 B | 41 B | 47% |

Trackers that already use APRS's own compact base-91 position and a short comment leave the header, the
callsign and a near-random position, which no predictor can shrink much, so the percentage is lower. It is
still about 13 bytes of a 45-byte packet. Comment length is a larger lever than the position encoding (see
`docs/rationale.md`, airtime table).

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

## Should the table be retrained? (LoRa)

Model v1 was trained on 22,364 packets from the earlier captures (about an hour of LoRa traffic and short VHF
windows, 22-23 October), not on the 5-minute pilot. To see whether the three-hour capture justifies a new
table, v1 and retrained tables were scored on the same held-out LoRa packets, **excluding every station
that was in v1's training data** (2,203 LoRa packets from stations none of the tables had seen):

| Table | Training data | Size | LoRa saved |
|---|---|---|---|
| **Model v1 (current)** | 22,364 packets, LoRa-heavy | 125 KB | **43.8%** |
| Retrained | 3-hour LoRa packets only | 111 KB | 42.8% |
| Retrained | 3-hour LoRa packets only | 169 KB | 43.3% |
| Retrained | 3-hour LoRa + v1's LoRa packets | 116 KB | 43.3% |
| Retrained | 3-hour LoRa + v1's LoRa packets | 185 KB | 43.7% |
| Retrained | 3-hour LoRa + VHF (the mixed table of this page) | 144 KB | 36.4% |
| Retrained | 3-hour LoRa + VHF + v1's packets | 226 KB | 40.8% |

* **A retrained LoRa table does no better than v1 at the same size.** So v1 stays for LoRa.
* **A table that includes the (larger) VHF share does worse on LoRa** (36% to 41%) because the VHF text
  takes table space. The LoRa figures on this page's tables (41%) are for that mixed table; a LoRa-only
  table gets about 43%.
* On VHF the picture reverses: v1 saves 15.0% of the whole frame on stations it never saw, the mixed table
  18.0%. A separate table trained on VHF packets for the VHF binding is worth trying.
* Scoring v1 on all test packets instead gives 54.2% on LoRa, but that is inflated: v1 saw some of the
  same stations, and they beacon the same text again.

## Reading it

* At a table size that fits a small device (144 KB), LoRa is near 41% and VHF near 20% of the whole
  frame. A 202 KB table reaches 43% and 22%.
* More data of the same kind did not help at a fixed table size. The limits are the table format and the
  model, not the corpus size.
* The country-to-country spread is smaller than the effect of table size and device mix.
* A new country starts about 4 points behind. Capping the vote costs 7 to 8 points on this much data.
* Still missing: confidence intervals; countries with too few packets (Korea, Ukraine, South Africa,
  New Zealand, Argentina, Denmark, Finland); non-ASCII encodings by country (Japan, Russia); a
  traffic-weighted airtime-saved-per-day figure; other times of day (the capture covers three evening
  hours in Europe only).

## Next

Add airtime saved per day and intervals, and replace this page when a wider capture exists.
