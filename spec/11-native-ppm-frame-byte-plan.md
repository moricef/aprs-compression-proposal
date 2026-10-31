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

`3C FF 02`, the header byte, the address elements, the path and the position block are those of draft 0 (this plan defines them exactly in Conformance); kinds 0 and 1 of draft 0 are not used. This plan adds: PPM-coded content (two header values), the destination table, a rule for when to fall back to raw text, and the place of the RXT block.

### Header byte (draft 0 layout, additions in bold)

| Bits | Draft 0 | This plan |
|---|---|---|
| 7-4 | number of path elements, 0 to 15 | same |
| 3 | an RXT block follows the path | **An RXT block ends the frame** (it follows the content field), which moves it from draft 0's place after the path |
| 2 | the text is Huffman coded | kinds 2 and 3: reserved, must be 0 (the content coding is in `V`) |
| 1-0 | information kind: 0 raw, 1 position (2 and 3 unused); this plan does not use 0 and 1, and a receiver drops them | **2 = information field as text, PPM coded. 3 = position block, then the comment PPM coded** |

F4MLV proposed using the two unused information-kind values to say which coder was used (comment 6), so this is his own placement. A receiver that does not know kind 2 or 3 MUST drop the frame and never show it as text.

### Content field (kinds 2 and 3)

The first byte of the content field is `V`, present in **every** frame of kinds 2 and 3:

    V = cc vvvvvv
    cc      2 bits   content coding: 0 (binary 00) = PPM, 3 (binary 11) = raw text; 1 and 2 are reserved and a receiver drops the frame.
                     The two codings differ in both bits on purpose: a single flipped bit then lands on a reserved value and the frame is dropped, instead of turning PPM content into raw text, which has no check.
    vvvvvv  6 bits   version of the table set: it names the PPM tables AND the destination list (64 sets); a receiver that does not hold it drops the frame

PPM content (`cc = 0`):

    V        1 byte
    L        1 byte    number of text bytes coded, 1..255
    CRC      2 bytes   CRC-16/X-25 (below), low byte first
    payload  n bytes   range coder bytes of spec 01, ending as in "PPM payload rules" below

Raw content (`cc = 3`): `V` followed by the text itself, up to the RXT block or the end of the frame (no `L`, no CRC). It is used when PPM would not shrink the text.

* Kind 2 carries the whole information field. Kind 3 follows a position block and carries only the comment that remains.
* **What the CRC covers** (PPM content only). The CRC input is, in this order: the byte `header & 0x07` (the kind bits and the reserved bit); the byte `V`; the source as TNC2 text; the byte `>` (0x3E); the destination as TNC2 text (decoded through the destination list); the byte `:` (0x3A); and the information field (the text after the first `:` of the TNC2 packet), rebuilt from the position block and the decoded text.
  It does not cover the path or the rest of the header (the path count and the RXT bit), which relays change. A relay that only rewrites the path and appends a tuple therefore never recomputes it, and a receiver holding the wrong destination list, or receiving a damaged source, destination or `V`, fails the CRC.
* The payload length is not sent; the receiver derives it (Receiver, step 3).

**Tables.** Kind 2 uses the *info-native* table and kind 3 the *comment-only* table, because a comment after a position looks different from a whole information field. The low six bits of `V` name the pair and the destination list. Version numbers are allocated by this spec, append-only:

| `vvvvvv` | Info-native table | Comment-only table | Status |
|---|---|---|---|
| 1 | `tools/hybrid/tables/model_hybridB_info_native.c` (sha256 `440a1ec2...dc270`, 77 KB) | `tools/hybrid/tables/model_hybridB_comment.c` (sha256 `d1262b44...eaf5`, 75 KB) | experimental: trained on 70% of the stations of one APRS-IS capture (callsign-hash buckets 3 to 9), 174,170 and 105,092 packets |

The tables' own header says "model version 2", which they share with the whole-packet and hybrid A tables of the same build; that number does not identify a layout, which is why the version in `V` is allocated here. A deployed first format needs a frozen set trained on all stations and on several networks' data
(`train_layout_model.py --base-counts`, see the harness README), with a new `vvvvvv`. The experimental set above must not be treated as the final one.

### Fallback to raw text

