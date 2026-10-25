# Comparing the native LoRa APRS frame with APRS-PPM

`compare_native.cpp` runs two encoders on the same packets and prints the mean frame size of each:

* **native**: the draft-0 "native LoRa APRS frame" of moricef (header `3C FF 02`), using moricef's encoder unchanged;
* **ppm-1st**: an APRS-PPM full frame (`3C FF 81`), using the codec library `lib/AprsCompress` of the iGate branch
  (<https://github.com/radiohound/LoRa_APRS_iGate/tree/feature/order3-compression/lib/AprsCompress/src>), the same
  code the firmware runs;
* **ppm-alt**: APRS-PPM as deployed with `compressFullEvery` 2: a full frame, then a primed frame (`3C FF C1`)
  coded against the station's previous full or plain packet, then a full frame again. The reference store is
  unlimited here, and it assumes the receiver heard the previous frame.

"plain" is the text plus the 3-byte LoRa APRS header. Every native frame is also decoded and compared with the
original packet; the output reports how many failed (none did in the runs described in
`docs/alternatives-considered.md`).

## Build

The native encoder is not part of this repository: it is GPL-3.0 and belongs to its author. Fetch the five files from
the tracker repository of moricef (`lib/native_aprs/src/`, branch `devel`) or from moricef's iGate repository (branch `rxt`,
`src/` and `include/`), into a folder `native/`:

    native_aprs.cpp  native_aprs.h  native_aprs_huffman_table.h  rxt_protocol.cpp  rxt_protocol.h

Then, from this folder, with the codec sources from the iGate branch (`<codec src dir>` below is its
`lib/AprsCompress/src`):

    g++ -std=c++17 -O2 -I native -I <codec src dir> compare_native.cpp native/native_aprs.cpp native/rxt_protocol.cpp \
        <codec src dir>/AprsCompress.cpp <codec src dir>/AprsCompressModel.cpp <codec src dir>/AprsCompressStore.cpp -o compare_native

## Run

    python3 to_tnc2.py < aprs_is_capture.txt | ./compare_native

Use packets neither format was designed on. A read-only APRS-IS login with a filter such as `u/APL*` gives live LoRa
APRS traffic; do not publish the capture itself, because it contains other people's callsigns and positions.
