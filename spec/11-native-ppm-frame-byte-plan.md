# Proposal draft 11 - Hybrid B: native frame with a PPM-coded content field (byte plan)

| | |
|---|---|
| Status | **First draft of the byte plan F4MLV asked for in issue 7 of moricef/LoRa_APRS_Tracker (comments 14 to 17). Written by K6ATV, not yet seen or agreed by F4MLV. No native encoder implements it; the PPM payload's integrity rules and the RXT tail are tested (below).** |
| Builds on | moricef's native frame, draft 0 (`docs/LORA_APRS_NATIVE_FRAME_DRAFT0.md`, `moricef/LoRa_APRS_iGate`, branch `rxt`); hybrid B of `tools/hybrid/` and its tables (`tools/hybrid/tables/`); spec 10 (ending, CRC-16, re-encode check) |
| Replaces | the standalone whole-text frame `3C FF 81` of spec 10 as a frame format. Spec 10 stays as the integrity rules of the PPM payload. PPM of the whole packet is **not** the plan: a relay would have to decompress, edit the path and recompress |

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
| 3 | an RXT block follows the path | **PPM kinds (2, 3): an RXT block ends the frame.** Kinds 0 and 1: as draft 0 |
| 2 | the text is Huffman coded | PPM kinds: reserved, must be 0 |
| 1-0 | information kind: 0 raw, 1 position (2 and 3 unused) | **2 = information field as text, PPM coded. 3 = position block, then the comment PPM coded** |

F4MLV proposed using the two unused information-kind values to say which coder was used (comment 6), so this is his own placement. A receiver that does not know kind 2 or 3 MUST drop the frame and never show it as text.

### Content field, PPM kinds (2 and 3)

    V        1 byte    version of the table set (below); a receiver drops a version it does not hold
    L        1 byte    number of text bytes coded, 1..255
    CRC      2 bytes   CRC-16/X-25 (spec 10) of the information field as the iGate would upload it, low byte first
    payload  n bytes   range coder bytes of spec 01, ending as in spec 10 ("Ending")

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

### Destination (table v2)

The destination is coded from the versioned list `tools/hybrid/tables/tocall_table_v2.txt` (sha256 `e700581b...f59aff`, 113 lines: 85 exact destinations and 28 patterns): an exact entry costs 1 byte, a registry pattern with one wildcard (`APL...` and `APGRW?`) 2 bytes, anything else an escape byte followed by the draft-0 address element (5 bytes
for an ordinary destination, so 6 in all; this is also the cost of a Mic-E destination, which draft 0 carries as a 5-byte element). v1 and v2 share their codes: v2 only adds entries, and the list is append-only.
Exact-entry coverage on the French capture (Ariège, 8,062 packets) is 85.1% with v1 and 97.3% with v2; on the Arizona sample (2,659 packets) 99.8% and 100%. The rest escape (Mic-E 2.5% in Ariège).

**The byte coding of this field is not defined yet** (see Open): the harness only counts its cost.

### RXT block (header bit 3 set, PPM kinds)

    tuple 1 | tuple 2 | ... | tuple k | mask          the last byte of the frame is the mask

* A tuple is 4 bytes as in draft 0: RSSI, SNR, frequency offset, time to hop, each the RXT character minus 33, so each byte is 0 to 93.
* The mask has **bit 7 set** as a marker. Bit j (0 to 6) set means that path element j (0 is the first element after the destination) carries a measurement. Tuples are in ascending order of j. `k` is the number of mask bits 0 to 6 that are set.
* The block is `4k + 1` bytes. Bits at or above the path element count are not allowed.

The marker makes the block safe against truncation: a tuple byte is never 0x80 or more, so after a cut inside the block the last byte can never be mistaken for a mask (tested below). It costs one path element against draft 0 (7 instead of 8).
This placement (end of frame, mask byte last) is K6ATV's proposal in issue 7 (comment 5): a relay appends without moving the content. For kinds 0 and 1 the block stays after the path as in draft 0.

A relay builds its outgoing frame from the front: new header and path, then the position block and content field copied byte for byte, then the old tuples, its own tuple, and the new mask. If there was no block it creates one (tuple, mask) and sets header bit 3.
A relayed frame grows by 5 bytes for the first relay and 4 for each further one.

## Receiver (normative)

1. Drop the frame unless it starts `3C FF 02` and the information kind, the version `V` and the table versions are known.
2. Parse header, addresses and path, and the position block (kind 3), as draft 0.
3. If header bit 3 is set: the last byte is the mask. Drop the frame unless bit 7 is set, `k >= 1`, every set bit is below the path element count, and at least `4k + 1` bytes remain after the CRC. The RXT block is the last `4k + 1` bytes; the payload is everything between the CRC and the block.
   If bit 3 is clear, the payload is everything after the CRC.
