# Questions and answers

Short answers, from what has been measured. The details are in [DETAILS.md](../DETAILS.md) and [the over-the-air test log](over-the-air-test.md).

*Most answers below were written for the earlier whole-text format (`3C FF 81`, `3C FF C1`). The current format, LACF ([spec 11](../spec/11-native-ppm-frame-byte-plan.md)), is in early development and still being reviewed for change; it also leaves plain frames untouched, receives always and transmits only when you choose it.*

## Does an iGate with the PPM code still work on ordinary LoRa APRS packets?

Yes. A plain frame goes through exactly as before; the compressed-frame check only acts on frames that carry the compressed marker. The same iGate also reads compressed frames, so it handles both kinds on one channel.

## What reaches APRS-IS when it hears a compressed frame?

The original packet, as ordinary APRS text. The iGate expands the frame back to the exact text the sender had, and from there it takes the same path as any plain packet. APRS-IS never sees a compressed frame. At our test gateway, a compressed beacon went out to APRS-IS one second after it was heard, as a normal packet with the gateway's callsign in the qAO part of the path.

## What else does an iGate do with a packet it hears: its log server, MQTT, KISS?

In the ESP32 firmware the expanded packet is the only copy there is. APRS-IS, the digipeater, KISS, MQTT, the web page list and the optional syslog server (the log server some iGates push received packets to) all take it from the same place, so they see the original text. That is read from the code. On the air we have watched the APRS-IS path, and not yet the syslog or MQTT output.

## Is the text really identical?

Yes. The compression is lossless: every frame we compared with what was sent expanded to the same text, and the reference codec reproduces 129 worked examples exactly (26 full frames and 103 primed ones, each written down as bytes on the air and the text they must expand to). For LACF the C and Python reference code reproduce 554 such examples and reject 38 invalid frames.

## Do I have to change my tracker or my iGate?

No. Receiving compressed frames is always on in the new code, and sending them is off by default. Nothing changes for a station that does not opt in.

## What happens to a station that does not have the code?

It hears a compressed frame and cannot read it. We tried it with an unmodified Direwolf: it logged the frame as unparsable and carried on, without crashing or passing it to APRS-IS. That is why a compressed frame should only be sent where the receivers that matter have the code.

## What if a frame is lost?

A primed frame depends on the same station's last full frame. If that was missed, the primed frame is dropped and the receiver recovers at the next full frame. That cost 6 primed frames in the 24-hour test, and it is why a full frame goes out at least every second packet.

## Where does this stand?

It is an idea that is showing promise and needs more testing. The earlier format worked on three kinds of hardware over two links; LACF has run on an iGate, a T-Deck and a Dire Wolf station, and both were replayed on tens of thousands of packets. More links, a busier channel, weaker signals and other spreading factors are the next things to try. The branches are there so people can try it.

## How do I try it?

Use spare units, not a live station. For the current format (LACF, spec 11, in early development and still being reviewed for change) there is test firmware for the
LILYGO LoRa32 V2.1 iGate and the LILYGO T-Deck Plus 433 tracker that you flash from a browser: <https://tiles.aivu.com/lacf>. The page explains the settings and warnings.
Receive is always on and transmit is opt-in, so the first step is to listen. For the earlier whole-text format, start with its safe-testing page (below).

## Where is the code?

**The current format (LACF, spec 11):**

* [Test firmware with a browser flasher](https://tiles.aivu.com/lacf) for the LoRa32 iGate and the T-Deck Plus
* [ESP32 iGate firmware](https://github.com/radiohound/LoRa_APRS_iGate-1/tree/feature/lacf) (`feature/lacf`)
* [T-Deck tracker firmware](https://github.com/radiohound/LoRa_APRS_Tracker-moricef/tree/hybrid) (`hybrid`)
* [Direwolf, LoRa HAT channel](https://github.com/radiohound/direwolf/tree/feature/lora-hybrid) (`feature/lora-hybrid`; install notes in `scripts/hill-hybrid/README.txt`)
* The C and Python reference code and the test vectors are in this repository ([`tools/hb`](../tools/hb), [`test/`](../test), `make hb-test`)

**The earlier whole-text format (superseded by LACF):**

* [ESP32 iGate firmware](https://github.com/radiohound/LoRa_APRS_iGate/tree/feature/order3-compression) (`feature/order3-compression`)
* [Direwolf, LoRa HAT channel](https://github.com/radiohound/direwolf/tree/feature/lora-compression) (`feature/lora-compression`)
* [T-Deck tracker firmware](https://github.com/radiohound/LoRa_APRS_Tracker-moricef/tree/ppm-compression) (`ppm-compression`)
* [Safe-testing page for the ESP32 build](https://github.com/radiohound/LoRa_APRS_iGate/blob/feature/order3-compression/tools/aprs_compress/EXPERIMENT.md)
