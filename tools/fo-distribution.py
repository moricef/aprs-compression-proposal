# Read-only: how large are the frequency offsets the collector has recorded?  ssh droplet 'python3 -' < fo-distribution.py
# Uses receptions that were decoded (outcome ok) and carry fo_hz. Opens the database read-only.
import os, sqlite3, datetime as dt, statistics as st
DB = os.environ.get("DB", "/var/lib/lora-collector/events.db")
db = sqlite3.connect("file:%s?mode=ro" % DB, uri=True)
rows = db.execute("SELECT receiver, source, freq_hz, fo_hz, time_ms FROM receptions WHERE outcome='ok' AND fo_hz IS NOT NULL AND freq_hz IS NOT NULL AND freq_hz > 0").fetchall()
def pct(v, p):
    v = sorted(v); k = (len(v) - 1) * p / 100.0; lo = int(k); hi = min(lo + 1, len(v) - 1)
    return v[lo] + (v[hi] - v[lo]) * (k - lo)
def line(label, v, unit="Hz"):
    print("  %-22s n=%-7d median %7.0f  p90 %7.0f  p95 %7.0f  p99 %7.0f  p99.9 %7.0f  max %7.0f %s" % (label, len(v), pct(v, 50), pct(v, 90), pct(v, 95), pct(v, 99), pct(v, 99.9), max(v), unit))
if not rows:
    raise SystemExit("no rows")
t0 = min(r[4] for r in rows) / 1000; t1 = max(r[4] for r in rows) / 1000
f = lambda t: dt.datetime.fromtimestamp(t, dt.timezone.utc).strftime("%Y-%m-%d")
print("== %d decoded receptions with a frequency offset, %s to %s (UTC)" % (len(rows), f(t0), f(t1)))
bands = {}
for rc, src, fq, fo, t in rows:
    bands.setdefault(int(round(fq / 1e6)), []).append(abs(fo))
print("\n== |offset| by band (the radio's reading, in Hz)")
for b in sorted(bands): line("%d MHz" % b, bands[b])
print("\n== share of receptions with |offset| beyond a limit (all bands)")
a = [abs(r[3]) for r in rows]
for lim in (1000, 2500, 3500, 5000, 7500, 10000, 12000, 15000, 20000):
    print("  beyond %6d Hz: %6.2f%%  (%d of %d)" % (lim, 100.0 * sum(1 for x in a if x > lim) / len(a), sum(1 for x in a if x > lim), len(a)))
print("\n== |offset| in ppm of the carrier (all bands)")
ppm = [abs(r[3]) / r[2] * 1e6 for r in rows]
line("ppm", ppm, "ppm")
# how much of the offset is a steady bias of one sender-receiver pair (crystals) and how much scatters
pairs = {}
for rc, src, fq, fo, t in rows:
    if src: pairs.setdefault((src, rc), []).append(fo)
good = {k: v for k, v in pairs.items() if len(v) >= 5}
print("\n== per sender and receiver pair, with at least 5 receptions: %d pairs" % len(good))
if good:
    med = [abs(st.median(v)) for v in good.values()]
    mad = [st.median([abs(x - st.median(v)) for x in v]) for v in good.values()]
    line("|median offset|", med)
    line("scatter (MAD)", mad)
    print("  pairs whose median is beyond 2500 Hz: %d of %d" % (sum(1 for x in med if x > 2500), len(med)))
    print("\n  the 10 largest pair medians (sender -> receiver: median Hz, receptions, scatter Hz):")
    for k, v in sorted(good.items(), key=lambda kv: -abs(st.median(kv[1])))[:10]:
        print("    %s -> %s: %7.0f  n=%-5d  scatter %4.0f" % (k[0], k[1], st.median(v), len(v), st.median([abs(x - st.median(v)) for x in v])))
