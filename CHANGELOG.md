# Changelog

LACF (spec 11) uses semantic versions: MAJOR = wire format version (the low six bits of `V`), MINOR = backward-compatible addition, PATCH = wording only. See "Versions" in `spec/11-native-ppm-frame-byte-plan.md`. The current number is in the file `VERSION`.

## 2.0.1 - 2 November 2026 (wording only, no byte changes)

Protocol version 2 and table version 0 are unchanged: every frame of 2.0.0 is a frame of 2.0.1. Documentation fixes after the layout settled:
* the frame diagram no longer says "V starts with binary 11" or "V = 1" (V is `10`: protocol 2, table 0), and the header kind bits are shown as form and position block;
* `docs/lacf-frame-layout.md`, `docs/lacf-byte-reference.md` and `docs/alternatives-considered.md` describe plain and LACF content, the four kinds and "table version" (not "table set");
* spec 11: the header-gap note is closed, and the implementation notes say every kind of `3C FF 02` instead of kinds 2 and 3.

## 2.0.0 - 2 November 2026 (protocol version 2, table version 0)

**Breaking.** Three changes to the bytes:

1. **Plain content has a CRC-16:** `V | CRC-16 (2 bytes, low first) | text`. The CRC input is the same as for LACF content (header kind bits, `V`, source, `>`, destination, `:`, information field), so a relay still never recomputes it.
2. **The form is in the header:** kind bit 1 = LACF (1) or plain (0), kind bit 0 = position block. Kinds 0 and 1 are plain (without and with a position block), 2 and 3 are LACF. Header bit 2 stays reserved. The two content codings are no longer in `V`.
3. **`V` is `vvvvv ttt`:** five bits of protocol version (2) and three bits of table version (0). The table version names the destination list and the LACF tables, revised together; the files keep their old names (`tocall_set1.txt`) but are table version 0.

* Why: F4MLV's RF test of 2 November 2026 (38 cases, commit `500c4c63`) showed that a plain frame cut by one byte before transmission was accepted and relayed with different text (cases B2, B6). Plain content had only the LoRa CRC, which is computed after the cut.
* Size rule: LACF is used when `4 + n < 3 + L` (was `4 + n < 1 + L`). Plain frames are 2 bytes longer. On 1,000 real RF packets: 11.7% are plain, +0.23 bytes per packet on average (55.89 to 56.12), +17 ms per packet at SF12 and +0.5 ms at SF7. 88.3% of packets are unchanged.
* Plain content meets the 1 in 65,536 target and does not beat it: 271 of 20,000,000 plain frames with 1 to 8 random bytes appended were accepted (1 in 73,801). LACF content also has the re-encode check. CRC-16 kept (a CRC-24 on plain would cost one more byte per plain frame).
* Test builds before this version sent `V` as `cc vvvvvv` (`01`, `02`, `C1`, `C2`). Those values read as protocol versions 0 and 24 and are dropped.
* The LACF payload, the tables, the destination list and the RXT block are unchanged.
* On the air, 2 November 2026 (LoRa32 iGate, T-Deck Plus tracker, hill Pi with Dire Wolf; 433 MHz, SF12): every exchange decoded with `V = 0x10` and a valid CRC, in both directions, with no frame dropped: T-Deck to LoRa32 (plain form, kind 1), LoRa32 to T-Deck and back through a relay with an RXT tuple (LACF form, kind 3), the hill to both ESP32 boards (LACF, kind 3, 39 B instead of 57 B) and a plain text frame re-encoded by the LoRa32 digipeater (kind 1). One site, a few metres apart, no range or error-rate measurement; truncated and modified frames were not sent over RF.
* Tests: C and Python references agree (0 disagreements on 3,000 packets and 143,224 damaged frames); every cut of every plain frame is rejected; F4MLV's cases A, B and C replay as expected (software replay, not RF).

## 1.0.0 - 1 November 2026 (wire format version 1, test builds only)

First byte plan: native frame `3C FF 02`, PPM-coded content with CRC-16, plain fallback without a check, RXT block at the end.
