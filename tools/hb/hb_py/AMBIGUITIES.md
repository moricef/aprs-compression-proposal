# Ambiguities found while writing hb.py from spec 11

Result: 500/500 encode, 500/500 decode, 246/246 relay (and 246/246 decode of the relayed frames) on the first run after one bug of mine (a decoder byte-index slip, not a spec issue) and one spec reading (item 1). Nothing was special-cased. "Confirmed" means the vectors would have failed under another reading; "untested" means the vectors never exercise the point, so my reading is only a guess.

## Needed a second try

1. **Relay tuple "the bytes `50 20 45 10`" is decimal.** Spec 11 Conformance: "its tuple is the bytes `50 20 45 10`". I read hex (0x50 0x20 0x45 0x10); the vectors end `...32142d0a81` = 50, 20, 45, 10 decimal. The spec should write `32 14 2d 0a` or say "decimal". Also the header says "each the RXT character minus 33", which suggests decimal values, but the Conformance line gives no base. Confirmed (by failing first).

## Format points where the text is silent or loose

2. **Frame size limit.** "the frame must be at most 255 bytes". I counted the whole frame including `3C FF 02`. No vector is near the limit, so untested. Also unsaid: whether a relayed frame (grown by RXT) over 255 bytes is dropped; I do not check it.
3. **Is the encoder required to prefer a plain frame when native is longer?** Spec says only that unencodable packets get a plain frame. I never compare with the plain length. No vector shows `-`, so nothing tests the non-encodable path at all (0 of 500 are `-`).
4. **WIDE vs callsign.** "`WIDEn-N` with n 1 to 7 and N 1 to n ... is one byte". I take `WIDE2-3` (N > n), `WIDE1-0`, `WIDE8-1` as not WIDE and so, when they fit, as callsigns (`WIDE2-3` is a legal 5-char callsign with SSID 3); `WIDE1-1` is WIDE (confirmed: byte 0x09), though it also fits the callsign rule. The spec does not say WIDE wins, only implies it. Priority confirmed for in-range WIDE; the out-of-range choice is untested.
5. **`WIDEn` without `-N`** ("N = 0 when there is no `-N`"). Decoding `N=0` as `WIDEn` with no suffix, so `WIDEn-0` can only be a text element. Untested.
6. **Callsign details.** "a decimal number 1 to 255 with no leading zero": so `-0`, `-05`, `-256` become text elements. Extra SSID byte: "31 with the SSID in one extra byte after the callsign when it is above 30". SSID exactly 31 therefore uses the extra byte (the field value 31 is both the marker and the case). The extra byte follows the four character bytes. Confirmed by vectors with `-31`, `-44`.
7. **Base-40 digit order.** Spec says "four bytes, big endian, in base 40 over the alphabet space, A..Z, 0..9" without saying the first character is the most significant digit. I assumed first character = most significant. Confirmed.
8. **Text element with `*`.** An element like `abc*` or `nocall-1*` is a text element whose bytes include the `*` (no `s` bit). Assumed; untested (generator has `nocall-1` without a star only).
9. **Empty or over-long elements** (empty source, `A>B,,C:`, text element over 31 bytes, more than 15 path elements): treated as not encodable (`None`). Spec only says "at most 15 path elements".
10. **Lenient address decoding.** Receiver rules do not say what to do with non-canonical elements (a callsign with an inner space, SSID extra byte below 31, a WIDE byte with n = 0 or N > n, base-40 value >= 40^6). I drop the unrepresentable ones (inner space, empty call, value >= 40^6, n = 0, N > n) and accept the rest (e.g. extra byte 0). Untested.
11. **Destination code and the list file.** The list file is `<index> <name>` lines plus `#` comments; I read it as a code -> name map. Code 62 is reserved and also absent from the file (62 entries 0..61). The destination must match exactly, including entries with an SSID such as `APLT00-1`; no `*`. The two top bits `ff` are ignored on receive and set to 0 on send (per spec). When a destination text matches a list entry the encoder must use the code, never the escape; that is implied by "Destination", not stated as a MUST for the decoder, which accepts both (escape for a listed name is accepted; untested).
12. **Where the destination sits.** The frame diagram lists `source | destination | path`, and the destination for kinds 2/3 is one code byte or `3F` + element. For kinds 0/1 the spec says only that they "stay draft 0 exactly" and "cannot use the table"; I decode their destination as a plain address element. See item 22.

