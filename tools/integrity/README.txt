Truncation and integrity tests (see docs/truncated-payloads-and-integrity.md)

The packet texts are NOT included: they hold real callsigns and positions. Make your own file with one APRS text (the part after ':' of a TNC2 line,
or whole TNC2 lines, as each program's header says) per line, from your own receiver log or an APRS-IS capture filtered on the LoRa destination codes.

Run first (needs only this repository):  make integrity
  trunc_ref.c         full-frame cuts, bit flips and random payloads, plain decoder vs check C, on fabricated packets or on your own text file;
                      results-ref-fabricated.txt (what make integrity prints) and results-ref-real.txt (the author's real texts, which are not in the repository)

LACF (spec/10):  make lacf-ending   (ending, CRC-16 and receiver against test/vectors_lacf_v1.txt and on fabricated packets)
                 make fast-freq     (the faster build_freq must equal the reference; TEXTS=file for your own texts)
  lacf_core.h        the ending, CRC-16, encoder and receiver of spec/10 as code (include after reference/aprs_ppm.c)
  lacf_ending.c      property tests for it (also: build/lacf_ending stress TEXTS TRIALS; gen / check for the vectors)
  fast_freq.h        a faster build_freq with byte-identical output; fast_freq_test.c proves it
  rxt_tail_test.py   truncation and flips on the tail of a native PPM frame with RXT tuples (spec 11); results-rxt-tail-{plain,marked}.txt
  lacf_py/           a second implementation in Python, written from spec 01 and spec 10 alone (lacf.py, test_lacf.py, AMBIGUITIES.md); build/lacf_ending recv is the C side of the receiver comparison
  esp32-timing/      PlatformIO project for a LILYGO LoRa32 v2.1: pio run -e keysram -t upload (envs: lora32 = reference, lora32fast, keysram, lora32ram, lora32prof)
  results-esp32-timing.txt, results-lacf-*.txt   the numbers quoted in the doc

Files
  trunc_test.c        the same cut test on the C decoder in the author's Dire Wolf port (his test bench; not needed for the ESP32 or T-Deck results)
  trunc_esp.cpp       the same on the ESP32 iGate library (lib/AprsCompress), built natively
  trunc_store.cpp     a garbled full frame stored as a reference, then the next primed frame
  ovr_test.cpp        how many bytes past its end the decoder reads; relay-tail (RXT) test
  primed_c_test.c     check C on primed frames against wrong references (needs the reference codec in this repo: build line in the file)
  checks_test.cpp     checks A (ends exactly), B (CRC-8 0x1D), C (decode, re-encode, compare)
  indep_test.cpp      the ending that decodes the same whatever follows
  igate-library-private-copy.patch   the library changes the ending experiments need (encodeCanon, decodeCanon, encodeIndep); a private copy, not for the iGate repo
  results*.txt        the numbers quoted in the doc

The tests below are the ones behind the doc's ending, relay-tail and ESP32 library numbers. They need the ESP32 iGate firmware's library checked out, and the ending tests need the patched copy.
Build (examples; adjust include paths to your checkouts)
  c++ -O2 -std=c++17 -I <igate>/lib/AprsCompress/src trunc_esp.cpp <igate>/lib/AprsCompress/src/*.cpp -o trunc_esp
  cc  -O2 -include string.h -include stdio.h -include stdlib.h -I <direwolf>/src trunc_test.c <direwolf>/src/loracompress.c -o trunc_test   (macOS needs the -include lines)
The checks and ending tests need the patched private copy of the library first (git apply the .patch to a COPY of lib/AprsCompress).
The programs were not built from this directory unchanged; the include paths are the part you will change.