The encoder uses PPM content only when it is shorter than raw content: `4 + n < 1 + L` bytes (PPM: `V`, `L`, CRC and payload; raw: `V` and the text). Otherwise it sends raw content (`cc = 3`) in the same kind, so the frame still uses the destination list and the RXT rules.
**Decided 1 November 2026 (Walter): draft 0 is not used; only this format. A receiver of this plan drops a frame of kind 0 or 1** (the reference code in C and Python does, and the cross-check agrees on it). Frames of kind 0 or 1 are not part of this plan; a raw-text fallback stays in kinds 2 and 3 as raw content (`cc = 3`). On our packets 15% of information fields do not shrink with the info-native table and 6% of comments with the comment-only table (about a third on the harness's data).
After a position block, an empty comment sends no text (kind 3, raw content with nothing after `V`). These rules are the harness's `PLAINTEXT=1`. Senders must follow the size rule, and a receiver drops PPM content the encoder would not have sent (Receiver, step 5), the same way it drops a payload that is not the canonical encoding; any receiver decodes every coding it holds.

### Destination (versioned table, two bits reserved)

The destination is coded in one byte, `ff iiiiii`:

* `ff`, the top **two bits, are reserved**. A sender sets them to 0 and a receiver ignores them, so a later version can give them a meaning (for example "this station decodes hybrid B" or "this station appends RXT") without changing the table or the frame.
  Whatever they come to mean describes the originating station, not a relay in the path.
* `iiiiii`, the low six bits, give 64 codes. **Codes 0 to 61 name the 62 most common destinations of that table version, in descending order of popularity** (code 0 is the most common), so the common ones are found first by a simple search. Code 62 is reserved (a receiver drops a frame that uses it); decided: it is kept reserved and not used as a 63rd destination. **Code 63 is the escape:** after it comes the draft-0 address element (5 bytes for an ordinary destination, so 6 in all; this is also the cost of a Mic-E destination, which draft 0 carries as a 5-byte element).
* A destination that has no direct code is not in the table: it escapes. There are no patterns and no second tier of 2-byte codes. In both measured captures registry patterns almost never fired (0 packets of 8,062 in Ariège and 2 of 14,305 in ours).
* **Each table version fills the 62 destination codes, in order of popularity, with the destinations that are most common then,** and need not keep the codes of the version before; the version names the list. A destination used only once is never given a code.

**Set 1** (`vvvvvv = 1`) is the file [`tools/hybrid/tables/tocall_set1.txt`](../tools/hybrid/tables/tocall_set1.txt) (sha256 `0e076bfbe365edc040cc294b7b271495d6f2e3d76292be9cd3dae5c4e6bf7a35`): 62 destinations ranked on our packets plus the destinations reported from the French capture; `APU25N`, `BEACON` and other rare ones are not in it. It is experimental, like the tables.

Measured on our 14,305 real packets (73 distinct destinations): the 62 most common as direct codes cover 99.9% of packets (the first five, `APLRG1`, `APLRT1`, `APLWS2`, `APLG01`, `APLRFD`, cover 88.8%), a mean of 1.0038 bytes per packet, against 1.015 for the v2 list of the harness (85 exact entries, 28 patterns). That ranking is made on the same data it is tested on, so it is optimistic; a real list is ranked on training stations and checked on held-out ones.
The harness's v2 list ([`tools/hybrid/tables/tocall_table_v2.txt`](../tools/hybrid/tables/tocall_table_v2.txt), sha256 `e700581b...f59aff`, 85 exact destinations and 28 patterns) is the starting data, not the final list: its entries seen once or twice in the captures (`APU25N`, `BEACON`) would not earn a direct code, and the few `APL...` destinations seen here that it lacks (`APSTKI` 20 packets, `APLM2D` 6, `APLGMT` 5, `APLHT2` 4) would compete for the 62 codes.
Exact-entry coverage of the v2 list on the French capture (Ariège, 8,062 packets) is 85.1% with v1 and 97.3% with v2; on the Arizona sample (2,659 packets) 99.8% and 100%; Mic-E is 2.5% in Ariège and always escapes.

**The table version is the low six bits of `V`**, present in every frame of kinds 2 and 3, so a receiver always knows which list a code refers to and drops a frame whose version it does not hold. The direct codes mean different destinations in different versions. Kinds 0 and 1 do not use the list.

### RXT block (header bit 3 set)

    tuple 1 | tuple 2 | ... | tuple k | mask          the last byte of the frame is the mask

* A tuple is 4 bytes as in draft 0: RSSI, SNR, frequency offset, time to hop, each the RXT character minus 33, so each byte is 0 to 89 (his encoder clamps every field to 90 values; SNR has 89 steps, 0 to 88).
* The mask has **bit 7 set** as a marker. Bit j (0 to 6) set means that path element j (0 is the first element after the destination) carries a measurement. Tuples are in ascending order of j. `k` is the number of mask bits 0 to 6 that are set.
* The block is `4k + 1` bytes. Bits at or above the path element count are not allowed. **Each tuple byte must not exceed its field's maximum** (RSSI 89, SNR 88, frequency offset 89, time to hop 89, from his encoder's clamping; a receiver drops a frame with a tuple outside them; his draft-0 decoder does not check this): a free check on the tuples, which random damage fails about 98% of the time.