## Position block and kinds

13. **What "the comment" is for altitude.** "the first `/A=` followed by six digits, or by `-` and five digits, within the first 255 bytes of the comment is removed ... stored as one byte (its offset in the remaining comment)". Readings: (a) first literal `/A=` occurrence that is followed by a valid number, or only the very first `/A=`; (b) the match must start vs end inside byte 255. I use: regex search for `/A=(-\d{5}|\d{6})` in the first 255 bytes of the comment (match must fit inside), so a malformed first `/A=` does not hide a later good one. "Offset in the remaining comment" I read as the offset in the comment *after* removal, which equals the offset in the original (the text before the match is unchanged). The comment is what follows the table/symbol, the course/speed (uncompressed) or the `T`/`c s T` bytes (compressed). Basic form confirmed by vectors; the 255 boundary and multiple `/A=` cases are untested.
14. **Altitude edge values.** `/A=-00000` (negative zero) cannot rebuild (it would come back `/A=000000`), so by the "does not rebuild the information field exactly" rule the packet becomes kind 2. Same for any case where the block does not round-trip: I implement that rule literally by building the block, parsing it back and comparing, so every such case (minutes >= 60 with hundredths that overflow, e.g. `4999.99N` which decodes as 50 deg, longitude over 22 bits, 3-digit course over 999) falls to kind 2. Untested by vectors beyond the generator's "position-looking but malformed" cases.
15. **"Position" definition.** Uncompressed: 19 characters "digits only in the numbers": I require N/S and E/W (no space ambiguity) and any table and symbol byte. Compressed: 13 characters; base-91 characters taken as 33..123 (`!`..`{`), symbol and `c s T` any byte. Spec does not give the base-91 range or restrict the symbol byte. Timestamp: six digits then `z`, `h` or `/`, only after `/` or `@`. For `!` and `=` there is never a timestamp.
16. **`c s` both spaces** (bit 6) means exactly 0x20 0x20; `c` = space with `s` other keeps all three bytes. Assumed; untested.
17. **Course/speed on uncompressed positions**: "when the 7 characters `ddd/ddd` follow the position": any three digits, `/`, three digits. Stored `course*1024+speed`; speed up to 999 fits. The decoder also checks both values <= 999. Confirmed by vectors for the normal case.
18. **Timestamp value**: `digits * 4 + {0,1,2}`; low bits `3` are invalid and I drop such frames. The decoder also does not say whether bit 2 (timestamp) may be set for `!`/`=`; I accept it and print the timestamp after `!`. A decoder could reasonably drop it.
19. **Flag combinations**: bit 7 set, bit 4 with bit 3, bit 6 without bit 3: not covered by the spec; I drop them.
20. **Kind 3 with an empty comment** is raw content `V` with nothing after it (stated). Kind 2 with an empty information field: I do the same (raw, empty); spec says "an empty text is raw content with nothing after `V`" which I read as covering both kinds. A kind 2 frame with an empty field and PPM would need L = 0, which is invalid, so raw is forced anyway.
21. **Altitude offset larger than the comment** on decode: dropped by me; not specified.

## Kinds 0 and 1 (draft 0) — spec 11 does not contain the layout

22. Spec 11 says kinds 0 and 1 are "draft 0 exactly" but draft 0 is not in the file. I implemented a guess for the receiver only (no encoder): header, source element, destination as an address element, path elements, then for kind 1 a position block, then raw text to the RXT block or frame end; header bit 2 (Huffman) set means drop. This is untested: neither vector file has a kind 0/1 frame, so the relay and the Receiver rule 3 text for kinds 0 and 1 is a guess.

## RXT block and receiver

