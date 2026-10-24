# Proposal draft 02 - Capability announcement

| | |
|---|---|
| Status | **Proposal draft by an individual. Not a standard. Not implemented or tested.** |
| Depends on | 01 Compressed frames |
| Changes to existing APRS | None for the baseline method; the optional explicit marker is open |

## Summary

A sender should use a compressed frame only when it has reason to believe the stations it wants to
reach can decode it, and should otherwise send plain APRS. This part says how a station learns that.

## Motivation

Compressed frames (01) are not valid APRS to a station that does not implement them. Without a way
to learn who can decode them, an upgraded station must either always send plain (no gain) or send
compressed and risk being unheard. Legacy stations, including their two-way messaging, must keep
working, so the default is plain.

## Baseline: listen-and-switch (no new syntax)

A station that can receive switches to compressed frames after it hears a compressed frame from an
iGate or digipeater (one that transmits the format almost certainly decodes it), and reverts to
plain after a period without hearing one. Messages to a station not known to be capable are sent
plain with the ordinary ack. This needs no change to any packet, so it is legacy-safe by construction.

Caveats:

* Hidden nodes: hearing an iGate does not prove it hears you.
* Several iGates, only some of which decode.
* Trackers that cannot afford to receive cannot use it.

Mitigation: send a plain frame every Nth transmission, and fall back to plain if no ack arrives for
a message.

## Optional: explicit marker

A field in an existing beacon or status packet saying which model versions and roles the station
supports. It needs a place in the text that existing software ignores or displays harmlessly, and
that is not yet found; `<` is already an APRS data-type identifier for station capabilities, so
reusing it needs care. Not proposed until a placement is tested against existing software.

## Compatibility with legacy stations

Nothing changes for a station that implements neither method. An upgraded iGate still gateways
compressed frames to APRS-IS as plain text, and sends plain to legacy stations.

## Security and regulatory considerations

None specific beyond those in 01. A capability marker, if added, is public information about the
station.

## Open issues

1. Marker syntax and placement, if any.
2. Whether a digipeater may advertise on behalf of the iGate it feeds.
3. How long to remember a capable station.
4. Radio parameters (frequency, spreading factor, baud rate) would naturally travel with a
   capability announcement; they are drafted separately in the phase 2 repo (`aprs-smarts`,
   `drafts/radio-parameters.md`) because they are a larger change.
5. Reach is a better signal than "I heard it"; see `drafts/reach-aware-repeating.md` in phase 2.