The tuples have no check of their own beyond the LoRa CRC, which already covers the whole frame at the level of VHF APRS (about 1 in 65,536); a block CRC would be recomputed by every relay and so add nothing the LoRa CRC does not give per hop.
The marker makes the block safe against truncation: a tuple byte is never 0x80 or more, so after a cut inside the block the last byte can never be mistaken for a mask (tested below). It costs one path element against draft 0 (7 instead of 8).
This placement (end of frame, mask byte last) is K6ATV's proposal in issue 7 (comment 5): a relay appends without moving the content, whatever the content is, and the content (PPM payload or text) is never touched by RXT, which is already compact binary and would not shrink. The same placement applies to both kinds, so a relay needs one rule.

A relay builds its outgoing frame from the front: new header and path, then the position block and content (the content field) copied byte for byte, then the old tuples, its own tuple and the new mask. If there was no block it creates one (tuple, mask) and sets header bit 3.
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

1. Drop the frame unless it starts `3C FF 02` and the information kind, the content coding `cc` (0 or 3) and the table-set version in `V` are known.
2. Parse header, addresses and path, and the position block (kind 3), as draft 0.
3. If header bit 3 is set: the last byte is the mask. Drop the frame unless bit 7 is set, `k >= 1`, every set bit is below the path element count, and at least `4k + 1` bytes remain after the position block and, for PPM kinds, after the content CRC. The RXT block is the last `4k + 1` bytes, and **the frame is dropped if a tuple byte exceeds its field maximum**; the payload (PPM content), or the raw text (`cc = 3`) is everything between the end of the position block or CRC and the block.
   If bit 3 is clear, there is no block and the payload or text is everything after.
4. Decode `L` text bytes from the payload with the range decoder (the table chosen by the kind), rebuild the information field, and drop the frame unless its CRC-16 matches, computed over the input listed under Content field. Raw content (`cc = 3`) has no CRC and nothing more to check. (The CRC is what protects `L`: the re-encode check alone passes a frame whose `L` is one too large 70% of the time.)
5. Re-encode the `L` text bytes with the sender's rules ("PPM payload rules", Ending). Drop the frame unless the result is **exactly** the payload (same length, same bytes), and drop it if the content field (`4 + n` bytes) is not shorter than `1 + L`, because the encoder would have sent raw content.
6. Only an accepted frame may be shown, uploaded, relayed, or kept for later use.

Steps 4 and 5 may be done in one pass over the model ("PPM payload rules", Receiver equivalence). Because the payload lies exactly between the CRC and the RXT block, there is no tail to guess about; a damaged or cut frame changes where the payload ends and step 5 fails.

## Relay (draft)

A relay accepts a frame by the Receiver rules, which for PPM kinds decode the content once to check it (one pass; 1.4 to 4.2 ms on the boards measured), and then forwards it with its own path update and its own RXT measurement **without re-encoding the content**: the outgoing frame is built from the bytes it received. A relay that skips the check forwards damage it cannot see. The content is never converted: a Mic-E packet is forwarded as it was received (see Mic-E below).

