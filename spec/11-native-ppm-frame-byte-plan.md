# Proposal draft 11 - Native PPM frame: byte plan (LACF)

| | |
|---|---|
| Status | **First draft of the byte plan F4MLV asked for in issue 7 of moricef/LoRa_APRS_Tracker (comment 17). Written by K6ATV, not yet seen or agreed by F4MLV. Nothing here is implemented in a native encoder; the integrity part is tested (below).** |
| Builds on | moricef's native frame, draft 0 (`docs/LORA_APRS_NATIVE_FRAME_DRAFT0.md`, `moricef/LoRa_APRS_iGate`, branch `rxt`) and spec 10 (ending, CRC-16, re-encode check) |
| Replaces | the standalone whole-text frame `3C FF 81` of spec 10 as the first experimental common format; spec 10's ending and receiver rules stay and apply to the PPM payload below |

## Scope (agreed in issue 7, comments 14 to 17)

A first experimental common format: native source, destination and path; the versioned destination table (v2); lossless draft-0 positions; standalone PPM for the remaining content.
Relays update the path and append RXT measurements without decompressing the PPM content. Extended callsigns and SSIDs, packet content, and the link between each measurement and its relay are preserved exactly.
Receivers drop any frame whose format, model or table version they do not know (append-only tables). Primed mode, rounding, alternative callsign coding and the capability frame are separate experiments and are not part of this plan.

## Frame

    3C FF 02 | header | source | destination | path... | [position block] | PPM section | [RXT block]

`3C FF 02`, the header byte, the address elements and the position block are those of draft 0. What this plan adds is the PPM section, the new header values that announce it, and the place of the RXT block.

### Header byte (draft 0 layout, two additions)

| Bits | Draft 0 | This plan |
|---|---|---|
| 7-4 | number of path elements, 0 to 15 | same |
| 3 | an RXT block follows the path | **PPM frames (kind 2, 3): an RXT block ends the frame.** Kinds 0 and 1: as draft 0 |
| 2 | the text is Huffman coded | PPM frames: reserved, must be 0 (see Open: destination table) |
| 1-0 | information kind: 0 raw, 1 position (2 and 3 unused) | **2 = information field as text, PPM coded. 3 = position block, then the comment PPM coded** |

A receiver that does not know kind 2 or 3 MUST drop the frame and never show it as text.

### PPM section (kinds 2 and 3)

    V        1 byte    model version of spec 01 (1 = model v1); a receiver drops a version it does not hold
    L        1 byte    number of text bytes coded, 1..255 (kind 2: the information field; kind 3: the comment that remains after the position block)
    CRC      2 bytes   CRC-16/X-25 (spec 10) of the information field as the iGate would upload it, low byte first
    payload  n bytes   range coder bytes of spec 01, ending as in spec 10 ("Ending")

The CRC covers the information field (the part of the TNC2 packet after the first `:`), rebuilt from the position block and the decoded text. A relay rewrites the path but not the information field, so it never has to change the CRC.
The payload's length is not sent: the receiver finds it as described below.

### RXT block (header bit 3 set, kinds 2 and 3)

    tuple 1 | tuple 2 | ... | tuple k | mask          the last byte of the frame is the mask

* A tuple is 4 bytes as in draft 0: RSSI, SNR, frequency offset, time to hop, each the RXT character minus 33, so each byte is 0 to 93.
* The mask has **bit 7 set** as a marker. Bit j (0 to 6) set means that path element j (0 is the first element after the destination) carries a measurement. Tuples are in ascending order of j. `k` is the number of mask bits 0 to 6 that are set.
* The block is `4k + 1` bytes. Bits at or above the path element count are not allowed.

The marker is what makes the block safe against truncation: a tuple byte is never 0x80 or more, so after a cut inside the block the last byte can never be mistaken for a mask (tested below). It costs one path element against draft 0 (7 instead of 8).

A relay builds its outgoing frame from the front: new header and path, then the position block and the PPM section copied byte for byte, then the old tuples, its own tuple, and the new mask. If there was no block it creates one (tuple, mask) and sets header bit 3.
A relayed frame grows by 5 bytes for the first relay and 4 for each further one.