4. Decode `L` text bytes from the payload with the range decoder (the table chosen by the kind), rebuild the information field, and drop the frame unless its CRC-16 matches.
5. Re-encode the `L` text bytes with the sender's rules (spec 10 "Ending"). Drop the frame unless the result is **exactly** the payload (same length, same bytes).
6. Only an accepted frame may be shown, uploaded, relayed, or kept for later use.

Steps 4 and 5 may be done in one pass over the model (spec 10 "Receiver"). Because the payload lies exactly between the CRC and the RXT block, there is no tail to guess about; a damaged or cut frame changes where the payload ends and step 5 fails.

## Size

Published figures for this combination, bytes saved against the plain frame including the 3-byte prefix, full frames only (N=1), no RXT, harness results and not RF measurements:

| Data | Combination | Saved |
|---|---|---|
| K6ATV APRS-IS capture, 93,145 held-out packets | hybrid B + tocall table + lossless draft-0 position | 46.2% |
| same | + packed position (optional, not in this plan) | 47.2% |
| F4MLV Ariège, 8,062 packets (comment 15) | table v2 + lossless draft-0 position, matched tables | 39.3% |
| F4MLV Arizona, 2,659 packets (comment 15) | same | 43.8% |

These do not include `V` and the CRC-16: the harness costs the PPM text with the native frame's own length byte, so the content field here is **3 bytes longer** than the harness assumes. With an average plain frame of 72 to 110 bytes that is roughly 3 to 4 points off each figure
(an estimate from the average frame lengths; not measured). Dropping `V` (folding the version into the header) would give one byte back. The 50.2% of spec 10 is the whole-packet figure and is not the figure for this plan.

## Integrity: what is measured

Spec 10's results apply to the PPM payload: cut frames, 0 wrongly accepted at every size; with the CRC-16 and the re-encode check together 0 of about 2.85 million damaged frames accepted. Those runs used the whole-packet table of spec 01 on whole texts; the hybrid B tables have not yet been run through them.

The RXT block is new. [`tools/integrity/rxt_tail_test.py`](../tools/integrity/rxt_tail_test.py) cuts 1 to 16 bytes off the end of 600 real packets carrying 0 to 3 tuples, and flips 1 and 3 bits, receiving with the rules above
(results: [`results-rxt-tail-marked.txt`](../tools/integrity/results-rxt-tail-marked.txt)):

* **With the marked mask byte: 0 of 21,564 cut frames were accepted**, at every cut size and tuple count.
* **Without the marker** (a plain mask in the last byte, [`results-rxt-tail-plain.txt`](../tools/integrity/results-rxt-tail-plain.txt)) 73 of the same 21,564 cut frames were accepted with a wrong mask (cuts of 4 or 8 bytes inside the block, 17 to 34 of 600 for the cut sizes concerned), because a left-over tuple byte can look like a valid mask. That is why the marker is in the plan.
* **Bit flips inside the tuples are not detected.** Nothing but the LoRa CRC covers the RXT bytes (a single flipped bit landed in a tuple and was accepted in 10% of frames with one tuple and 25% with three). This is the protection every field has today; extending the CRC-16 over the block would cost 2 bytes on relayed frames and every relay would recompute it. Not in this plan.

## Not yet measured

* The size of a native PPM frame with a real native encoder, `V` and the CRC included.
* Timing of the hybrid B tables on the ESP32 and T-Deck. Spec 10's 5 to 6 ms figures were for whole packets with the whole-packet table; hybrid B codes shorter text.
* The relay path end to end (path rewrite, RXT append, no PPM decode), including locating the end of the position block without decoding it.
* A native encoder or decoder with this plan, and a complete RF chain with RXT (F4MLV's stated next step).

## Open

* **Byte coding of the destination table.** Every first byte of a draft-0 address element is taken (`0 s nnn NNN`, `1 0 s xxxxx`, `1 1 lllll`), so a one-byte table index needs a mechanism that only F4MLV's encoder can settle: unused first-byte values (many `0 s nnn NNN` values are not valid `WIDEn-N`), or a header flag (header bit 2 is free in PPM kinds, but kinds 0 and 1 also want the table).
  The harness assumes 1 byte; the real cost depends on this.
* **Kind values 2 and 3, or a new frame type.** F4MLV suggested the unused kinds himself; the issue also floated `01` text, `02` native, `03` capability, `80` to `FF` PPM. A draft-0 decoder that treats unknown kinds as errors will drop these frames, which must be checked in its code.
* **Frozen tables.** The experimental set (`V = 1`) is trained on one capture. The first format needs a frozen set trained on several networks, and a decision on who maintains it (append-only).
* **Where the RXT block goes for kinds 0 and 1** (draft 0 puts it after the path) and **seven path elements instead of eight.**
* **Jon N7UV's view** on anything that changes what the tuple carries (the plan keeps the four measurements unchanged), and **F6BVP's** on identification: hybrid B keeps the source callsign recoverable in every frame, as F4MLV prefers.
* **The `V` byte** could share bits with another field.
* **Mic-E** goes as text; its destination escape costs 6 bytes in this plan (draft 0 has no Mic-E codec).
