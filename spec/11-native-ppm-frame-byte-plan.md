# Proposal draft 11 - Hybrid B: native frame with a PPM-coded content field (byte plan)

| | |
|---|---|
| Status | **First draft of the byte plan F4MLV asked for in issue 7 of moricef/LoRa_APRS_Tracker (comments 14 to 17). Written by K6ATV, not yet seen or agreed by F4MLV. No native encoder implements it; the PPM payload's integrity rules and the RXT tail are tested (below).** |
| Builds on | moricef's native frame, draft 0 (`docs/LORA_APRS_NATIVE_FRAME_DRAFT0.md`, `moricef/LoRa_APRS_iGate`, branch `rxt`); hybrid B of `tools/hybrid/` and its tables (`tools/hybrid/tables/`); the PPM payload rules below (ending, CRC-16, re-encode check) |
| Replaces | nothing that was ever on the air. An earlier draft of this repository defined a standalone whole-text frame (`3C FF 81`); that was never the plan and has been withdrawn. PPM of the whole packet is **not** the plan: a relay would have to decompress, edit the path and recompress |

## What this is

This is **hybrid B** of `tools/hybrid/`: the native part (source, destination, path, RXT) is structured so a relay can edit it; the PPM part is only the content, which a relay copies unchanged.
In the terms of the thread (issue 7, comments 5 to 17): hybrid B + destination table v2 + lossless draft-0 position block, with the comment-only table when the position is in the native block. Relays update the path and append RXT without decompressing.

Agreed with F4MLV as the scope of the first experimental common format (comment 16, confirmed in 17): native source, destination and path; the versioned destination table; lossless draft-0 positions; standalone PPM for the remaining content; extended callsigns and SSIDs, content and the link between each measurement and its relay preserved exactly; receivers drop any frame whose format, model or table version they do not know (append-only tables).
**Not in the first format:** primed mode (and so the 1-byte station id), rounded or packed positions (`PACKPOS`, an optional tracker setting later), WSPR-style callsigns, the capability beacon and the power/backhaul questions, Mic-E as a native position (Mic-E travels as text).

## Frame

    3C FF 02 | header | source | destination | path... | [position block] | content field | [RXT block]

`3C FF 02`, the header byte, the address elements, the path and the position block are those of draft 0. This plan adds: PPM-coded content (two header values), the destination table, a rule for when to fall back to raw text, and the place of the RXT block.

### Header byte (draft 0 layout, additions in bold)

| Bits | Draft 0 | This plan |
|---|---|---|
| 7-4 | number of path elements, 0 to 15 | same |
| 3 | an RXT block follows the path | **An RXT block ends the frame, for every kind (0 to 3).** This moves the block for kinds 0 and 1 (see Open) |
| 2 | the text is Huffman coded | PPM kinds: reserved, must be 0 |
| 1-0 | information kind: 0 raw, 1 position (2 and 3 unused) | **2 = information field as text, PPM coded. 3 = position block, then the comment PPM coded** |

F4MLV proposed using the two unused information-kind values to say which coder was used (comment 6), so this is his own placement. A receiver that does not know kind 2 or 3 MUST drop the frame and never show it as text.

### Content field, PPM kinds (2 and 3)

    V        1 byte    version of the table set (below); a receiver drops a version it does not hold
    L        1 byte    number of text bytes coded, 1..255
    CRC      2 bytes   CRC-16/X-25 (below) of the information field as the iGate would upload it, low byte first
    payload  n bytes   range coder bytes of spec 01, ending as in "PPM payload rules" below

* Kind 2 codes the whole information field (`:INFO` of the harness). Kind 3 follows a position block and codes only the comment that remains.
* The CRC covers the information field rebuilt from the position block and the decoded text. A relay rewrites the path but not the information field, so it never recomputes the CRC.
* The payload length is not sent; the receiver derives it (Receiver, step 3).

**Tables.** Kind 2 uses the *info-native* table and kind 3 the *comment-only* table, because a comment after a position looks different from a whole information field. `V` names the pair. Version numbers are allocated by this spec, append-only:

