# Proposal draft 11 - Hybrid B: native frame with a PPM-coded content field (byte plan)

> **DRAFT, WORK IN PROGRESS.** Not a standard, and not yet seen or agreed by F4MLV (moricef). No firmware implements it, and nothing has been tested on the air. Details will change; do not build deployed software from this text. Where it is weak or undecided is listed under "Open" and "Not yet measured".
>
> *Last changed 31 October 2026.*

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

The encoder uses a PPM kind only when the content field (`V`, `L`, CRC and payload: `4 + n` bytes) is shorter than the raw text it replaces. Otherwise it sends kind 0 or 1 exactly as draft 0 (raw or Huffman text). On our packets 15% of information fields do not shrink with the info-native table and 6% of comments with the comment-only table (about a third on the harness's data).
After a position block, an empty comment sends no text (kind 1 with nothing after the position). These rules are the harness's `PLAINTEXT=1`. Senders must follow the size rule, and a receiver drops a PPM frame the encoder would not have sent (Receiver, step 5), the same way it drops a payload that is not the canonical encoding; any receiver decodes all four kinds.

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

* A tuple is 4 bytes as in draft 0: RSSI, SNR, frequency offset, time to hop, each the RXT character minus 33, so each byte is 0 to 89 (his encoder clamps every field to 90 values; SNR has 89 steps, 0 to 88).
* The mask has **bit 7 set** as a marker. Bit j (0 to 6) set means that path element j (0 is the first element after the destination) carries a measurement. Tuples are in ascending order of j. `k` is the number of mask bits 0 to 6 that are set.
* The block is `4k + 1` bytes. Bits at or above the path element count are not allowed. **Each tuple byte must not exceed its field's maximum** (RSSI 89, SNR 88, frequency offset 89, time to hop 89, from his encoder's clamping; a receiver drops a frame with a tuple outside them; his draft-0 decoder does not check this): a free check on the tuples, which random damage fails about 98% of the time.

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

The result is the fewest bytes after which **any** following bytes decode to the same text (in all but rare binary texts: 1 in 23,709 adversarial binary texts ended one byte longer than needed, because the decoder clamps to the last symbol; correctness is unaffected, since every receiver compares against the same deterministic encoder). Spec 01's ending (the fewest bytes assuming zeros follow, then stripping zeros) is replaced for these frames; the new ending is on average 0.25 bytes longer.
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
4. Decode `L` text bytes from the payload with the range decoder (the table chosen by the kind), rebuild the information field, and drop the frame unless its CRC-16 matches. (The CRC is what protects `L`: the re-encode check alone passes a frame whose `L` is one too large 70% of the time.)
5. Re-encode the `L` text bytes with the sender's rules ("PPM payload rules", Ending). Drop the frame unless the result is **exactly** the payload (same length, same bytes), and drop it if the content field (`4 + n` bytes) is not shorter than `L`, because the encoder would have sent raw text.
6. Only an accepted frame may be shown, uploaded, relayed, or kept for later use.

Steps 4 and 5 may be done in one pass over the model ("PPM payload rules", Receiver equivalence). Because the payload lies exactly between the CRC and the RXT block, there is no tail to guess about; a damaged or cut frame changes where the payload ends and step 5 fails.

## Relay (draft)

A relay accepts a frame by the Receiver rules, which for PPM kinds decode the content once to check it (one pass; 1.4 to 4.2 ms on the boards measured), and then forwards it with its own path update and its own RXT measurement **without re-encoding the content**: the outgoing frame is built from the bytes it received. A relay that skips the check forwards damage it cannot see. The content is never converted: a Mic-E packet is forwarded as it was received (see Mic-E below).

