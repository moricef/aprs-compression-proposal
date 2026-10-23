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
* **A binary form** in a future frame type: frequency in 3 bytes, SF/BW/CR packed in 1-2 bytes,
  power in 1 byte. About 5 bytes.

## Open questions

* Where the field lives so existing software ignores it (comment, status, new frame type).
* Whether a digipeater advertises on behalf of the gateway it feeds.
* How to express more than one channel (some gateways listen on several spreading factors).
* How much to trust a claimed value; a station can lie, so a heard-on-this-channel measurement is
  better evidence than a claim.