| V | Info-native table | Comment-only table | Status |
|---|---|---|---|
| 1 | `tools/hybrid/tables/model_hybridB_info_native.c` (sha256 `440a1ec2...dc270`, 77 KB) | `tools/hybrid/tables/model_hybridB_comment.c` (sha256 `d1262b44...eaf5`, 75 KB) | experimental: trained on 70% of the stations of one APRS-IS capture (callsign-hash buckets 3 to 9), 174,170 and 105,092 packets |

The tables' own header says "model version 2", which they share with the whole-packet and hybrid A tables of the same build; that number does not identify a layout, which is why `V` is allocated here. A deployed first format needs a frozen set trained on all stations and on several networks' data
(`train_layout_model.py --base-counts`, see the harness README), with a new `V`. The experimental set above must not be treated as the final one.

### Fallback to raw text

The encoder uses a PPM kind only when the content field (`V`, `L`, CRC and payload: `4 + n` bytes) is shorter than the raw text it replaces. Otherwise it sends kind 0 or 1 exactly as draft 0 (raw or Huffman text). About a third of information-only fields do not shrink.
After a position block, an empty comment sends no text (kind 1 with nothing after the position). These rules are the harness's `PLAINTEXT=1` and are a size rule only; any receiver decodes all four kinds.

### Destination (versioned table, two bits reserved)

The destination is coded in one byte, `ff iiiiii`:

* `ff`, the top **two bits, are reserved**. A sender sets them to 0 and a receiver ignores them, so a later version can give them a meaning (for example "this station decodes hybrid B" or "this station appends RXT") without changing the table or the frame.
  Whatever they come to mean describes the originating station, not a relay in the path.
* `iiiiii`, the low six bits, give 64 codes. **Codes 0 to 61 name the 62 most common destinations of that table version, in descending order of popularity** (code 0 is the most common), so the common ones are found first by a simple search. Code 62 is reserved (a receiver drops a frame that uses it). **Code 63 is the escape:** after it comes the draft-0 address element (5 bytes for an ordinary destination, so 6 in all; this is also the cost of a Mic-E destination, which draft 0 carries as a 5-byte element).
* A destination that has no direct code is not in the table: it escapes. There are no patterns and no second tier of 2-byte codes. In both measured captures registry patterns almost never fired (0 packets of 8,062 in Ariège and 2 of 14,305 in ours).
* **Each table version fills the 62 destination codes, in order of popularity, with the destinations that are most common then,** and need not keep the codes of the version before; the version names the list. A destination used only once is never given a code.

Measured on our 14,305 real packets (73 distinct destinations): the 62 most common as direct codes cover 99.9% of packets (the first five, `APLRG1`, `APLRT1`, `APLWS2`, `APLG01`, `APLRFD`, cover 88.8%), a mean of 1.0038 bytes per packet, against 1.015 for the v2 list of the harness (85 exact entries, 28 patterns). That ranking is made on the same data it is tested on, so it is optimistic; a real list is ranked on training stations and checked on held-out ones.
The harness's v2 list ([`tools/hybrid/tables/tocall_table_v2.txt`](../tools/hybrid/tables/tocall_table_v2.txt), sha256 `e700581b...f59aff`, 85 exact destinations and 28 patterns) is the starting data, not the final list: its entries seen once or twice in the captures (`APU25N`, `BEACON`) would not earn a direct code, and the few `APL...` destinations seen here that it lacks (`APSTKI` 20 packets, `APLM2D` 6, `APLGMT` 5, `APLHT2` 4) would compete for the 62 codes.
Exact-entry coverage of the v2 list on the French capture (Ariège, 8,062 packets) is 85.1% with v1 and 97.3% with v2; on the Arizona sample (2,659 packets) 99.8% and 100%; Mic-E is 2.5% in Ariège and always escapes.

**Which table version is in use has no field yet** (see Open): the direct codes mean different destinations in different versions.

