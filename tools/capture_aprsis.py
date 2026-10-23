#!/usr/bin/env python3
"""Capture raw packets from APRS-IS with a read-only login, for training or testing the model.

  capture_aprsis.py --call YOURCALL --filter 'u/APL*' --seconds 600 --out lora.txt
  capture_aprsis.py --call YOURCALL --filter 'r/37.0/-121.6/250' --seconds 600 --out vhf.txt

The login uses passcode -1, which APRS-IS treats as receive-only: nothing you send is accepted.
Use your own callsign. Be polite: short captures, one connection. The output contains real
callsigns and positions: do not publish it.

Filters used for model v1 (see docs/how-it-works.md): LoRa traffic was selected by software device
ID (tocall prefix APL*, APLRT*, APLRG*, APLT*); VHF traffic by radius, keeping only packets that an
iGate heard over the air (q-construct qAR or qAO) when training.
"""
import argparse, socket, sys, time

ap = argparse.ArgumentParser()
ap.add_argument("--call", required=True, help="your callsign (read-only login)")
ap.add_argument("--filter", required=True, help="APRS-IS filter string")
ap.add_argument("--seconds", type=int, default=600)
ap.add_argument("--out", required=True)
ap.add_argument("--server", default="rotate.aprs2.net")
a = ap.parse_args()

s = socket.create_connection((a.server, 14580), timeout=20)
s.recv(200)
s.sendall(f"user {a.call} pass -1 vers ppm-capture 0.1 filter {a.filter}\r\n".encode())
s.settimeout(10)
end = time.time() + a.seconds
with open(a.out, "wb") as f:
    while time.time() < end:
        try:
            d = s.recv(65536)
        except socket.timeout:
            continue
        if not d:
            break
        f.write(d)
print(f"wrote {a.out}", file=sys.stderr)