23. **Tuple field order and maxima.** "RSSI, SNR, frequency offset, time to hop" with maxima "RSSI 89, SNR 88, frequency offset 89, time to hop 89". I applied them in that order; the relay vectors' tuple (50 20 45 10) satisfies them but cannot distinguish the order. Hand-made tests only.
24. **"at least `4k+1` bytes remain after the position block and, for PPM kinds, after the content CRC."** I read this as: the payload may be zero bytes long, and the block must lie entirely after `V`, `L`, CRC (PPM kind) or after `V` (raw content). For raw kinds 2/3 the spec says "after the position block", which would let the block overlap `V`; I make `V` also inviolable (the block must start after `V`) — the same hole exists for raw V unmentioned. For kinds 0/1 I use the position block's end.
25. **Which check fails first** is not important for accept/drop but the spec says "The CRC is checked before the comparison" (Receiver equivalence), and Receiver step 5 also drops "not shorter than 1+L". I do: parse, decode, CRC, re-encode compare, size rule. Result is the same accept/drop for all inputs; I return error names such as `crc`, `not_canonical`, `should_be_raw`, `rxt_out_of_range`; the spec fixes no names.
26. **Step 1 vs parse order.** Step 1 says to drop unless the table-set version in `V` is known, but `V` sits after the addresses and position block, so it can only be checked after parsing step 2. Cosmetic.
27. **Decoder zero-padding.** Spec 01 says the decoder pads missing input with zeros; spec 11 says the payload lies exactly between CRC and block. I still pad (the last symbols read bytes past the payload end) because the 32-bit register is read ahead; the re-encode comparison is what makes this safe. The spec should say this explicitly: "the decoder reads past the payload as zeros, and acceptance does not depend on it."
28. **CRC input.** "the source as TNC2 text ... the destination as TNC2 text (decoded through the destination list) ... the information field". Source text as written (includes SSID, `*` if any); `header & 0x07`, not `& 0x03`. Confirmed by the 166 PPM-content vectors.
29. **Reserved header bit 2 on kinds 2/3**: dropped. Untested.
30. **A kind-2 frame whose `V` is raw but which carries RXT** works as written; the vectors have no RXT frames at all in the encode/decode set.

## Relay