### RXT block (header bit 3 set, every kind)

    tuple 1 | tuple 2 | ... | tuple k | mask          the last byte of the frame is the mask

* A tuple is 4 bytes as in draft 0: RSSI, SNR, frequency offset, time to hop, each the RXT character minus 33, so each byte is 0 to 93.
* The mask has **bit 7 set** as a marker. Bit j (0 to 6) set means that path element j (0 is the first element after the destination) carries a measurement. Tuples are in ascending order of j. `k` is the number of mask bits 0 to 6 that are set.
* The block is `4k + 1` bytes. Bits at or above the path element count are not allowed. **Each tuple byte must not exceed its field's maximum** (RSSI 89, SNR 88, frequency offset 93, time to hop 93, as far as the RXT documents say; a receiver drops a frame with a tuple outside them): a free check on the tuples, which random damage fails about 98% of the time.

The tuples have no check of their own beyond the LoRa CRC, which already covers the whole frame at the level of VHF APRS (about 1 in 65,536); a block CRC would be recomputed by every relay and so add nothing the LoRa CRC does not give per hop.
The marker makes the block safe against truncation: a tuple byte is never 0x80 or more, so after a cut inside the block the last byte can never be mistaken for a mask (tested below). It costs one path element against draft 0 (7 instead of 8).
This placement (end of frame, mask byte last) is K6ATV's proposal in issue 7 (comment 5): a relay appends without moving the content, whatever the content is, and the content (PPM payload or text) is never touched by RXT, which is already compact binary and would not shrink. The same placement applies to every kind, so a relay needs one rule.
For kinds 0 and 1 this **changes draft 0**, which puts the block after the path and has no check; it is a proposal that needs F4MLV's agreement, because his firmware sends and receives that layout.
For kinds 0 and 1 the text has no length of its own when it is raw (draft 0: the text is the rest of the frame), so the receiver finds the block from the end: header bit 3 says the block exists, the last byte is the mask, and the text is everything before the block.

A relay builds its outgoing frame from the front: new header and path, then the position block and content (a content field, or text for kinds 0 and 1) copied byte for byte, then the old tuples, its own tuple and the new mask. If there was no block it creates one (tuple, mask) and sets header bit 3.
A relayed frame grows by **5 bytes for the first relay** (4 for the tuple, 1 for the mask) and 4 for each further one.

## PPM payload rules (normative)

These apply to the `payload` of the content field. The model, the range coder and every probability are those of spec 01 with the table the kind and `V` select.

**Why.** A payload cut short before it is sent carries a valid LoRa CRC, because the sender computes the CRC after the cut, and a decoder that pads missing input with zeros turns the cut payload into plausible text.
Measured on 14,298 real full frames, the unmodified decoder accepted every frame cut by 1 to 6 bytes, with the right length and a wrong tail. The rules below make the end of the payload unambiguous and make the receiver prove that the payload is the one an encoder would have sent.

**Ending.** After the last text byte is encoded, the encoder holds `low` (up to 33 bits, with a carry pending in bit 32) and `range`. Then:

1. If `low >= 2^32`: add 1 to the payload so far (propagating the carry through the bytes already written) and set `low = low - 2^32`.
2. Let `hi = low + range - 1`.
3. For `nb = 1, 2, 3, 4` in turn: let `blk = 2^(8*(4-nb))` and `v = ceil(low / blk) * blk` (the first multiple of `blk` that is `>= low`). If `v + blk - 1 <= hi`, stop.
   (If a decoder reads `nb` bytes and then anything at all, the 32-bit value it sees is some number from `v` to `v + blk - 1`; the condition says that every such number lies inside `[low, hi]`.
   `nb = 0` can never succeed, because `range < 2^32`. Because `range >= 2^24` after the last normalisation, `nb` is in fact always 1 or 2; 3 and 4 are never reached.)
4. If `v >= 2^32`: add 1 to the payload so far and set `v = v - 2^32`.
5. Append the top `nb` bytes of `v` (most significant first).
6. **Do not strip trailing zero bytes.** A zero last byte is sent.

