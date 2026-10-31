# Ambiguities found while implementing spec 01 + spec 10 independently

Result: all 20 vectors in test/vectors_lacf_v1.txt matched on the first run (encode == frame, receive == text);
`gen` on test/synthetic_packets.txt reproduces the vector file exactly. Nothing was special-cased.
Below is every place I had to guess. "Confirmed" = the vectors agree with my reading; several rare paths
are not exercised by the vectors at all (stated per item).

1. Model file layout is not described in the spec. Spec 01 says "a sorted list of contexts ... packed big-endian into a 3-byte key"
   with "(byte, count) pairs", but not that OFFS has contexts+1 entries, nor that offsets index ONE global SYMBOLS/COUNTS array shared by
   all orders (order 1 starts at 134 = end of order 0). Inferred from the arrays. Confirmed.
2. Key for orders < 3: spec says only "the preceding 0..3 bytes ... packed big-endian". I used right-aligned (zero-padded on the left,
   last byte least significant). Confirmed.
3. "for k = min(3, i) down to 0, while rem > 0": I test rem before each order. If rem could hit 0 mid-way the alternative reading
   is identical; vectors do not exercise it.
4. "if absent, continue": absent context leaves rem unchanged. Confirmed.
5. Widths: spec says rem is 64-bit and pr is 32-bit, but `(pr[s] + floor_share) * (65536 - 256)` needs up to ~48 bits; the spec should
   say the product is 64-bit. Python hides this; no vector tests it.
6. I did not assert "total <= 65536" or that pr[b] stays below 2^32; both follow from the sum of floors being <= initial rem.
7. Range coder `r = range / total` is floor division. Confirmed.
8. Carry: "add 1 to the output so far (propagate carry)" - implemented as ripple through 0xFF bytes (0xFF -> 0x00, continue). The spec does not
   say low may sit >= 2^32 across several encode steps with no shift in between (pending carry in bit 32); spec 10 step 1 handles that at
   the end, and `shift` handles it at the next shift. I do not know that any vector actually triggers a carry.
9. In `shift`, the carry test must come before "output byte (low >> 24)"; the pseudo-code order says so, but it is easy to miss.
10. Ending with nb=0: blk = 2^32, v = ceil(low/blk)*blk is 0 (low=0) or 2^32 (low>0). Stop needs v+2^32-1 <= hi. The spec does not say
    explicitly that an empty payload (6-byte frame) is legal; "n >= 0" implies it. Not exercised by the vectors.
11. Step 4 (v >= 2^32, carry into payload, v -= 2^32) is reachable for nb>=1; implemented as written; not exercised by the vectors, I believe.
12. Step 3 is a loop "stop" with nb=4 guaranteed; fine.
13. "Strictly shorter": `6 + n < 3 + L` with n the UNSTRIPPED payload length. The vector file has no '-' line (0 of 20), so the
    decline path is not tested by the vectors at all.
14. Decode: "pick the byte whose cumulative interval contains v" = smallest s with v < cum + freq[s]. `v = min(code / r, total - 1)`
    with integer division. Confirmed.
15. Decode shift: after `code <<= 8` plus next byte I mask to 32 bits; cannot matter because code < range < 2^24 at that point. Not specified.
16. Receiver: spec lists mark, decode, CRC, re-encode, tail but not what to do with L = 0 or a frame shorter than 6 bytes. I drop both.
    Order used: mark/length -> decode -> CRC -> re-encode -> tail. Order is not observable (every failure drops).
17. Receiver step 4: "re-encode with the sender's rules (header, CRC, ending)". The sender's rules include "MAY send only when strictly shorter".
    I do NOT apply the shortness test on receive (only payload bytes are compared). The spec should say which.
18. Step 4 "E's payload is not the first m bytes of the received payload": the "received payload" is everything after offset 6
    (a tail cannot be told apart before m is known). A received payload shorter than m is a mismatch -> drop.
19. Step 5: no tail allowed by default (allow_tail=False); receive() returns payload_len = m so the caller can locate the tail.
20. Mark byte: spec 10 step 1 says "drop if the third byte is unknown". I accept exactly 0x81. Spec 01 allows 0x80|version 1..63 and 0xC0 primed;
    other versions are dropped here because no other table exists. Whether a receiver holding several models should route by mark is not defined for LACF.
21. Text may in principle contain any byte 0..255 (model gives every byte freq >= 1); spec 10 does not restrict it. I allowed all.
22. CRC low byte first, check value 0x906E: clear and confirmed.
23. Spec 01's frame layout (payload at offset 4, no CRC) is still the text a reader sees first; for LACF the payload moves to offset 6 and the
    spec 01 "Finishing" (strip zeros) is replaced. Easy to mix up, though spec 10 says it.

Hard to follow
- Ending step 3's explanation of `blk` ("the 32-bit values a decoder could see if it read nb bytes and then anything at all") is cryptic; the loop itself is clear.
- Spec 10's Conformance mentions `make lacf-ending` and repo code, useless to an independent implementer.
- The vectors have no '-' line and no way to tell whether carries, nb=0, or the v >= 2^32 branch are exercised; extra vectors for those would help
  (e.g. a text whose compressed form is not shorter, and a one-character text).

## Resolved in the spec after this list (31 October 2026)

Items 1, 2, 5, 8, 9 (spec 01 notes added), 10 (nb = 0 cannot succeed; nb is always 1 or 2), 13 and 17 (receiver drops a frame the sender would have declined; this file's `receive` was changed to match),
16 and 18 (frames shorter than 6 bytes and L = 0 are dropped; the payload is everything from byte 6; a payload shorter than m is dropped), the cryptic `blk` explanation, and the missing edge vectors
(test/lacf_edge_packets.txt). Items 3, 4, 6, 7, 11, 14, 15, 19, 20, 21, 22 and 23 were confirmed or left as written (20: the mark is provisional, see spec 10 Open).