31. **The relay rule is in Conformance, not "Relay (draft)".** The draft says "WIDEn-N loses one hop", Conformance restricts it to `WIDE1-1` and `WIDE2-2` as the first unused element (`WIDE2-1`, `WIDE3-3`, `TCPIP*`, no path = not relayed). I followed Conformance. 246 vectors: 58 `WIDE1-1`, 55 `WIDE1-1,WIDE2-2`, 49 `WIDE1-1,WIDE2-1`, 44 `N0RLY-3*,N0RLY-5*,...`, 40 `WIDE2-2`. Confirmed.
32. **Mask/tuple handling when the incoming frame already has a block** is untested: no relay vector has one (all 246 inputs have header bit 3 clear). My reading: bits stay with their elements; for `WIDE2-2` (one element becomes two) old bits at index >= j move up by one; for `WIDE1-1` an old tuple at j (on an unused element, which is odd) is replaced by the relay's. If the relay's index is 7 or more there is no bit for it, so the frame is relayed with no own tuple (the spec's "sends none"), but old tuples that would shift past bit 6 make me return `None`. The 15-element path limit also returns `None`.
33. **Header on relayed frames.** "new path count and the RXT bit": I keep `header & 7` unchanged and keep the reserved bit as-is. Tuple order ascending by index. Confirmed.
34. **The relay accepts first** (Receiver rules) before forwarding. The vectors do not test relay of invalid frames.

## Spec 01 and the coder (the model part)

35. **Spec 01 ordering of probabilities** is clear; I did not find ambiguity beyond: "while rem > 0" (checked at the top of each order step), `pr[b] +=` with no symbol exclusion (confirmed by vectors), counts used as stored (255 is a count, not a cap).
36. **Table parsing.** Spec 01 says the `OFFS` of all orders index one shared array and `OFFSk` has one more entry than contexts: true in both files; order 0 key is `0,0,0`. Header "model version 2" is not the spec's `V` (explained in spec 11). The `KEYS` of orders below 3 are zero-left-padded (confirmed).
37. **Encoder ending**: spec 11 text is exact; one small thing worth stating: after step 1's carry the loop for `nb` may hit `v >= 2^32` only with the carry added to the bytes already written (step 4), which can ripple into the previous bytes. Implemented as written; all 500 frames agree. Spec 11 says `nb` is "always 1 or 2"; I did not assert it.

## Other

38. **CLI input.** `gen` reads packet lines as raw bytes split on `\n` only (packets contain control bytes 0x1c..0x1f); a trailing `\r` is stripped and `#`-lines skipped. `recv` takes a frame hex per line, ignores text after the first space.
39. **Hard to follow:** the spec jumps between "the content field" (V ... payload), "payload" and "text"; the sentence in Receiver 3 about where the RXT block starts ("at least 4k+1 bytes remain ... after the CRC") is easy to misread as the block starting at the CRC. A single worked example frame (hex, annotated) in the spec would have saved the most time: I derived the frame layout by decoding the first vector by eye.
40. **The Size / Integrity / Open sections** state a measured result "Two independent implementations (C and Python) agree"; this implementation is a third, written from the text, and agrees on all 500 + 246 vectors; it does not test the damage statistics claimed there.

## Resolved in the spec and the code (1 November 2026)

Every item above was answered by a spec sentence, a rule in the code, a vector, or a decision. "Vector" means a line in `test/`: the edge packets are in `hb_edge_packets.txt` (part of `vectors_hb_v1.txt`), the rejection vectors in `vectors_hb_reject_v1.txt`, the relay vectors in `vectors_hb_relay_v1.txt` and `vectors_hb_relay_chain_v1.txt`.

| Items | Answer |
|---|---|
| 1 | The tuple is decimal; the spec says so and gives `N0RLY-(k+1)` with `50+k 20+k 45+k 10+k`. |
| 2 | The 255 bytes count the whole frame; a relay whose result would exceed 255 does not relay (edge vectors: one packet that relays at 251 bytes, one that does not). |
| 3 | Decided: the encoder sends the hybrid frame whenever the packet is encodable and does not compare it with the plain frame. |
| 4, 5, 6, 8 | The one-byte WIDE form wins when it matches exactly; look-alikes follow the callsign and text rules; SSID 31 uses the extra byte; a trailing star of a text element is one of its bytes. Edge vectors for each. |
| 7 | The first character is the most significant base-40 digit (spec). |
| 9 | The not-encodable cases are listed in the spec (empty element, text element over 31, more than 15 path elements, frame or information field over 255, missing `>` or `:`); 8 of the 548 vectors are `-`. |
| 10 | Strict rule: the receiver drops every element no encoder can produce, also an SSID extra byte below 31 and a callsign digit above 36 (spec, reject vectors). |
| 11 | The encoder must use the destination code when the destination is listed; a receiver also accepts the escape form (spec). |
| 12, 22 | Kinds 0 and 1 are not used and are dropped (decided; reject vectors). |
| 13, 14, 15, 16 | The spec states the altitude search, when a position falls back to kind 2 (`/A=-00000`, `4999.99N`: found by the edge packets and fixed in C), the base-91 range and the `c s` rule. |
| 17, 18, 19, 21 | Strict rule for the position block (spec, reject vectors), including a timestamp flag on `!` and `=`. |
| 20 | An empty text is raw content with nothing after `V`, for both kinds (spec). |
| 23 | The order is fixed by the rejection vectors, one per field above its maximum. |
| 24, 26 | Receiver steps 1 to 3 rewritten: version is read after parsing; the RXT block lies after `V` (raw) or after `V`, `L` and the CRC (PPM). |
| 25 | Reason names are not part of the format (spec). |
| 27 | The decoder may read zeros past the payload; acceptance never depends on it (spec). |
| 28, 29 | Confirmed; header bit 2 set is in the reject vectors. |
| 30 | RXT on raw content is covered: about two thirds of the relay vectors (197 of 298, 129 of 199) are raw frames. |
| 31, 32, 33, 34 | The relay vectors use three cases; a deployed relay may use its full rule (spec). Tuples belong to used elements, so no mask bit moves. An eighth element or later relays without its own tuple (C changed to match). A relay refuses invalid frames (Python test). Vectors for frames that already carry tuples: `vectors_hb_relay_chain_v1.txt`. |
| 35, 36, 37, 38 | Confirmed, nothing to change. |
| 39 | Terms defined (content field, text, payload, body) and the worked example added. |
| 40 | Spec sentence updated: the C reference and this implementation agree on all vectors and 122,689 intact and damaged frames. |
