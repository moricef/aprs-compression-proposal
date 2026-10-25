#!/usr/bin/env python3
"""Turns APRS-IS lines ("SRC>DEST,PATH,qAC,SERVER:payload") into the packet text a LoRa station would send
("SRC>DEST,PATH:payload"): everything from the q-construct (or TCPIP*) on is dropped.  Reads stdin, writes stdout."""
import re, sys
out = sys.stdout.buffer
for raw in sys.stdin.buffer:
    raw = raw.rstrip(b"\r\n")
    if raw.startswith(b"#"):
        continue
    c = raw.find(b":")
    if c < 0 or b">" not in raw[:c]:
        continue
    parts = raw[:c].split(b",")
    keep = []
    for p in parts[1:]:
        if re.match(rb"q[A-Z]{2}$", p) or p.startswith(b"TCPIP") or p.startswith(b"TCPXX"):
            break
        keep.append(p)
    out.write(b",".join([parts[0]] + keep) + b":" + raw[c + 1:] + b"\n")