## Receiver (normative)

1. Drop the frame unless it starts `3C FF 02` and the information kind, the model version `V` and the table versions are known.
2. Parse header, addresses and path, and the position block (kind 3), as draft 0.
3. If header bit 3 is set: the last byte is the mask. Drop the frame unless bit 7 is set, `k >= 1`, every set bit is below the path element count, and at least `4k + 1` bytes remain after the CRC. The RXT block is the last `4k + 1` bytes; the payload is everything between the CRC and the block.
   If bit 3 is clear, the payload is everything after the CRC.
4. Decode `L` text bytes from the payload with the range decoder, rebuild the information field, and drop the frame unless its CRC-16 matches.
5. Re-encode the `L` text bytes with the sender's rules (spec 10 "Ending"). Drop the frame unless the result is **exactly** the payload (same length, same bytes).
6. Only an accepted frame may be shown, uploaded, relayed, or kept as a reference.

Steps 4 and 5 may be done in one pass over the model (spec 10 "Receiver"). Because the payload lies exactly between the CRC and the RXT block, there is no "tail" to guess about; a damaged or cut frame changes where the payload ends and step 5 fails.

## Integrity: what is measured

Spec 10's results apply to the payload: cut frames, 0 wrongly accepted at every size; with the CRC-16 and the re-encode check together 0 of about 2.85 million damaged frames accepted.

The RXT block is new. [`tools/integrity/rxt_tail_test.py`](../tools/integrity/rxt_tail_test.py) cuts 1 to 16 bytes off the end of 600 real packets carrying 0 to 3 tuples, and flips 1 and 3 bits, receiving with the rules above
(results: [`results-rxt-tail-marked.txt`](../tools/integrity/results-rxt-tail-marked.txt)):

* **With the marked mask byte: 0 of 21,564 cut frames were accepted**, at every cut size and tuple count.
* **Without the marker** (a plain mask in the last byte, [`results-rxt-tail-plain.txt`](../tools/integrity/results-rxt-tail-plain.txt)) 73 of the same 21,564 cut frames were accepted with a wrong mask (cuts of 4 or 8 bytes inside the block, 17 to 34 of 600 for the cut sizes concerned), because a left-over tuple byte can look like a valid mask. That is why the marker is in the plan.
* **Bit flips inside the tuples are not detected.** Nothing but the LoRa CRC covers the RXT bytes (a single flipped bit landed in a tuple and was accepted in 10% of frames with one tuple and 25% with three). This is the protection every field has today; extending the CRC-16 over the block would cost 2 bytes on relayed frames and would have to be recomputed by every relay. Not in this plan.

## Not yet measured

* The size of a native PPM frame. The 50.2% saving of the whole-text frame (spec 10) is not the figure for this layout; it needs the native encoder (draft 0) to produce the source, destination, path and position bytes.
* The relay path end to end (path rewrite, RXT append, no PPM decode), and the cost of locating the end of the position block without decoding it.
* A native encoder or decoder with this plan, and a complete RF chain with RXT.

## Open

* **Kind values 2 and 3, or a new frame type.** The issue suggests types `01` text, `02` native, `03` capability, `80` to `FF` PPM. Using the two unused information kinds keeps `02` and the flag F4MLV and K6ATV agreed on; a draft-0 decoder that treats them as errors will drop these frames, which must be checked in its code.
* **Destination table v2.** Agreed in scope, not coded here. Every first byte of a draft-0 address element is taken, so a table index needs a new mechanism; header bit 2 (reserved above) is one candidate, a one-byte index with an escape to a draft-0 element another.
* **Where the RXT block goes for kinds 0 and 1.** Draft 0 puts it after the path; this plan puts it at the end for PPM kinds only.
* **Seven path elements instead of eight** for tuples, and whether RXT's current cap of three tuples is the limit that matters.
* **Jon N7UV's view** on anything that changes what the tuple carries (the plan keeps the four measurements unchanged).
* **The frame mark question of spec 10 is moot here** (the type byte is `02`); it remains for the standalone `3C FF 81` frame if that is still wanted.
* **The V byte** (one byte per frame) could share bits with another field.