1. **Rewrite the front.** Decode the header, addresses and path of the native part to the TNC2 path, apply the usual digipeater rule (for example `WIDE1-1` becomes the relay's callsign with the used mark; `WIDEn-N` loses one hop), and encode the header, addresses and path again. This is cheap and needs no PPM decoding. The destination is copied as received
   (a Mic-E destination is its 6 latitude characters, so it is the escape followed by the 5-byte element). *Draft 0 leaves a native relay state undefined and works on the TNC2 path; this step follows that. His iGate relay today decodes the whole native frame to TNC2, moves the measurements to their callsigns, adds its own tuple and encodes the whole frame again; for the PPM kinds the content field is copied instead of decoded and re-encoded, which his code does not do yet.*
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

**Target.** On one hop the undetected-error rate should be no worse than VHF APRS, whose AX.25 frame check is a CRC-16: about 1 in 65,536 for random damage. (VHF APRS is also protected per hop only: a digipeater recomputes the check.) The LoRa payload CRC is 16 bits (CCITT), covers every payload byte, header byte, address and tuple included, and gives that level for random channel damage on one hop. It is optional,
so a sender must enable it and a receiver must act on its status. It cannot see three things: a **payload cut before transmission** (the CRC is computed after the cut), a **decoder using the wrong table** (the frame arrives intact), and a **fault in a relay** after reception (the relay then sends with a fresh LoRa CRC).

**What protects each part of the frame:**

| Part | Channel noise on a hop | Cut before transmission | Wrong table, or a relay fault |
|---|---|---|---|
| `3C FF 02`, header byte, source, destination code, path, `V` | LoRa CRC only | not applicable (the front is not cut) | not covered. A flipped header kind bit (2 to 0, 3 to 1) turns a PPM frame into raw text with no check |
| PPM payload | LoRa CRC, then the re-encode check and the content CRC | the re-encode check rejects every cut | the re-encode check passes about 0.4% of wrong-table decodes (up to 2.1% for some table pairs), the content CRC alone all but 2 of 360,000, both together 0 of 360,000 |
| Information field (position block and comment) | LoRa CRC and the content CRC, end to end across relays | the content CRC | the content CRC |
| RXT tuples and mask | LoRa CRC only, plus the marker and range checks | the marked mask byte | not covered (see below) |
| Raw text (kinds 0 and 1, and raw fallback) | LoRa CRC only | nothing | nothing |

So the plan meets the VHF level per hop for channel noise on every byte, and is stronger than VHF for the information field across relays. It is **not** end-to-end protection for the header, addresses, path, `V`, RXT tuples or raw text.

**How the combined rates are meant to be read.** The end-to-end figure for damage to the PPM payload is (the probability that damage passes the LoRa CRC, about 1 in 65,536) times (the share of those frames our checks accept, measured below). That product assumes the damage is random-looking, that the two checks are independent, and that the rate is per damaged frame, not per frame sent. Errors of 1 to 3 bits cannot pass a 16-bit LoRa CRC at all, so the rows for few-bit flips describe an event that does not occur on the air; they test the checks, not the channel. Cuts and wrong tables have no LoRa CRC factor.

* **Truncation of the PPM payload:** the re-encode check. Alone, with no CRC, 0 of about 230,000 cut frames per cut size were accepted (286,100 texts, [`results-hybridB-reencode-alone.txt`](../tools/integrity/results-hybridB-reencode-alone.txt)). A review that tried every cut point on about 300,000 adversarial texts found that a payload that is a proper prefix or extension of the canonical encoding can never be accepted, so cuts, appended bytes and joined frames are rejected by construction.
* **Wrong table, wrong model, a second implementation that differs:** the re-encode check alone catches about 99.6% (0.4% accepted over 12 ordered pairs of four tables; one changed count in a table, 1 in 155), the CRC-16 alone all but 2 of 360,000, and both together all of them (95% upper bound about 1 in 120,000). The **content CRC-16** is therefore kept: computed by the origin over the information field, never touched by relays. (An earlier draft said the re-encode check cannot see a wrong table; that figure, 46%, was for primed wrong references with the old ending.)
* **Random damage to the payload after the LoRa CRC has passed:** not zero. A review with 180 million random payload damages found 1 frame accepted with wrong text (5.6e-9), and 6 of 90.3 million trials that replaced one of the last three payload bytes (6.6e-8); the wrong text differs in the last 4 to 9 characters. The re-encode check alone passes 1 in 327 of the random damage and the CRC-16 alone 1 in 64,000. Our own earlier run, 0 of about 2.85 million, bounds the combined rate only to about 1 in 950,000.
* **`L`** is protected by the CRC only (see Receiver, step 4).
* **The RXT block is unauthenticated.** Only the LoRa CRC protects it, plus two cheap checks: the marked mask byte makes every cut inside the block fail, and the range check fails random bytes about 98% of the time, but a single-bit flip of a tuple byte stays in range 76% of the time. In a test block, 61% of single-bit flips were accepted with changed values, and a two-bit mask change that keeps the number of tuples (`0x83` to `0x85`) was accepted every time and silently moves the tuples to different path elements.
  A per-tuple check written by the relay that owns the tuple would add protection; a block CRC recomputed by every relay would not.

Spec 11 spends 2 bytes on the content CRC-16 and none on the RXT block. **Changes that would close gaps found in review, not adopted yet:** have the content CRC also cover the header byte, source and destination (no extra bytes); make `V` mandatory in every new-family frame so the destination list has a version (see Open).

**The RXT block** ([`tools/integrity/rxt_tail_test.py`](../tools/integrity/rxt_tail_test.py)), tested on 600 real packets carrying 0 to 3 tuples, with the rules above:

* **Cut frames, 1 to 16 bytes off the end: 0 of 21,564 accepted** with the marked mask byte ([`results-rxt-tail-marked.txt`](../tools/integrity/results-rxt-tail-marked.txt)).
  Without the marker (a plain mask in the last byte, [`results-rxt-tail-plain.txt`](../tools/integrity/results-rxt-tail-plain.txt)) 73 of the same cut frames were accepted with a wrong mask (cuts of 4 or 8 bytes inside the block, 17 to 34 of 600 for the cut sizes concerned), because a left-over tuple byte can look like a valid mask. That is why the marker is in the plan.
* **Damage inside the block** (1 or 3 flipped bits, 36,000 trials): the range check lowers the share accepted from 87% and 68% to 66% and 32% (one and three tuples; [`results-rxt-tail-marked-range.txt`](../tools/integrity/results-rxt-tail-marked-range.txt)). Flips of a few bits are already caught by the LoRa CRC; the range check matters for random-looking damage. A CRC-16 over the block (tested: 0 accepted, and every 1- to 3-bit flip detected) was **not adopted**: it would be recomputed by every relay and
  adds nothing the LoRa CRC does not give per hop ([`results-rxt-crc.txt`](../tools/integrity/results-rxt-crc.txt)).

## Receiver time (ESP32, measured)

One-pass receiver (decode, CRC-16, re-encode compare) with the hybrid B tables, fast `build_freq`, mean per frame over the texts PPM can shrink ([`results-esp32-timing.txt`](../tools/integrity/results-esp32-timing.txt)):

| Board | Information field (kind 2) | Comment after a position (kind 3) |
|---|---|---|
| LILYGO LoRa32 v2.1 (ESP32-PICO-D4) | 4.2 ms (3.5 ms with the key tables copied to RAM) | 2.0 ms (1.8 ms) |
| LILYGO T-Deck (ESP32-S3) | 2.8 ms (2.7 ms) | 1.4 ms (1.4 ms) |

The earlier whole-packet figures (6.3 ms and 5.0 ms) are higher because the text is longer. The re-encode check adds nothing measurable in the one-pass receiver; a two-pass receiver roughly doubles the time. These are codec-only test sketches on 48 fabricated packets (28 information fields and 18 comments shrink), without WiFi, not the iGate or tracker firmware.

## Not yet measured

* **Kind 3 end to end.** No executable test covers a kind-3 frame (the CRC over the rebuilt position plus comment, the comment-only table) or the destination and path parse; the test code uses a 6-byte test frame with no `V` byte.
* **The RXT block with raw text (kinds 0 and 1).** The tail tests used PPM payloads. Raw or Huffman text has no check of its own (draft 0 relies on the LoRa CRC), so a cut that removes the whole block and part of the text is caught only if the new last byte fails the mask test; ASCII text almost always does, binary text may not. Not measured with real raw-text frames.

* The size of a native PPM frame with a real native encoder, `V` and the CRC included.
* The relay path end to end (path rewrite, RXT append, no PPM decode), including locating the end of the position block without decoding it.
* A native encoder or decoder with this plan, and a complete RF chain with RXT (F4MLV's stated next step).

## Extending the format later

The information kind (header bits 1 and 0) is full once kinds 2 and 3 are taken, so a future need does not have to squeeze into this header. **A new header is a new frame type byte** (`3C FF xx`), with its own layout, defined when it is warranted. Receivers drop any `3C FF` type they do not know and never show it as text, so a new type is invisible to old firmware.
Types in use or suggested: `01` plain text, `02` native (this plan and draft 0), `03` capability frame (suggested in issue 7), `80` to `FF` the whole-packet PPM frames of spec 01 and spec 08. Several different headers can therefore live side by side, each used where it fits.
Inside this header, the content field's version byte `V` is allocated append-only and can name further table sets; relays copy the content field without reading it, so adding a table set changes nothing for them.

## Roles and profiles (to be defined)

Interoperability and a staged upgrade need a standard profile for each kind of station, so that firmware by different authors agrees on who must understand what. Three roles, with what each needs, as a starting point:

| Role | Originates | Must decode | Relays |
|---|---|---|---|
| **Tracker** | its own packets, as plain text, and optionally native or hybrid B (no RXT, path as configured) | plain text, native, hybrid B (messages and replies reach it) | no, in the code read |
| **Digipeater** | normally nothing of its own except beacons and telemetry | every format it relays | yes: path rewrite, its own RXT tuple, content copied (Relay, above) |
| **iGate** (RF to APRS-IS) | beacons, telemetry, and packets from APRS-IS to RF in the format its settings choose | every format, decoded to TNC2 with the RXT block removed before upload | optional: digipeating is a setting of an iGate, so **every iGate needs the encoder as well as the decoder** |

Rules that apply to every role: a plain text frame is always accepted and always sendable (the floor); an unknown `3C FF` type, kind or table version is dropped and never shown as text; the table-set version `V` decides which tables a frame needs.
A **digipeater is a mode of an iGate (or of a tracker), not a separate profile**: the frame is the same and only the relay rules above apply, so nothing is needed for it beyond the iGate's decoder and encoder (a "relays hybrid B" capability bit may still be wanted).
Open: who defines and maintains the profiles; how a role advertises what it supports (the two reserved destination bits, or a capability frame); and how many profile levels there are (for example level 0 plain text, level 1 native draft 0, level 2 hybrid B).

## Firmware integration notes (from reading his tracker code)

His tracker (`moricef/LoRa_APRS_Tracker`, branch `devel`, `src/lora_utils.cpp` and `lib/native_aprs/`; the native codec there is a copy of his iGate's, with the relay functions left out) does this today:

* **Receive:** the radio is read as raw bytes and only when the radio library reports no CRC error. A native frame (`3C FF 02`) is decoded to TNC2, its RXT block turned into a v2 text trailer, and passed on as a `3C FF 01` text frame, so everything after that sees ordinary text. Any other frame is read as text up to the first zero byte. **It does not drop an unknown `3C FF xx` type in that file**, which this plan requires (a `3C FF 81` frame would be shown as garbage); the check belongs with the new type.
* **Transmit:** `txFormat` is 0 text, 1 native, 2 text and then native (two frames per packet, a transition mode). A packet that cannot be encoded natively goes as text; native frames are capped at 255 bytes. The tracker adds no RXT of its own.
* **Where hybrid B plugs in:** the receive step (a new branch for kinds 2 and 3, producing the same TNC2 text) and the transmit step (a new `txFormat` value, falling back to native or text when PPM does not shrink the content). The setting is stored in the tracker's JSON configuration and, I did not check, in its web page.
* **Not read:** its digipeater code, if any, and its message handling, which should not depend on the frame format because everything after receive sees text.

### K6ATV's iGate fork (`LoRa_APRS_iGate`, branch `feature/order3-compression`, ESP32 LoRa32 v2.1; no RXT, no native frames)

* **Receive** (`readFrame` in `src/lora_utils.cpp`): a compressed (`3C FF 81`) or primed (`3C FF C1`) frame is decoded, only when the radio library reports no CRC error, and handed on as a `3C FF 01` text frame, so everything after receive sees text. An unknown `3C FF xx` type is dropped and never shown as text (the check his tracker `devel` lacks).
* **Digipeater** (`src/digi_utils.cpp`): it works entirely on the TNC2 **text** path: `buildPacket` rewrites it with string operations (`WIDE1-1` as the first hop becomes its callsign plus the used mark; `WIDE2-n` handling in mode 2; mode 3 repeats when its callsign is in the path; a cross-frequency mode; `NOGATE` is not repeated). It then puts the text packet in the output buffer, and the transmit step re-encodes the **whole** packet (compressed if `compressTx`).
  So today a relay decodes the whole packet and encodes it again. For hybrid B the same path rules would have to run on the path decoded from the native front, with the content copied, not decoded and re-encoded.
* **Duplicates:** `isIn25SegHashBuffer(sender, payload)` drops a repeat of the same sender and payload within 25 seconds. With hybrid B a relay could use the source from the native front and the content CRC-16 as the key, without decoding the content (for PPM kinds it has decoded it once anyway, to verify).
* **Settings:** `compressTx` (on or off) and `compressFullEvery`; no `txFormat`. A `txFormat` like his tracker's (text, native, hybrid B, or both) would replace the boolean.
* **Where hybrid B plugs in:** `readFrame` (native `3C FF 02` decode, kinds 2 and 3), the digipeater (path rewrite on the native front), `transmitText` (a new format), and the configuration. The iGate-to-APRS-IS side needs no change, since it uploads the decoded TNC2 text.

### K6ATV's Direwolf port (`~/direwolf`, branch `feature/lora-cad-live`; the hill Raspberry Pi test bench, not a deployment target)

* **Receive** (`rx` thread in `src/loraspi.c`): `lorac_rx` (`src/loracompress.c`) expands a `3C FF 81` or `3C FF C1` frame to its TNC2 text, or reports a compressed frame it could not decode, which is dropped. The text is then parsed into an AX.25 packet (`ax25_from_text`) and handed to Direwolf's core (`dlq_rec_frame`), where the digipeater, the APRS decoder and the IGate work on that AX.25 object. The settings are `LORACOMPRESSRX` (on by default) and `LORACOMPRESS n` (a full frame at least every n-th packet; 0 or 1 means no primed frames).
* **Transmit:** the core hands the driver an AX.25 packet, which becomes TNC2 text, and `lorac_tx` picks the smallest of primed, full compressed or plain.
* **A relay therefore decodes and re-encodes the whole frame.** For PPM content this is harmless: the encoder is deterministic, so the same text with the same table and the same raw-or-PPM decision gives **byte-identical content**, and a relay that re-encodes produces what a relay that copies would. It costs only CPU. What does not survive the trip through Direwolf's core is anything that is not in the AX.25 object: the RXT block (it would need a side channel from receive to transmit, as the iGate's `RxtState` is), and addresses outside AX.25's limits (SSIDs above 15 are rejected at parse, which the native address element allows).
* **K6ATV-10's configuration** (`/home/walter/direwolf.conf` on the hill Pi, read from a filtered paste, login line removed): one service runs the LoRa channel and a VHF channel (RTL-SDR on 144.390 MHz). LoRa is channel 10 on a 433 MHz SPI hat, 433.775 MHz, SF12, BW 125 kHz, CR 4/5, sync word 0x12, 22 dBm, boosted receive gain, with the events log. It is an **iGate and a beacon, not a digipeater**: every `DIGIPEAT` line is commented out, `IGSERVER` is `noam.aprs2.net`, `IGTXVIA` is commented out (nothing from APRS-IS goes to RF), and two `PBEACON` lines send its own beacon over LoRa and to APRS-IS every 30 minutes.
  There is no `LORACOMPRESS` line, so it sends plain frames; `LORACOMPRESSRX` is on by default. As configured today this node needs only the **decoder**, but digipeating is a setting in Direwolf as in the ESP32 iGate (the `DIGIPEAT` lines are only commented out), and it also sends its own beacons, so a complete iGate profile needs the **encoder** as well; it has no relay path to change until digipeating is switched on.
* **Where hybrid B plugs in:** the `rx` step (a native decoder in place of `lorac_rx`, producing the same TNC2 text) and the transmit step (a native encoder). RXT and the destination table need work in the core's packet path; the table could live in the driver, since the destination is a field of the TNC2 text.

## Open

* **The destination byte needs no disambiguation inside kinds 2 and 3.** Draft-0 decoders reject any information kind above 1, so a frame of kind 2 or 3 never reaches an address-element parser built for draft 0; this plan defines the destination field of those kinds as the code byte (escape plus a draft-0 element), and there is nothing to tell apart. The consequence is that **kinds 0 and 1 cannot use the table** without breaking draft-0 decoders (their address parser accepts every first byte below 0x80 as a `WIDEn-N` element). A raw-text fallback frame therefore needs to stay in the new family: **proposal:** for kinds 2 and 3, header bit 2 (free in this plan) says the content is raw text (1) or PPM (0), so fallback frames still use the table. Needs F4MLV's agreement.
* **Which table version (a gap found in review).** The direct codes change from version to version and the frame has no field for the destination list version, so a receiver with a different list silently misreads the destination, and an iGate would upload it. Proposed: make `V` mandatory in every new-family frame, as `cc vvvvvv` (`cc` the content coding: 0 PPM, 1 raw text, 2 and 3 future; `vvvvvv` the table-set version, which names the PPM tables and the destination list together). It costs 0 bytes on PPM frames and 1 byte on raw-text frames (about 0.1 byte per packet on average) and frees header bit 2. The alternative is an append-only list that never re-ranks, with no version field. Not decided.
* **Close the header gap.** A flipped header kind bit (2 to 0, 3 to 1) turns a PPM frame into raw text with no check. Proposed: the content CRC-16 input also covers the header byte, source and destination (no extra bytes), so a frame whose header was damaged fails the CRC. Not adopted yet.
* **Distribute destination-list updates over the air.** The list is small (62 destinations are about 310 bytes, two frames, about 18 s at SF12), so a station that sees an unknown list version could ask its nearest neighbour, or an iGate could fetch and verify it and pass it on. The PPM tables (about 150 KB, 600 full frames) cannot go this way. Needs a version field, a signature from a key built into the firmware, rate-limited requests, and old versions kept. Not designed.
* **Upgrade path.** Receivers first and senders last, plain text as the fallback, the two reserved destination bits as a capability flag, unknown types dropped and never shown as text, old formats decoded indefinitely. Not written up.
* **What the two reserved bits mean.** Not decided; candidates are "decodes hybrid B", "appends RXT", a device class. A capability bit here would replace part of the capability announcement of spec 02.
* **Kind values 2 and 3, or a new frame type.** F4MLV suggested the unused kinds himself; the issue also floated `01` text, `02` native, `03` capability, `80` to `FF` PPM. A draft-0 decoder that treats unknown kinds as errors will drop these frames, which must be checked in its code.
* **Frozen tables.** The experimental set (`V = 1`) is trained on one capture. The first format needs a frozen set trained on several networks, and a decision on who maintains it (append-only).
* **RXT at the end for kinds 0 and 1 too.** K6ATV's choice: one rule for every frame, and the argument is the same as for PPM frames (a relay appends without moving bytes, and RXT is never inside the content). It changes draft 0 for kinds 0 and 1 and so needs F4MLV's agreement and a matching change in his firmware; frames from his current builds put the block after the path and would not interoperate for RXT. Also **seven path elements instead of eight**, because of the marker bit.
* **Jon N7UV's view** on anything that changes what the tuple carries (the plan keeps the four measurements unchanged), and **F6BVP's** on identification: hybrid B keeps the source callsign recoverable in every frame, as F4MLV prefers.
* **The `V` byte** could share bits with another field.
* **Mic-E** is forwarded unchanged (decided: not worth converting). Open: whether draft 0 applies the destination-SSID path rule to a Mic-E destination.
* **The relay procedure** above is a draft: path rewriting in native elements (the index of the relay's element, moving mask bits when elements change), and time to hop at transmit time, need F4MLV's firmware to confirm.
