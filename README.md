# LoRa APRS ideas (working name)

Ideas, and a draft spec with a reference codec, for making LoRa APRS a little smarter: carrying the
same APRS packets in fewer bytes, and borrowing neighbour-discovery and routing ideas from
Meshtastic and MeshCore. It extends APRS; it does not replace it.

**This is a draft by one person. It is not a standard, and the on-air format has not been tested over
the air.** Licensed under the [MIT License](LICENSE).

## Parts

| Part | Status |
|---|---|
| [01 Compressed frames (APRS-PPM)](spec/01-compressed-frames.md) | Spec + C reference codec + frozen model v1 + test vectors. Host-tested only |
| [02 Capability announcement](spec/02-capability-announcement.md) | Idea |
| [03 Implicit acks and smarter digipeating](spec/03-implicit-ack-and-digipeating.md) | Ideas for recommended practice, no format change |
| [04 Message path learning](spec/04-message-path-learning.md) | Idea |
| [05 Reach-aware repeating](spec/05-reach-aware-repeating.md) | Idea: measured reach, not height, decides who repeats |
| [06 Advertising radio parameters](spec/06-radio-parameters.md) | Idea: frequency, SF/BW/CR (LoRa) or baud rate (VHF), TX and RX separately |
| [Rationale and measurements](docs/rationale.md) | Numbers behind the above |

## Layout

    spec/        the drafts
    reference/   plain-C codec (integer-only, C99)
    model/       frozen model v1 table + SHA-256
    tools/       train_model.py (retrain from an APRS-IS capture)
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

* Choose the project name.
* Test over the air on two radios.
* Tell the maintainers of the firmwares this builds on before announcing anything.