The result is the fewest bytes after which **any** following bytes decode to the same text. Spec 01's ending (the fewest bytes assuming zeros follow, then stripping zeros) is replaced for these frames; the new ending is on average 0.25 bytes longer.
The encoder is deterministic: the same text always gives the same bytes, and a second implementation MUST produce them exactly, because the receiver check depends on it.

**CRC-16.** CRC-16/X-25, the AX.25 frame check: polynomial 0x1021 reflected (0x8408), initial value 0xFFFF, final xor 0xFFFF; the CRC of the ASCII string `123456789` is 0x906E. It is sent low byte first.

**Receiver equivalence.** The receiver decodes `L` text bytes, checks the CRC-16, re-encodes the text with the rules above and requires the result to equal the payload exactly (same length, same bytes). Decoding and re-encoding use the same probabilities for the same character,
so a receiver MAY re-encode while it decodes (one pass over the model instead of two); it must give the same accept and drop answers as the two-pass form. The CRC is checked before the comparison, so a damaged frame is dropped as early as possible.

**Measured.** Cut frames: 0 wrongly accepted at every cut size (1 to 16 bytes), 13,731 to 14,293 per size, with and without a relay tail. Damage inside the payload after the LoRa CRC has passed (1, 2, 3 and 8 flipped bits, random payload): the re-encode check alone leaves 1 in 208 to 1 in 2,286,
the CRC-16 alone about 1 in 52,000 to 1 in 142,000, and the two together **0 of about 2.85 million**, which meets the 1 in 65,536 of the AX.25 frame check. Two independent implementations (C and Python, the second written from the rules alone) agree on all 14,305 real texts and on 11,996 damaged frames.
Details and the other results are in [`docs/truncated-payloads-and-integrity.md`](../docs/truncated-payloads-and-integrity.md).

## Receiver (normative)

1. Drop the frame unless it starts `3C FF 02` and the information kind, the version `V` and the table versions are known.
2. Parse header, addresses and path, and the position block (kind 3), as draft 0.
3. If header bit 3 is set: the last byte is the mask. Drop the frame unless bit 7 is set, `k >= 1`, every set bit is below the path element count, and at least `4k + 1` bytes remain after the position block and, for PPM kinds, after the content CRC. The RXT block is the last `4k + 1` bytes, and **the frame is dropped if a tuple byte exceeds its field maximum**; the payload (PPM kinds) or the text (kinds 0 and 1) is everything between the end of the position block or CRC and the block.
   If bit 3 is clear, there is no block and the payload or text is everything after.
4. Decode `L` text bytes from the payload with the range decoder (the table chosen by the kind), rebuild the information field, and drop the frame unless its CRC-16 matches.
5. Re-encode the `L` text bytes with the sender's rules ("PPM payload rules", Ending). Drop the frame unless the result is **exactly** the payload (same length, same bytes).
6. Only an accepted frame may be shown, uploaded, relayed, or kept for later use.

Steps 4 and 5 may be done in one pass over the model ("PPM payload rules", Receiver equivalence). Because the payload lies exactly between the CRC and the RXT block, there is no tail to guess about; a damaged or cut frame changes where the payload ends and step 5 fails.

## Relay (draft)

A relay forwards a frame it has accepted (Receiver, steps 1 to 6) with its own path update and its own RXT measurement, **without decoding the content**. The content is never converted: a Mic-E packet is forwarded as it was received (see Mic-E below).