1. **Rewrite the front.** Decode the header, addresses and path of the native part to the TNC2 path, apply the usual digipeater rule (for example `WIDE1-1` becomes the relay's callsign with the used mark; `WIDEn-N` loses one hop), and encode the header, addresses and path again. This is cheap and needs no PPM decoding. The destination is copied as received
   (a Mic-E destination is its 6 latitude characters, so it is the escape followed by the 5-byte element). *Draft 0 leaves a native relay state undefined and works on the TNC2 path; this step follows that. His iGate relay today decodes the whole native frame to TNC2, moves the measurements to their callsigns, adds its own tuple and encodes the whole frame again; for the PPM kinds the content field is copied instead of decoded and re-encoded, which his code does not do yet.*
2. **Copy the rest unchanged**, byte for byte: the position block and the content (a content field with its CRC-16 and payload, or raw text).
3. **Find the index of the relay's own path element** after step 1, set that bit in the mask, and insert the relay's tuple in ascending order of index. Tuples already in the block keep their meaning only if their path elements keep their index; a relay that changes the number or order of elements before them must move the mask bits with them (draft 0 moves measurements by full callsign).
4. **Fill the tuple at transmit time.** RSSI, SNR and frequency offset are those measured when the frame was received. Time to hop runs from the end of reception to the end of the relay's own transmission, including queue and backoff, so it is computed just before transmission starts from the queue time plus the time on air, which the frame length fixes.
5. **A relay that cannot finish the tuple in time sends none** (a frame without a block is valid) instead of sending a wrong one.
6. **Drop duplicates** on the decoded APRS content (source, destination and information field), as draft 0 does; the path and RXT differ between copies and are ignored.
7. **An iGate** decodes the frame to TNC2 for APRS-IS, MQTT and TNC output, removing the RXT block, and uploads the same source, destination and information field a text frame would have given.

### Mic-E

Mic-E is not decoded or converted by this plan. A Mic-E packet is carried as received: the destination as the escape plus the 5-byte element (6 bytes, since its characters are the position), and the information field as raw content (kind 2, `cc = 3`), or as a PPM content field (kind 2) only if that is shorter, which is unusual because the field is printable binary. Relays forward it exactly as above. Converting Mic-E to a native position
would save only 1 to 3 bytes per Mic-E packet, almost all of it in the destination, and would change the bytes of the packet; it is not worth that and is not part of this plan.
If the sender used the destination SSID as a path code, the relay applies the usual rule to it; whether draft 0 does so is not checked.

## Conformance (draft)

An implementation conforms if, for every line of [`test/vectors_hb_v1.txt`](../test/vectors_hb_v1.txt) (`<frame hex> <TNC2 packet hex>`, 500 fabricated packets, table set 1 and destination list set 1), the encoder produces exactly that frame from the packet and the receiver accepts the frame and returns exactly that packet;
and, for [`test/vectors_hb_relay_v1.txt`](../test/vectors_hb_relay_v1.txt) (`<received frame> <relayed frame> <relayed packet>`), the relay produces the relayed frame. The C reference is [`tools/hb/`](../tools/hb/) (`make hb-test`); the fabricated packets come from `tools/hb/gen_packets.py`.
The frame bytes depend on these encoder choices, which are therefore part of the format:

* **TNC2 packet:** `SOURCE>DESTINATION[,PATH...]:INFORMATION`, split at the first `>`, the first `:` after it, and the commas between them; at most 15 path elements; the frame must be at most 255 bytes and the information field at most 255 bytes, otherwise the packet is not encodable and a plain frame is sent. The encoder decodes what it built and uses the frame only if the packet comes back byte for byte.
* **Address elements** (draft 0): `WIDEn` or `WIDEn-N` with `n` 1 to 7 and `N` 1 to `n`, optionally followed by `*`, is one byte `0 s nnn NNN` (`s` = the `*`, `N` = 0 when there is no `-N`). A callsign of 1 to 6 characters `A-Z0-9`, optionally `-SSID` (a decimal number 1 to 255 with no leading zero), optionally `*`, is `1 0 s xxxxx` (`xxxxx` the SSID, or 31 with the SSID in one extra byte after the callsign when it is above 30) followed by the 6 characters (padded with spaces) as four bytes, big endian, in base 40 over the alphabet space, `A` to `Z`, `0` to `9` (space is 0, `A` is 1, `0` is 27).
  Anything else (lower case, `-0`, `-05`, longer names) is a text element `1 1 lllll` and its 1 to 31 bytes.
  Tactical and other names that do not fit (longer than 6 characters, lower case, other characters) travel as text elements and are never rejected for their content.
  **A receiver drops a frame with an element that no encoder can produce (strict rule):** a `WIDE` byte with `n` = 0 or `N` above `n`; a callsign element whose four-byte value is 40^6 (4,096,000,000) or more or has a base-40 digit above 36 (the alphabet has 37 symbols), that is empty, or that has a space inside it; any element that runs past the end of the frame; a text element of length 0; a NUL byte (0x00) anywhere in the decoded text (an encoder is given a text string and cannot produce one). This covers the source, the destination escape and every path element, and it costs nothing because the receiver parses them anyway. The content of a text element is not checked, so the rule gives less protection where the path is text.
* **Destination:** a byte `00 iiiiii` with the index when the destination text equals an entry of the destination list (exact match, no `*`); otherwise the byte `0x3F` (code 63) and the destination as an address element.
* **Kind:** the encoder uses kind 3 when the information field starts with a position, otherwise kind 2. A position is: a data type indicator `!`, `=`, `/` or `@`; for `/` and `@` a timestamp of six digits and one of `z`, `h`, `/`; then an uncompressed position (`ddmm.hhN` or `S`, a table character, `dddmm.hhE` or `W`, a symbol character; 19 characters, digits only in the numbers) or a compressed one (a table character from `/ \ A-Z a-j`, four and four base-91 characters, a symbol character and three characters `c s T`; 13 characters).
  If the position block built from it does not rebuild the information field exactly, kind 2 is used.
* **Position block** (draft 0): a flags byte (bits 1-0 the data type indicator 0 `!`, 1 `=`, 2 `/`, 3 `@`; bit 2 timestamp; bit 3 compressed; bit 4 course/speed present, uncompressed only; bit 5 altitude present; bit 6 compressed with `c s` both spaces, so that only `T` follows), then the timestamp as three bytes (the six digits as a number times 4, plus 0, 1 or 2 for `z`, `h`, `/`), then the position: uncompressed is six bytes holding (latitude in hundredths of a minute times 2, plus 1 for south) times 2^22 plus (longitude in hundredths of a minute times 2, plus 1 for west), then the table and symbol characters, then (bit 4) three bytes of course times 1024 plus speed when the 7 characters `ddd/ddd` follow the position;
  compressed is seven bytes holding the latitude's four base-91 characters as a number times 2^27 plus the longitude's, then the table and symbol characters, then the one byte `T` (bit 6) or the three bytes `c s T`. Altitude (bit 5): the first `/A=` followed by six digits, or by `-` and five digits, within the first 255 bytes of the comment is removed from it and stored as one byte (its offset in the remaining comment) and three bytes (the altitude in feet, 24-bit two's complement). What remains of the comment is the text.
  **A receiver drops a position block that no encoder can produce (strict rule):** flags bit 7 set; bit 4 (course/speed) together with bit 3 (compressed); bit 6 without bit 3; a timestamp value above 999999 or with unit code 3; a latitude above 90 degrees or a longitude above 180 degrees (uncompressed, in hundredths of a minute: 540000 and 1080000); a compressed latitude or longitude of 91^4 or more; a course or a speed above 999; an altitude outside -99999 to 999999; an altitude offset beyond the comment.

* **Content:** the text is the information field (kind 2) or the comment that remains (kind 3), at most 255 bytes. PPM content is used when the payload (from the table: info-native for kind 2, comment-only for kind 3) satisfies `4 + n < 1 + L`; otherwise raw content, and an empty text is raw content with nothing after `V`.
* **Relay vectors:** the relay callsign is `N0RLY-1` and its tuple is the bytes `50 20 45 10`. The relay takes the first path element that has no `*`: `WIDE1-1` becomes `N0RLY-1*`; `WIDE2-2` becomes `N0RLY-1*,WIDE2-1`; with any other first unused element, or no path, the packet is not relayed and has no vector. The header gets the new path count and the RXT bit; the position block and the content field are copied byte for byte; the block is the old tuples, the relay's tuple at the index of its element, and the mask.

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
| Source, destination, `V`, the header's kind bits (PPM content) | LoRa CRC and the content CRC, end to end across relays | not applicable (the front is not cut) | the content CRC: a wrong destination list or a damaged source, destination or `V` fails it. A flipped kind bit that turns kind 2 or 3 into 0 or 1 is **not** caught, because those kinds carry no check |
| `3C FF 02`, path, path count, RXT bit | LoRa CRC only | not applicable | not covered (relays change them) |
| PPM payload | LoRa CRC, then the re-encode check and the content CRC | the re-encode check rejects every cut | the re-encode check passes about 0.4% of wrong-table decodes (up to 2.1% for some table pairs), the content CRC alone all but 2 of 360,000, both together 0 of 360,000 |
| Information field (position block and comment) | LoRa CRC and the content CRC, end to end across relays | the content CRC | the content CRC |
| RXT tuples and mask | LoRa CRC only, plus the marker and range checks | the marked mask byte | not covered (see below) |
| Raw content (`cc = 3`) | LoRa CRC only (`V` is checked for a known version) | nothing | nothing |

So the plan meets the VHF level per hop for channel noise on every byte, and is stronger than VHF for the information field across relays. It is **not** end-to-end protection for the path, the path count, RXT tuples or raw text, and a kind-bit flip into a raw kind is not caught.

**How the combined rates are meant to be read.** The end-to-end figure for damage to the PPM payload is (the probability that damage passes the LoRa CRC, about 1 in 65,536) times (the share of those frames our checks accept, measured below). That product assumes the damage is random-looking, that the two checks are independent, and that the rate is per damaged frame, not per frame sent. Errors of 1 to 3 bits cannot pass a 16-bit LoRa CRC at all, so the rows for few-bit flips describe an event that does not occur on the air; they test the checks, not the channel. Cuts and wrong tables have no LoRa CRC factor.

* **Truncation of the PPM payload:** the re-encode check. Alone, with no CRC, 0 of about 230,000 cut frames per cut size were accepted (286,100 texts, [`results-hybridB-reencode-alone.txt`](../tools/integrity/results-hybridB-reencode-alone.txt)). A review that tried every cut point on about 300,000 adversarial texts found that a payload that is a proper prefix or extension of the canonical encoding can never be accepted, so cuts, appended bytes and joined frames are rejected by construction.
* **Wrong table, wrong model, a second implementation that differs:** the re-encode check alone catches about 99.6% (0.4% accepted over 12 ordered pairs of four tables; one changed count in a table, 1 in 155), the CRC-16 alone all but 2 of 360,000, and both together all of them (95% upper bound about 1 in 120,000). The **content CRC-16** is therefore kept: computed by the origin over the information field, never touched by relays. (An earlier draft said the re-encode check cannot see a wrong table; that figure, 46%, was for primed wrong references with the old ending.)
* **Random damage to the payload after the LoRa CRC has passed:** not zero. A review with 180 million random payload damages found 1 frame accepted with wrong text (5.6e-9), and 6 of 90.3 million trials that replaced one of the last three payload bytes (6.6e-8); the wrong text differs in the last 4 to 9 characters. The re-encode check alone passes 1 in 327 of the random damage and the CRC-16 alone 1 in 64,000. Our own earlier run, 0 of about 2.85 million, bounds the combined rate only to about 1 in 950,000.
* **`L`** is protected by the CRC only (see Receiver, step 4).
* **The RXT block is unauthenticated.** Only the LoRa CRC protects it, plus two cheap checks: the marked mask byte makes every cut inside the block fail, and the range check fails random bytes about 98% of the time, but a single-bit flip of a tuple byte stays in range 76% of the time. In a test block, 61% of single-bit flips were accepted with changed values, and a two-bit mask change that keeps the number of tuples (`0x83` to `0x85`) was accepted every time and silently moves the tuples to different path elements.
  A per-tuple check written by the relay that owns the tuple would add protection; a block CRC recomputed by every relay would not.

Spec 11 spends 2 bytes on the content CRC-16 and none on the RXT block. **Adopted after review:** the content CRC also covers the kind bits, `V`, source and destination (no extra bytes), and `V` is mandatory in every frame of kinds 2 and 3, so the destination list has a version.

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

* **The extended CRC input.** The test code computes the CRC over the text only; the input of this section (kind bits, `V`, source and destination text, information field) and the packing of `V` as `cc vvvvvv` are not implemented in any test yet.
* **Kind 3 end to end.** No executable test covers a kind-3 frame (the CRC over the rebuilt position plus comment, the comment-only table) or the destination and path parse; the test code uses a 6-byte test frame with no `V` byte.
* **The RXT block with raw content (`cc = 3`).** The tail tests used PPM payloads. Raw text has no check of its own (the LoRa CRC only), so a cut that removes the whole block and part of the text is caught only if the new last byte fails the mask test; ASCII text almost always does, binary text may not. Not measured with real raw-text frames.

* The size of a native PPM frame with a real native encoder, `V` and the CRC included.
* The relay path end to end (path rewrite, RXT append, no PPM decode), including locating the end of the position block without decoding it.
* A native encoder or decoder with this plan, and a complete RF chain with RXT (F4MLV's stated next step).

## Extending the format later

The information kind (header bits 1 and 0) is full once kinds 2 and 3 are taken, so a future need does not have to squeeze into this header. **A new header is a new frame type byte** (`3C FF xx`), with its own layout, defined when it is warranted. Receivers drop any `3C FF` type they do not know and never show it as text, so a new type is invisible to old firmware.
Types in use or suggested: `01` plain text, `02` native (this plan), `03` capability frame (suggested in issue 7), `80` to `FF` the whole-packet PPM frames of spec 01 and spec 08. Several different headers can therefore live side by side, each used where it fits.
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
Open: who defines and maintains the profiles; how a role advertises what it supports (the two reserved destination bits, or a capability frame); and how many profile levels there are (for example level 0 plain text, level 1 hybrid B).

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

* **The destination byte needs no disambiguation inside kinds 2 and 3.** Draft-0 decoders reject any information kind above 1, so a frame of kind 2 or 3 never reaches an address-element parser built for draft 0; this plan defines the destination field of those kinds as the code byte (escape plus a draft-0 element), and there is nothing to tell apart. Kinds 0 and 1 are not used (decided 1 November 2026), so a raw-text fallback frame stays in the new family, as raw content (`cc = 3` in `V`).
* **Table version: decided.** `V = cc vvvvvv` is mandatory in every frame of kinds 2 and 3 (0 bytes on PPM frames, 1 byte on raw frames, about 0.1 byte per packet on average); `vvvvvv` names the PPM tables and the destination list together. Needs F4MLV's agreement.
* **Header gap: closed in part.** The content CRC now covers the kind bits, `V`, source and destination. A flipped kind bit that turns kind 2 or 3 into 0 or 1 is not caught, since raw kinds carry no check; with channel noise it cannot pass the LoRa CRC, so it needs a relay or encoder fault.
* **Distribute destination-list updates over the air.** The list is small (62 destinations are about 310 bytes, two frames, about 18 s at SF12), so a station that sees an unknown list version could ask its nearest neighbour, or an iGate could fetch and verify it and pass it on. The PPM tables (about 150 KB, 600 full frames) cannot go this way. Needs a version field, a signature from a key built into the firmware, rate-limited requests, and old versions kept. Not designed.
* **Upgrade path.** Receivers first and senders last, plain text as the fallback, the two reserved destination bits as a capability flag, unknown types dropped and never shown as text, old formats decoded indefinitely. Not written up.
* **The two reserved destination bits: decided to stay reserved.** All compressed LoRa will use this format until a later version, so a "decodes hybrid B" flag is not needed now; senders set the bits to 0, receivers ignore them, and a future version can give them a meaning.
* **Kind values 2 and 3, or a new frame type.** F4MLV suggested the unused kinds himself; the issue also floated `01` text, `02` native, `03` capability, `80` to `FF` PPM. A draft-0 decoder that treats unknown kinds as errors will drop these frames, which must be checked in its code.
* **Frozen tables.** The experimental set (`V = 1`) is trained on one capture. The first format needs a frozen set trained on several networks, and a decision on who maintains it (append-only).
* **RXT at the end.** K6ATV's choice: one rule for every frame (a relay appends without moving bytes, and RXT is never inside the content). It differs from draft 0, which puts the block after the path, so it needs F4MLV's agreement and a matching change in his firmware; frames from his current builds would not interoperate for RXT. Also **seven path elements instead of eight**, because of the marker bit.
* **Jon N7UV's view** on anything that changes what the tuple carries (the plan keeps the four measurements unchanged), and **F6BVP's** on identification: hybrid B keeps the source callsign recoverable in every frame, as F4MLV prefers.
* **`V` shares its byte with the content coding** (`cc`), so it costs one byte per frame of kinds 2 and 3.
* **Mic-E** is forwarded unchanged (decided: not worth converting). Open: whether draft 0 applies the destination-SSID path rule to a Mic-E destination.
* **The relay procedure** above is a draft: path rewriting in native elements (the index of the relay's element, moving mask bits when elements change), and time to hop at transmit time, need F4MLV's firmware to confirm.
