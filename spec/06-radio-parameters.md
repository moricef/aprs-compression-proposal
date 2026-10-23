# 06 - Advertising radio parameters

**Status: idea, not specified or implemented. The syntax below is a proposal, not a decision.**

A packet on APRS-IS says nothing about how it was sent or heard. On LoRa that matters more than on
VHF, because the same band hosts several incompatible settings: networks I have seen data for run
SF7 in one region and SF12 in another, and a receiver set to the wrong spreading factor hears
nothing. A station should be able to say what it transmits and listens on.

## What to advertise

For LoRa: frequency, spreading factor, bandwidth, coding rate, transmit power.
For VHF/UHF: frequency, modulation, baud rate (300 / 1200 / 9600), transmit power.

Transmit and receive can differ. Richard Guzman's iGate firmware has separate `rxFreq`/`txFreq`
and `rxSpreadingFactor`/`txSpreadingFactor` settings (read in `configuration.cpp`), so a gateway
may listen on one channel and answer on another. A station that advertises only one set misleads
trackers about where to transmit. So the field carries RX and TX separately, and a single set means
both are the same.

## Why it is needed

* **Finding the right channel.** A new tracker or a traveller can learn the local network's
  settings from one beacon instead of guessing.
* **Reach and link budget (05).** Range depends on spreading factor, bandwidth and power. A
  receiver cannot turn RSSI and SNR into a reach estimate without them.
* **Capability switching (02).** A tracker deciding whether to use compressed frames or change its
  spreading factor needs to know what the nearby gateway actually listens on.
* **Airtime accounting (03).** Airtime per packet depends on SF and bandwidth, so an airtime
  budget needs them.
* **APRS-IS loses it.** When a gateway forwards a packet to the internet, the frequency and
  spreading factor it was heard on are gone. An iGate that publishes its own receive settings once
  in its status lets maps and tools label coverage by network.

## Prior art

APRS already has a frequency convention in comments (a frequency such as `146.520MHz`, optionally
followed by tone and offset). I am citing that from memory of the APRS spec and have not rechecked
it. It carries frequency but not spreading factor, bandwidth, or baud rate, and LoRa trackers
rarely use it.

## Cost, and how to keep it small

Written as plain text, a full set is about 30-35 bytes, for example:

    RF:433.775/SF12/BW125/CR5/P20

That is as large as a position fix. Three ways to keep it cheap:

* **Send it rarely**, in a status or capability beacon, not in every position packet.
* **Let the compressed frames (01) carry it.** The strings are repetitive and the model can learn
  them if training data includes them.
* **A binary form**: a 2-byte word, plus 3 bytes of frequency only when no preset fits. Proposed
  below.

## Binary form (proposal)

One 16-bit word, sent most significant byte first. It carries the station's role and compression
support (02) as well as its radio settings, so one announcement answers both "can you decode me"
and "where do I find you".

    bits    LoRa                              VHF/UHF (AFSK/FSK)
    15-14   role                              role
    13-12   compression                       compression
    11-9    SF: 0-7 = SF5..SF12               reserved, send 0
    8-7     BW: 62.5 / 125 / 250 / 500 kHz    baud: 300 / 1200 / 9600 / reserved
    6-5     CR: 4/5 / 4/6 / 4/7 / 4/8         framing: AX.25 / FX.25 / IL2P / reserved
    4-0     preset                            preset

**Role** (bits 15-14): 0 = transmit only (a tracker that does not receive), 1 = listens (a tracker or
client that receives), 2 = digipeater, 3 = iGate.

**Compression** (bits 13-12): the highest model version from 01 that the station decodes. 0 = none,
1-3 = model v1-v3. Whether a station also *sends* compressed frames is not carried; a receiver
sees that by hearing one.

**Preset** (bits 4-0) names the channel, and the preset decides which column above applies:

    0       the channel this announcement was heard on (family = how it was received)
    1       144.390 MHz, AFSK. Fixed; never reassigned.
    2-29    table, to be defined (see open questions)
    30      explicit frequency follows, LoRa
    31      explicit frequency follows, AFSK/FSK

**Explicit frequency** (presets 30 and 31 only): 3 bytes, unsigned, most significant first, in
units of 500 Hz. That reaches 8.39 GHz and represents half-kHz channels such as 439.9125 MHz.

Two escapes rather than one, so a receiver always knows whether to read bits 11-5 as SF/BW/CR or as
baud/framing, even when the frequency is not in the table. On VHF the coding-rate bits carry the
framing instead, because a receiver cannot decode FX.25 or IL2P without knowing to expect it.

Preset 0 is only meaningful over RF. An iGate gating an announcement to APRS-IS, or a station whose
announcement is likely to be read there, should use a numbered preset or an explicit frequency.

SF5 and SF6 are included because SX126x-class radios (for example the STM32WLE5) support them, even
though SX127x radios use SF6 only in implicit-header mode and current LoRa APRS uses SF7-SF12.

### Examples

    1200-baud AX.25 iGate on 144.390                  C0 81              (2 bytes)
    listening tracker, decodes v1, SF7/125/4:5,
        same channel as heard                         54 80              (2 bytes)
    iGate, decodes v1, SF12/125/4:5 on 433.775 MHz    DE 9E 0D 3C DE     (5 bytes)
    9600-baud AX.25 iGate on 445.925 MHz              C1 1F 0D 9B CA     (5 bytes)

### Where it is carried

* **LoRa, binary.** A frame type of its own: `3C FF` + a type byte + the word (+ frequency). The type
  byte cannot be 0x01 (plain LoRa APRS) or 0x80-0xFF (compressed frames, 01). 0x02 is the obvious
  candidate. Not yet checked against what other LoRa APRS firmware does with third bytes other than
  0x01. Legacy receivers that check for exactly `3C FF 01` drop it, which is fine: they could not
  use it anyway.
* **APRS text** (VHF, APRS-IS, and plain LoRa frames). Raw bytes do not survive APRS-IS, so the word
  is written as printable characters, for example 4 hex digits in a station-capabilities (`<`)
  packet: `<IGATE,RF=DE9E0D3CDE`. Token name, and hex versus base-91 (3 characters for the word), are
  undecided.

## Open questions

* Where the field lives so existing software ignores it (comment, status, new frame type). The
  binary form above proposes a frame type for LoRa and a `<` token for text.
* The preset table (2-29): which channels, who maintains it, and how it is versioned. A preset can
  also name an RX/TX pair for gateways that listen and answer on different channels.
* RX and TX that differ and are not in the table: send the word twice (TX first, then RX)?
* Transmit power is not in the word. Reach (05) needs it; an optional extra byte is one option.
* Two bits of model version run out at v3. Either models change rarely, or 3 means "v3 or later,
  see my status".
* Whether a digipeater advertises on behalf of the gateway it feeds.
* How to express more than one channel (some gateways listen on several spreading factors).
* How much to trust a claimed value; a station can lie, so a heard-on-this-channel measurement is
  better evidence than a claim.
