# 03 - Implicit acknowledgement and smarter digipeating

**Status: ideas for recommended practice. Needs no change to the on-air format.**

Borrowed from Meshtastic (see `docs/rationale.md`). All of it is local to a station.

1. **SNR-weighted digipeat delay with cancel-on-hear.** Before repeating a packet, wait a random
   delay that is shorter the weaker the packet was received (far stations repeat first). If the
   same packet (sender + payload) is heard repeated by someone else during the wait, cancel.
2. **Implicit ack for beacons.** A tracker that hears its own callsign come back with a
   digipeated mark (`CALL*`) knows the beacon got out; if it does not hear that, it may retry,
   raise power, or lengthen its spreading factor.
3. **Airtime-aware beaconing.** Stretch the beacon interval when the channel is busy; allow an
   airtime budget per station.
4. **Heard-stations table** with RSSI, SNR and a "heard direct" flag, shown to the operator.

What an existing firmware (Richard Guzman's LoRa_APRS_iGate, read 2026-10-23) already does:
channel-activity-detection backoff before transmit, a 25 s duplicate filter, a 50-entry heard table
with RSSI/SNR/heard-direct, and a recently-heard list. It does not do 1 or 2, and I saw no logic for 3.
