# What a stock decoder does with compressed VHF frames (no radio)

Tests the "would it break legacy" question without a radio. `tools/vhf_frame_sim.py` builds 1200-baud
AFSK audio of five frame variants from six fabricated packets (`test/synthetic_packets.txt`), and an
unmodified Direwolf decoder (`atest`) listens to the WAV files.

    make build/ppm_tool
    python3 tools/vhf_frame_sim.py --ppm build/ppm_tool --packets test/synthetic_packets.txt --outdir /tmp/vhf
    atest -B 1200 /tmp/vhf/plain.wav        # and the other four

The tool builds frames only; it does not decode compressed audio. The compressed bytes come from the
reference codec, which is round-trip tested separately.

## Variants and what stock Direwolf (atest) did

| Variant | Frame | Packets decoded (of 6) | What it showed |
|---|---|---|---|
| plain | ordinary AX.25 UI frame (control) | 6 | normal APRS text |
| infopid | normal addresses, PID 0xF1, info = version, length, coded bytes | 6 | the callsigns and path, then binary junk as the info |
| hybrid | source and destination only, PID 0xF1, compressed path and info | 6 | the callsigns, then junk |
| whole | no AX.25 addresses, valid standard FCS | 6 | junk everywhere, including the source field |
| wholefcs | as whole, but a different CRC | **0** | nothing: the frame is silently dropped |

## Reading it

* A frame with a valid standard FCS is accepted by the TNC layer and handed on, even when its content
  is binary. In `atest` it appears as a packet of junk. So **a legacy decoder does show junk** for the
  first three compressed variants.
* **A different FCS makes the frame invisible** to the legacy decoder, which matches the earlier
  reasoning that a legacy TNC drops bad-checksum frames without comment. No junk, no logged packet.
* Direwolf's IGate states in `igate.c` that its caller has already verified the packet is APRS
  (control 3, PID 0xF0) and that it has a correct CRC. By that comment, frames with PID 0xF1 or a
  failed FCS should not be gated to APRS-IS. This was read from the comment, not tested by running
  the iGate, and the caller's check was not read.
* The whole-packet frame with a standard FCS parses as a frame with garbage addresses. Whether the
  iGate would pass that on depends on the same check and was not tested.

## The real Direwolf, with its iGate and a KISS client

`tools/direwolf_legacy_harness.py` feeds the same audio to a running Direwolf (not `atest`) set up as
a plain iGate: channel 0, MYCALL `TEST-9`, a made-up passcode, `IGSERVER 127.0.0.1`. The script plays
the APRS-IS server on localhost, so no real callsign, passcode or internet server is involved, and
it also connects a KISS TCP client. Direwolf waits about 10 s after login before sending to the
server, so the audio is fed after that.

| Variant | Logged by Direwolf | Received by KISS client | Sent to the (dummy) APRS-IS |
|---|---|---|---|
| plain | 6 | 6 | **5** (the sixth has `TCPIP` in its path and is correctly not gated) |
| infopid | 6, with binary junk as the info | 6 | **0** |
| hybrid | 6, junk | 6 | **0** |
| whole (standard FCS) | 6, shown as `(Not AX.25)` plus junk | 6 | **0** |
| wholefcs (different CRC) | **0** | **0** | **0** |

* The plain frames are gated, which shows the setup works. None of the compressed variants was sent
  on to APRS-IS, so a Direwolf iGate does not put them on the internet. That matches the check
  Direwolf makes (control 3, PID 0xF0, correct CRC).
* A running Direwolf does log the junk for the first three compressed variants, and it hands those
  frames to KISS clients. An application on the other end of KISS (for example APRSdroid) would
  receive them, and what it does with them was not tested.
* A different CRC makes the frame invisible at every level: not logged, not passed to KISS, not gated.

Why the first control run showed zero gated frames: Direwolf waits seven seconds after sending its
login before it starts sending packets to the server. The earlier runs fed audio too soon.

## From source code: the Mobilinkd TNC3 firmware (a hardware KISS TNC)

Not a test run, a reading of the open-source firmware (`mobilinkd/tnc3-firmware`, last commit
2023-04-17, version for TNC3 hardware 2.1.1). The files are `TNC/HdlcDecoder.cpp`, `HdlcFrame.hpp`
and `Digipeater.hpp`. What they show:

* The receive path checks the frame's standard 16-bit FCS (`parse_fcs()`) and reports a CRC error for a
  mismatch. A frame with a good FCS is passed on to the KISS side if it is longer than 2 bytes. I found
  no check of the PID or control field. So a frame with PID 0xF1, or a whole-packet frame, with a
  **standard FCS reaches the host unchanged**, and one with a **different FCS is dropped** by the TNC.
  This matches the reasoning in spec 07 about KISS TNCs.
* The TNC3 has no FX.25 or IL2P code that I could find (no match for either in the source). I did not
  work out what its decoder does with an FX.25 frame.
* The digipeater in this version is a stub: `can_repeat()` returns `nullptr`, so the firmware does not
  digipeat. Its comment describes the intended rules (a UI frame matching a digipeater alias), but
  they are not implemented.

Other TNCs: closed-source TNCs (Kantronics, Kenwood radio TNCs, OpenTracker, Byonics) cannot be read.
The NinoTNC firmware repository I found (`M0LTE/ninotnc-firmwares`) was not examined.

## Limits

* One decoder (Direwolf `atest` and the full `direwolf`, built from a branch whose AFSK decode path is unchanged from
  upstream), clean synthetic audio, six packets. Not tested: aprx, APRSdroid, hardware TNCs, the
  real iGate path, noisy or weak audio, or a real radio.
* The audio generator is simple (phase-continuous AFSK, 40 flags of preamble) and is not a model of
  any particular radio.
