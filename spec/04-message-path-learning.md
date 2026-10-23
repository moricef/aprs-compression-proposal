# 04 - Message path learning

**Status: idea, not specified.**

MeshCore floods the first message to discover a route, then sends later messages along the
learned path (one source claims 95-98% less airtime for repeated traffic). Meshtastic 2.6 added
next-hop routing for direct messages, kept compatible with older firmware.

For APRS: use the path of a heard message and its existing message-number ack to learn which
digipeater reaches an addressee, and address the next message to it instead of using a generic
`WIDE1-1`. Falls back to the normal path when the learned one stops working. Open questions: where
the learned path is stored, how it ages, and whether digipeaters need to cooperate.
