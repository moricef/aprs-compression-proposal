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

## Limits

* One decoder (Direwolf `atest`, built from a branch whose AFSK decode path is unchanged from
  upstream), clean synthetic audio, six packets. Not tested: aprx, APRSdroid, hardware TNCs, the
  real iGate path, noisy or weak audio, or a real radio.
* The audio generator is simple (phase-continuous AFSK, 40 flags of preamble) and is not a model of
  any particular radio.
