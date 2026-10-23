# 02 - Capability announcement

**Status: idea, not specified or implemented.**

Compressed frames (01) are invisible to stations that do not implement them. A sender needs a way
to know that the stations it wants to reach can decode them.

Options to work out:

* **Listen-and-switch.** A tracker that also receives switches to compressed frames after it
  hears a compressed frame from an iGate or digipeater (an iGate that transmits the format almost
  certainly decodes it), and reverts to plain after a period without hearing one. Caveats: hidden
  nodes (hearing an iGate does not prove it hears you), several iGates of which only some decode,
  and trackers that cannot afford to receive. Mitigation: send a plain frame every Nth transmission.
* **Explicit marker.** A field in an existing beacon or status packet saying which model versions
  and roles the station supports. Needs a place in the text that existing software ignores.

Open questions: marker syntax; whether a digipeater should advertise on behalf of the iGate it
feeds; how long to remember a capable station.