1. **Rewrite the front.** Decode the header, addresses and path of the native part to the TNC2 path, apply the usual digipeater rule (for example `WIDE1-1` becomes the relay's callsign with the used mark; `WIDEn-N` loses one hop), and encode the header, addresses and path again. This is cheap and needs no PPM decoding. The destination is copied as received
   (a Mic-E destination is its 6 latitude characters, so it is the escape followed by the 5-byte element). *Draft 0 leaves a native relay state undefined and works on the TNC2 path; this step follows that.*
2. **Copy the rest unchanged**, byte for byte: the position block and the content (a content field with its CRC-16 and payload, or raw or Huffman text).
3. **Find the index of the relay's own path element** after step 1, set that bit in the mask, and insert the relay's tuple in ascending order of index. Tuples already in the block keep their meaning only if their path elements keep their index; a relay that changes the number or order of elements before them must move the mask bits with them (draft 0 moves measurements by full callsign).
4. **Fill the tuple at transmit time.** RSSI, SNR and frequency offset are those measured when the frame was received. Time to hop runs from the end of reception to the end of the relay's own transmission, including queue and backoff, so it is computed just before transmission starts from the queue time plus the time on air, which the frame length fixes.
5. **A relay that cannot finish the tuple in time sends none** (a frame without a block is valid) instead of sending a wrong one.
6. **Drop duplicates** on the decoded APRS content (source, destination and information field), as draft 0 does; the path and RXT differ between copies and are ignored.
7. **An iGate** decodes the frame to TNC2 for APRS-IS, MQTT and TNC output, removing the RXT block, and uploads the same source, destination and information field a text frame would have given.

### Mic-E

Mic-E is not decoded or converted by this plan. A Mic-E packet is carried as received: the destination as the escape plus the 5-byte element (6 bytes, since its characters are the position), and the information field as raw text (kind 0), or as a PPM content field (kind 2) only if that is shorter, which is unusual because the field is printable binary. Relays forward it exactly as above. Converting Mic-E to a native position
would save only 1 to 3 bytes per Mic-E packet, almost all of it in the destination, and would change the bytes of the packet; it is not worth that and is not part of this plan.
If the sender used the destination SSID as a path code, the relay applies the usual rule to it; whether draft 0 does so is not checked.

## Size

Published figures for this combination, bytes saved against the plain frame including the 3-byte prefix, full frames only (N=1), no RXT, harness results and not RF measurements:

| Data | Combination | Saved |
|---|---|---|
| K6ATV APRS-IS capture, 93,145 held-out packets | hybrid B + tocall table + lossless draft-0 position | 46.2% |
| same | + packed position (optional, not in this plan) | 47.2% |
| F4MLV Ariège, 8,062 packets (comment 15) | table v2 + lossless draft-0 position, matched tables | 39.3% |
| F4MLV Arizona, 2,659 packets (comment 15) | same | 43.8% |

These do not include `V` and the CRC-16: the harness costs the PPM text with the native frame's own length byte, so the content field here is **3 bytes longer** than the harness assumes. With an average plain frame of 72 to 110 bytes that is roughly 3 to 4 points off each figure
(an estimate from the average frame lengths; not measured). Dropping `V` (folding the version into the header) would give one byte back. The 50.2% measured earlier on the whole-text test frame is the whole-packet figure and is not the figure for this plan.

## Integrity: what is measured

The payload rules were tested first on a whole-text test frame (withdrawn as a format; the results concern the payload and carry over): cut frames, 0 wrongly accepted at every size; with the CRC-16 and the re-encode check together 0 of about 2.85 million damaged frames accepted. Those runs used the whole-packet table of spec 01 on whole texts. The same checks were repeated with the hybrid B tables on real texts
([`results-hybridB-payload.txt`](../tools/integrity/results-hybridB-payload.txt)): the info-native table on 14,305 information fields (12,166 smaller than raw) and the comment-only table on 11,367 comments after a position (10,704 smaller than raw; the position is stripped with regular expressions, so this is an approximation of the draft-0 comment). For both tables the ending is deterministic and independent of what follows (328,482 of 328,482 and 289,008 of 289,008 decodes right with 0 to 8 following bytes),
the receiver accepts every intact frame with and without a 6-byte tail, **0 cut frames are wrongly accepted at any cut size (1 to 16 bytes)**, 0 damaged frames (1, 2, 3, 8 flipped bits, random payload) are accepted, and the one-pass receiver agrees with the two-pass one. The frame header of those tests is the withdrawn test frame's (6 bytes), not this plan's (4 bytes plus the native fields); the checks concern the payload.
Larger damage runs with the shipping combination (CRC-16 and re-encode check), 10 damaged copies per text and damage type ([`results-hybridB-stress.txt`](../tools/integrity/results-hybridB-stress.txt)): **0 wrongly accepted of about 608,000 trials (info-native table) and about 533,000 (comment-only table)**.
The re-encode check alone leaves 1 in 181 to 1 in 2,644 (info-native) and 1 in 104 to 1 in 204 (comment-only: short comments leave less for the check to catch), and the CRC-16 alone about 1 in 40,000 to 1 in 120,000, so the two are needed together; a 95% upper bound for 0 of 1.14 million is about 1 in 380,000.

**Target.** The undetected-error rate must be no worse than VHF APRS, whose AX.25 frame check is a CRC-16: about 1 in 65,536. The LoRa payload CRC (also 16 bits) covers every byte of the frame and already gives that level for random channel damage; for end-to-end damage it is multiplied by whatever our own checks let through.
The LoRa CRC cannot see two things that issue 7 raised: a **payload cut before transmission** (the CRC is computed after the cut) and **a receiver decoding against the wrong reference or the wrong table** (the frame arrives intact). The checks in this plan address those, and nothing else needs a byte:

* **Truncation of the PPM payload:** the re-encode check. Alone, without any CRC, 0 of about 230,000 cut frames per cut size were accepted (286,100 texts, [`results-hybridB-reencode-alone.txt`](../tools/integrity/results-hybridB-reencode-alone.txt)), so the rate is below about 1 in 77,000 at 95% confidence.
* **Wrong table, wrong model, a second implementation that differs:** the re-encode check cannot see these (decode and re-encode use the same wrong table; against a wrong reference it accepted 46% of wrong decodes), so the **content CRC-16** is kept: it is computed by the origin over the text, relays never touch it, and it catches the decode at about 1 in 65,536. It covers the position and the comment together.
* **Truncation inside the RXT block:** the marked mask byte (below). **A damaged tuple:** the range check, which random damage fails about 98% of the time, and otherwise the LoRa CRC alone, which is VHF level.
* **Everything else** (header, addresses, path, position, raw text) is covered by the LoRa CRC, which is VHF level. Primed mode, where a stale reference makes an intact frame decode wrongly, is not in this plan.

Spec 11 spends 2 bytes on this (the content CRC-16) and none on the RXT block.

**The RXT block** ([`tools/integrity/rxt_tail_test.py`](../tools/integrity/rxt_tail_test.py)), tested on 600 real packets carrying 0 to 3 tuples, with the rules above:

* **Cut frames, 1 to 16 bytes off the end: 0 of 21,564 accepted** with the marked mask byte ([`results-rxt-tail-marked.txt`](../tools/integrity/results-rxt-tail-marked.txt)).
  Without the marker (a plain mask in the last byte, [`results-rxt-tail-plain.txt`](../tools/integrity/results-rxt-tail-plain.txt)) 73 of the same cut frames were accepted with a wrong mask (cuts of 4 or 8 bytes inside the block, 17 to 34 of 600 for the cut sizes concerned), because a left-over tuple byte can look like a valid mask. That is why the marker is in the plan.
* **Damage inside the block** (1 or 3 flipped bits, 36,000 trials): the range check lowers the share accepted from 87% and 68% to 68% and 32% (one and three tuples; [`results-rxt-tail-marked-range.txt`](../tools/integrity/results-rxt-tail-marked-range.txt)). Flips of a few bits are already caught by the LoRa CRC; the range check matters for random-looking damage. A CRC-16 over the block (tested: 0 accepted, and every 1- to 3-bit flip detected) was **not adopted**: it would be recomputed by every relay and
  adds nothing the LoRa CRC does not give per hop ([`results-rxt-crc.txt`](../tools/integrity/results-rxt-crc.txt)).

## Receiver time (ESP32, measured)

One-pass receiver (decode, CRC-16, re-encode compare) with the hybrid B tables, fast `build_freq`, mean per frame over the texts PPM can shrink ([`results-esp32-timing.txt`](../tools/integrity/results-esp32-timing.txt)):

| Board | Information field (kind 2) | Comment after a position (kind 3) |
|---|---|---|
| LILYGO LoRa32 v2.1 (ESP32-PICO-D4) | 4.2 ms (3.5 ms with the key tables copied to RAM) | 2.0 ms (1.8 ms) |
| LILYGO T-Deck (ESP32-S3) | 2.8 ms (2.7 ms) | 1.4 ms (1.4 ms) |

The earlier whole-packet figures (6.3 ms and 5.0 ms) are higher because the text is longer. The re-encode check adds nothing measurable in the one-pass receiver; a two-pass receiver roughly doubles the time. These are codec-only test sketches on 48 fabricated packets (28 information fields and 18 comments shrink), without WiFi, not the iGate or tracker firmware.

## Not yet measured

* **The RXT block with raw text (kinds 0 and 1).** The tail tests used PPM payloads. Raw or Huffman text has no check of its own (draft 0 relies on the LoRa CRC), so a cut that removes the whole block and part of the text is caught only if the new last byte fails the mask test; ASCII text almost always does, binary text may not. Not measured with real raw-text frames.

* The size of a native PPM frame with a real native encoder, `V` and the CRC included.
* The relay path end to end (path rewrite, RXT append, no PPM decode), including locating the end of the position block without decoding it.
* A native encoder or decoder with this plan, and a complete RF chain with RXT (F4MLV's stated next step).

## Open

* **Mechanism for the destination byte.** Every first byte of a draft-0 address element is taken (`0 s nnn NNN`, `1 0 s xxxxx`, `1 1 lllll`), so a code byte `ff iiiiii` needs a way to be told apart from an address element: unused first-byte values (many `0 s nnn NNN` values are not valid `WIDEn-N`) or a header flag.
  Header bit 2 is free in PPM kinds, but kinds 0 and 1 would also want the table. Only F4MLV's encoder can settle this; the harness assumes 1 byte.
* **Which table version.** The direct codes change from version to version, so a receiver must know the version. The natural place is the same field as the PPM table set (`V`), which kinds 0 and 1 do not carry; either they carry it, or they do not use the table.
* **What the two reserved bits mean.** Not decided; candidates are "decodes hybrid B", "appends RXT", a device class. A capability bit here would replace part of the capability announcement of spec 02.
* **Kind values 2 and 3, or a new frame type.** F4MLV suggested the unused kinds himself; the issue also floated `01` text, `02` native, `03` capability, `80` to `FF` PPM. A draft-0 decoder that treats unknown kinds as errors will drop these frames, which must be checked in its code.
* **Frozen tables.** The experimental set (`V = 1`) is trained on one capture. The first format needs a frozen set trained on several networks, and a decision on who maintains it (append-only).
* **RXT at the end for kinds 0 and 1 too.** K6ATV's choice: one rule for every frame, and the argument is the same as for PPM frames (a relay appends without moving bytes, and RXT is never inside the content). It changes draft 0 for kinds 0 and 1 and so needs F4MLV's agreement and a matching change in his firmware; frames from his current builds put the block after the path and would not interoperate for RXT. Also **seven path elements instead of eight**, because of the marker bit.
* **Jon N7UV's view** on anything that changes what the tuple carries (the plan keeps the four measurements unchanged), and **F6BVP's** on identification: hybrid B keeps the source callsign recoverable in every frame, as F4MLV prefers.
* **The `V` byte** could share bits with another field.
* **Mic-E** is forwarded unchanged (decided: not worth converting). Open: whether draft 0 applies the destination-SSID path rule to a Mic-E destination.
* **The relay procedure** above is a draft: path rewriting in native elements (the index of the relay's element, moving mask bits when elements change), and time to hop at transmit time, need F4MLV's firmware to confirm.
