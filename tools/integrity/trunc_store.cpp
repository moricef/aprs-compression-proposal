#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "AprsCompress.h"
#include "AprsCompressStore.h"
int main(int argc, char** argv) {
    std::vector<std::string> texts; char line[400]; FILE* f = fopen(argv[1], "rb");
    while (fgets(line, sizeof line, f)) { std::string s(line); while (!s.empty() && s.back() == '\n') s.pop_back(); if (s.size() >= 20) texts.push_back(s); }
    fclose(f);
    long n = 0, found = 0, misdecoded = 0, accepted_wrong = 0, dropped = 0;
    for (auto& t : texts) {
        const uint8_t* tx = (const uint8_t*)t.data(); uint8_t fr[300], pf[300], out[300];
        size_t fl = AprsCompress::encodeFrame(tx, t.size(), fr, sizeof fr), pl = AprsCompress::encodePrimed(tx, t.size(), tx, t.size(), pf, sizeof pf);
        if (!fl || !pl) continue;
        AprsCompress::PacketStore rx;
        size_t r = AprsCompress::decodeFrame(fr, fl - 2, out, 255);       // the full frame arrives 2 bytes short and is accepted
        if (!r) continue;
        rx.put(out, r, true); n++;                                         // the firmware stores what it decoded as the reference
        uint8_t id = 0, check = 0; AprsCompress::primedHeader(pf, pl, id, check);
        AprsCompress::PacketStore::Entry* ref = rx.findForFrame(id, check);   // the next, intact primed frame (made against the true text)
        if (!ref) { dropped++; continue; }
        found++;
        size_t m = AprsCompress::decodePrimed(ref->text, ref->len, pf, pl, out, 255);
        if (m == 0) dropped++; else if (m == t.size() && !memcmp(out, tx, m)) ; else accepted_wrong++;
    }
    printf("after a full frame was accepted 2 bytes short (%ld cases), the NEXT intact primed frame from the same station:\n  dropped (no matching reference, or did not decode): %ld (%.2f%%)\n  reference matched by chance: %ld (%.2f%%), of which decoded to wrong text: %ld\n", n, dropped, 100.0 * dropped / n, found, 100.0 * found / n, accepted_wrong);
}
