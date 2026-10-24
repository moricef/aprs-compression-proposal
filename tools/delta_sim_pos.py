"""Position parser shared by delta_sim.py and receiver_sim.py."""
import re

POS_UNC = re.compile(rb"(\d{2})(\d{2}\.\d{2})([NS]).(\d{3})(\d{2}\.\d{2})([EW])")

def position(info):
    """(lat, lon) from a plain or compressed position report, else None (Mic-E, objects, status...)."""
    dti = info[:1]
    if dti in (b"!", b"="): body = info[1:]
    elif dti in (b"/", b"@"): body = info[8:]
    else: return None
    if body[:1].isdigit():
        m = POS_UNC.match(body)
        if not m: return None
        lat = int(m.group(1)) + float(m.group(2)) / 60; lon = int(m.group(4)) + float(m.group(5)) / 60
        return (-lat if m.group(3) == b"S" else lat, -lon if m.group(6) == b"W" else lon)
    if len(body) < 9: return None
    v = lambda c: sum((ch - 33) * 91 ** (3 - i) for i, ch in enumerate(c))
    return (90 - v(body[1:5]) / 380926.0, -180 + v(body[5:9]) / 190463.0)

