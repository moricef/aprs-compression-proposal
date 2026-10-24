#!/usr/bin/env python3
"""Does a real Direwolf APRS digipeater repeat a frame? Feeds WAV files from vhf_frame_sim.py to Direwolf
(callsign TEST-9) with a digipeater from channel 0 to a KISS-TCP channel, and counts the frames it sends
there. This avoids needing a transmit audio device: the "transmitter" is a socket this script listens on.

  mkdir <wavdir>/digi && cp docs/direwolf-digi.conf <wavdir>/digi/conf2
  direwolf_digi_harness.py --direwolf /path/to/direwolf --wavdir <wavdir> plain infopid infof0 hybrid whole

Direwolf truncates a long -c path, so the script changes into <wavdir>/digi and passes conf2.
"""
import socket,subprocess,threading,time,wave,re,sys
import argparse,os
ap=argparse.ArgumentParser()
ap.add_argument("--direwolf",required=True); ap.add_argument("--wavdir",required=True); ap.add_argument("names",nargs="+")
a=ap.parse_args()
DW=os.path.abspath(a.direwolf); D=os.path.abspath(a.wavdir); os.chdir(os.path.join(D,"digi"))
def kiss_frames(buf):
    out=[];cur=None
    for b in buf:
        if b==0xC0:
            if cur: out.append(bytes(cur))
            cur=bytearray()
        elif cur is not None: cur.append(b)
    return out
def unesc(f):
    o=bytearray();i=0
    while i<len(f):
        if f[i]==0xDB and i+1<len(f): o.append(0xC0 if f[i+1]==0xDC else 0xDB); i+=2
        else: o.append(f[i]); i+=1
    return bytes(o)
def addrs(fr):
    calls=[];i=0
    while i+7<=len(fr):
        c="".join(chr(x>>1) for x in fr[i:i+6]).strip(); ssid=(fr[i+6]>>1)&15; hbit=fr[i+6]&0x80
        calls.append(c+("-%d"%ssid if ssid else "")+("*" if hbit else "")); last=fr[i+6]&1; i+=7
        if last: break
    return calls,i
def run(name):
    got=bytearray(); stop=threading.Event()
    srv=socket.socket(); srv.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1); srv.bind(("127.0.0.1",8011)); srv.listen(1); srv.settimeout(1)
    def accept():
        while not stop.is_set():
            try: c,_=srv.accept()
            except socket.timeout: continue
            c.settimeout(1)
            while not stop.is_set():
                try: d=c.recv(4096)
                except socket.timeout: continue
                if not d: break
                got.extend(d)
    threading.Thread(target=accept,daemon=True).start()
    p=subprocess.Popen([DW,"-c","conf2","-r","44100","-n","1","-b","16","-t","0","-"],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    out=bytearray(); threading.Thread(target=lambda:[out.extend(l) for l in iter(p.stdout.readline,b"")],daemon=True).start()
    time.sleep(6)
    w=wave.open(D+f"/{name}.wav","rb"); p.stdin.write(w.readframes(w.getnframes())); p.stdin.flush()
    time.sleep(15); stop.set(); p.terminate()
    try: p.wait(5)
    except Exception: p.kill()
    srv.close()
    txt=re.sub(rb"\x1b\[[0-9;]*[A-Za-z]",b"",bytes(out)).decode("latin1")
    rx=len([l for l in txt.splitlines() if re.match(r"\[0\.\d\]",l)])
    frames=[unesc(f) for f in kiss_frames(got) if len(f)>1]
    print(f"=== {name}: Direwolf received {rx}; frames it sent on the digipeat channel: {len(frames)}")
    for f in frames[:2]:
        a,i=addrs(f[1:]); body=f[1:]
        print("   ", ">".join([a[1],a[0]]) if len(a)>1 else a, "via", a[2:], "control %02x pid %02x"%(body[i],body[i+1]) if len(body)>i+1 else "")
for n in a.names: run(n)
