// Compares the "native" LoRa APRS frame of moricef (draft 0) with APRS-PPM on the same packets.
// Input on stdin: one over-the-air TNC2 packet per line (see to_tnc2.py).  Output: mean bytes per packet and the
// saving against the plain frame (text plus the 3-byte header), by packet kind.
// Build: see README.md (it needs the native_aprs sources from moricef's repository, which are GPL-3.0 and are not
// copied here).
#include "native_aprs.h"
#include "AprsCompress.h"
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>
struct Ref { std::string text; int sinceFull; };
struct Acc {
    long n = 0, repeat = 0, natNoEnc = 0, natBad = 0, natBigger = 0, pBetter = 0, primedUsed = 0;
    double plain = 0, nat = 0, full = 0, alt = 0;
};
static std::string kindOf(const std::string& t) {
    size_t c = t.find(':');
    if (c == std::string::npos || c + 1 >= t.size()) return "other";
    char k = t[c + 1];
    if (k == '!' || k == '=' || k == '/' || k == '@') return "position";
    if (k == '`' || k == '\'') return "mic-e";
    if (k == ':') return "message";
    if (k == '>') return "status";
    return "other";
}
int main() {
    std::map<std::string, Ref> refs;
    std::map<std::string, Acc> by;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t gt = line.find('>'), colon = line.find(':');
        if (gt == std::string::npos || colon == std::string::npos || gt > 9 || colon < gt || line.size() > 252) continue;
        std::string src = line.substr(0, gt), kind = kindOf(line);
        size_t plain = line.size() + 3;
        Acc& a = by[kind]; Acc& all = by["ALL"];
        const uint8_t* t = reinterpret_cast<const uint8_t*>(line.data());
        size_t tl = line.size();

        // moricef native frame
        std::vector<uint8_t> out; std::vector<NATIVE_APRS::RxtTuple> rx;
        size_t nat = plain; bool encOk = NATIVE_APRS::encode(line, rx, out) && out.size() <= 255;
        bool bad = false, bigger = false;
        if (encOk) {
            std::string back; std::vector<NATIVE_APRS::RxtTuple> rb;
            if (!NATIVE_APRS::decode(out.data(), out.size(), back, rb) || back != line) bad = true;
            nat = out.size(); if (nat > plain) bigger = true;
        }
        // APRS-PPM: full frame, and the deployed full/primed alternation (compressFullEvery 2)
        uint8_t full[256], primed[256];
        size_t nf = AprsCompress::encodeFrame(t, tl, full, sizeof full);
        size_t fullSize = nf ? nf : plain, altSize;
        auto it = refs.find(src); bool hadRef = it != refs.end();
        size_t np = 0;
        if (hadRef && it->second.sinceFull + 1 < 2)
            np = AprsCompress::encodePrimed(reinterpret_cast<const uint8_t*>(it->second.text.data()), it->second.text.size(), t, tl, primed, sizeof primed);
        bool usedPrimed = false;
        if (np > 0 && (nf == 0 || np < nf)) { altSize = np; it->second.sinceFull++; usedPrimed = true; }
        else { altSize = nf ? nf : plain; refs[src] = Ref{line, 0}; }

        for (Acc* p : {&a, &all}) {
            p->n++; p->plain += plain; p->nat += nat; p->full += fullSize; p->alt += altSize;
            if (hadRef) p->repeat++;
            if (!encOk) p->natNoEnc++;
            if (bad) p->natBad++;
            if (bigger) p->natBigger++;
            if (usedPrimed) p->primedUsed++;
        }
    }
    printf("%-9s %6s %6s | %7s %7s %7s %7s | %6s %6s %6s | %s\n", "kind", "n", "repeat", "plain", "native", "ppm-1st", "ppm-alt", "nat%", "full%", "alt%", "notes");
    for (const char* k : {"position", "mic-e", "message", "status", "other", "ALL"}) {
        auto f = by.find(k); if (f == by.end()) continue; const Acc& a = f->second;
        double n = a.n;
        printf("%-9s %6ld %6ld | %7.1f %7.1f %7.1f %7.1f | %5.0f%% %5.0f%% %5.0f%% | native not encodable %ld, round-trip bad %ld, native>plain %ld; primed used %ld\n",
               k, a.n, a.repeat, a.plain / n, a.nat / n, a.full / n, a.alt / n,
               100 * (1 - a.nat / a.plain), 100 * (1 - a.full / a.plain), 100 * (1 - a.alt / a.plain), a.natNoEnc, a.natBad, a.natBigger, a.primedUsed);
    }
}
