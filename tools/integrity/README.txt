Truncation and integrity tests (see docs/truncated-payloads-and-integrity.md)

The packet texts are NOT included: they hold real callsigns and positions. Make your own file with one APRS text (the part after ':' of a TNC2 line,
or whole TNC2 lines, as each program's header says) per line, from your own receiver log or an APRS-IS capture filtered on the LoRa destination codes.

Files
  trunc_test.c        Dire Wolf C decoder (src/loracompress.c): cut a full frame, classify the decode
  trunc_esp.cpp       the same on the ESP32 iGate library (lib/AprsCompress), built natively
  trunc_store.cpp     a garbled full frame stored as a reference, then the next primed frame
  ovr_test.cpp        how many bytes past its end the decoder reads; relay-tail (RXT) test
  primed_c_test.c     check C on primed frames against wrong references (needs the reference codec in this repo: build line in the file)
  checks_test.cpp     checks A (ends exactly), B (CRC-8 0x1D), C (decode, re-encode, compare)
  indep_test.cpp      the ending that decodes the same whatever follows
  igate-library-private-copy.patch   the library changes the ending experiments need (encodeCanon, decodeCanon, encodeIndep); a private copy, not for the iGate repo
  results*.txt        the numbers quoted in the doc

Build (examples; adjust include paths to your checkouts)
  c++ -O2 -std=c++17 -I <igate>/lib/AprsCompress/src trunc_esp.cpp <igate>/lib/AprsCompress/src/*.cpp -o trunc_esp
  cc  -O2 -include string.h -include stdio.h -include stdlib.h -I <direwolf>/src trunc_test.c <direwolf>/src/loracompress.c -o trunc_test   (macOS needs the -include lines)
The checks and ending tests need the patched private copy of the library first (git apply the .patch to a COPY of lib/AprsCompress).
The programs were not built from this directory unchanged; the include paths are the part you will change.
