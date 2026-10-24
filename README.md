# Proposals for extending APRS: compressed frames and capability announcement

**Phase 1 of bringing APRS up to date:** proposal drafts for carrying the same APRS packets in fewer
bytes (APRS-PPM compressed frames), a way for stations to learn what the stations around them can
decode, and a binding for VHF/AX.25. It extends APRS; it does not replace it, and legacy stations
keep working. Phase 2, the harder "smarts" work (smarter digipeating, path learning, reach-aware
repeating, a measured pilot), is in the companion repo `aprs-smarts`.

**This is a draft by one person. It is not a standard, and the on-air format has not been tested over
the air.** Licensed under the [MIT License](LICENSE).

## Parts

| Part | Status |
|---|---|
| [01 Compressed frames (APRS-PPM)](spec/01-compressed-frames.md) | Spec + C reference codec + frozen model v1 + test vectors. Host-tested only |
| [02 Capability announcement](spec/02-capability-announcement.md) | Proposal draft: how a sender learns a station can decode compressed frames |
| [07 AX.25 binding (VHF)](spec/07-ax25-binding.md) | Proposal draft: carry the same compressed payload in AX.25 info fields; smaller gain, marker undecided |
| [08 Priming with a station's previous packet](spec/08-priming-from-previous-packet.md) | Idea + simulation: about 62-66% saved on VHF whole-packet frames, against 41% for the static table |
| [Direwolf notes](docs/direwolf-notes.md) | Planning notes for a Direwolf implementation |
| [How the compression works and how it was trained](docs/how-it-works.md) | Plain-language explanation, comparison with other methods, training method and caveats |
| [Compression by country (rough estimate)](docs/per-country-estimate.md) | Estimate per country and device type, LoRa and VHF, from one hour of capture; to be replaced by the 8-hour run |
| [Legacy decoder test (no radio)](docs/legacy-decoder-test.md) | What stock Direwolf does with compressed VHF frames, using generated audio |
| [Proposed implementations and how much they cut](docs/proposed-implementations.md) | One table: LoRa and VHF, bytes and airtime, with status |
| [Rationale and measurements](docs/rationale.md) | Numbers behind the above |

## Layout

    spec/        the drafts
    reference/   plain-C codec (integer-only, C99)
    model/       frozen model v1 table + SHA-256
    tools/       train_model.py (retrain), capture_aprsis.py (read-only APRS-IS capture)
    test/        test vectors (fabricated packets) and the tool that checks them
    docs/        measurements, notes

## Try it

    make test                     # checks the reference codec against test/vectors_v1.txt
    build/ppm_tool stats < packets.txt

## Implementations

* Firmware fork of richonguzman/LoRa_APRS_iGate with receive-always, opt-in transmit
  (`lora.compressTx`): local branch `feature/order3-compression`, builds for `ttgo-lora32-v21`.
  Its C++ codec produces byte-identical frames to the reference on 4,520 vectors.
* Nothing else yet. A Direwolf port and an STM32WLE5 tracker build are candidates.

## TODO

* Reframed as proposal drafts. Any project or extension name is left to whoever adopts it; no branding chosen.
* Test over the air on two radios.
* Once it works over the air, bring the proposal to the APRS Foundation (contact form / aprssig) and open a PR to richonguzman/LoRa_APRS_iGate that links here for the format details.
