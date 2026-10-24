#!/usr/bin/env python3
"""Run a real Direwolf (not atest) on WAV files made by vhf_frame_sim.py and report, per file: packets
logged, frames a KISS TCP client received, and packet lines sent to a LOCAL DUMMY APRS-IS server.

No real callsign, passcode or internet server is involved: Direwolf is configured as TEST-9 with a
made-up passcode and an iGate server at 127.0.0.1, where this script plays the server. Direwolf waits
about 10 s after logging in before it sends anything, so the script does too.

  vhf_frame_sim.py ... --outdir /tmp/vhf
  mkdir /tmp/vhf/real && cp docs/direwolf-legacy.conf /tmp/vhf/real/conf
  direwolf_legacy_harness.py --direwolf /path/to/direwolf --wavdir /tmp/vhf plain infopid hybrid whole wholefcs

Config note: Direwolf truncates a long -c path, so the script changes into <wavdir>/real and passes
the config as plain `conf`.
"""
import socket,threading,subprocess,sys,time,wave,re,os
import argparse
ap=argparse.ArgumentParser()
ap.add_argument("--direwolf",required=True); ap.add_argument("--wavdir",required=True); ap.add_argument("names",nargs="+")
a=ap.parse_args()
D=os.path.abspath(a.wavdir); DW=os.path.abspath(a.direwolf); os.chdir(os.path.join(D,"real"))
def run(name):
    got_is=[]; kiss=bytearray(); stop=threading.Event()
    srv=socket.socket(); srv.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1); srv.bind(("127.0.0.1",14580)); srv.listen(1); srv.settimeout(1)
    def isserver():
        while not stop.is_set():
            try: c,_=srv.accept()
            except socket.timeout: continue
            c.settimeout(1); c.sendall(b"# dummy-server\r\n")
            buf=b""
            while not stop.is_set():
                try: d=c.recv(4096)
                except socket.timeout: continue
                if not d: break
                buf+=d
                while b"\n" in buf:
                    line,buf=buf.split(b"\n",1); line=line.rstrip(b"\r")
                    got_is.append(line)
                    if line.startswith(b"user "): c.sendall(b"# logresp TEST-9 verified, server DUMMY\r\n")
    t=threading.Thread(target=isserver,daemon=True); t.start()
    p=subprocess.Popen([DW,"-c","conf","-r","44100","-n","1","-b","16","-t","0","-"],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    out=bytearray()
    threading.Thread(target=lambda:[out.extend(l) for l in iter(p.stdout.readline,b"")],daemon=True).start()
    def kisscli():
        for _ in range(40):
            try: s=socket.create_connection(("127.0.0.1",8001),timeout=1); break
            except OSError: time.sleep(0.25)
        else: return
        s.settimeout(1)
        while not stop.is_set():
            try: d=s.recv(4096)
            except socket.timeout: continue
            if not d: break
            kiss.extend(d)
    threading.Thread(target=kisscli,daemon=True).start()
    for _ in range(80):
        if any(l.startswith(b'user ') for l in got_is): break
        time.sleep(0.5)
    time.sleep(10)
    if p.poll() is not None:
        print('direwolf died early, rc',p.poll()); print(re.sub(rb'\x1b\[[0-9;]*[A-Za-z]',b'',bytes(out)).decode('latin1')[-800:]); return
    w=wave.open(f"{D}/{name}.wav","rb"); raw=w.readframes(w.getnframes())
    p.stdin.write(raw); p.stdin.flush()
    time.sleep(9)
    stop.set(); p.terminate()
    try: p.wait(5)
    except Exception: p.kill()
    srv.close()
    txt=re.sub(rb"\x1b\[[0-9;]*[A-Za-z]",b"",bytes(out)).decode("latin1")
    heard=[l for l in txt.splitlines() if l.startswith("[0")]
    nk=bytes(kiss).count(b"\xc0")//2
    gated=[l for l in got_is if not l.startswith(b"user ") and not l.startswith(b"#")]
    safe=lambda b:"".join(chr(c) if 32<=c<127 else "." for c in (b if isinstance(b,bytes) else b.encode("latin1")))
    print(f"=== {name}: logged {len(heard)} packets; KISS client got {nk} frames; sent to dummy APRS-IS: {len(gated)} packet lines")
    for l in heard[:2]: print("   log :",safe(l)[:100])
    for l in gated[:2]: print("   gate:",safe(l)[:100])
for n in a.names: run(n)
