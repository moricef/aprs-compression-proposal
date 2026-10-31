#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "AprsCompress.h"
namespace AprsCompress {
    size_t encodeCanon(const uint8_t*, size_t, uint8_t*, size_t, bool);
    size_t encodeIndep(const uint8_t*, size_t, uint8_t*, size_t);
    size_t decodeCanon(const uint8_t*, size_t, size_t, size_t, uint8_t*, size_t, bool, size_t*);
}
static uint64_t rs = 88172645463325252ULL;
static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
int main(int argc, char** argv) {
    std::vector<std::string> texts; char line[400]; FILE* f = fopen(argv[1], "rb");
    while (fgets(line, sizeof line, f)) { std::string s(line); while (!s.empty() && s.back() == '\n') s.pop_back(); if (s.size() >= 20) texts.push_back(s); }
    fclose(f);
    long n = 0, sc = 0, sk = 0, si = 0, indep_ok = 0, indep_n = 0, tails_ok = 0, tails_n = 0; std::vector<long> flush(5, 0);
    long cut_acc[9] = {0}, cut_ok[9] = {0}, cut_n[9] = {0}; const int ks[9] = { 0, 1, 2, 3, 4, 6, 8, 12, 16 };
    for (auto& t : texts) {
        const uint8_t* tx = (const uint8_t*)t.data(); uint8_t cur[300], can[300], ind[300], out[300], re[300];
        size_t cl = AprsCompress::encodeFrame(tx, t.size(), cur, sizeof cur), kl = AprsCompress::encodeCanon(tx, t.size(), can, sizeof can, false), il = AprsCompress::encodeIndep(tx, t.size(), ind, sizeof ind);
        if (!cl || !kl || !il) continue;
        n++; sc += cl; sk += kl; si += il;
        // independence: decode with any bytes following the payload
        size_t need = 0; indep_n++; if (AprsCompress::decodeCanon(ind, il, 4, il - 4, out, 255, false, &need) == t.size() && !memcmp(out, tx, t.size())) indep_ok++;
        for (int rep = 0; rep < 6; rep++) { uint8_t x[320]; memcpy(x, ind, il); size_t extra = 1 + rnd() % 8; for (size_t i = 0; i < extra; i++) x[il + i] = (uint8_t)rnd();
            tails_n++; if (AprsCompress::decodeCanon(x, il + extra, 4, il - 4 + extra, out, 255, false, &need) == t.size() && !memcmp(out, tx, t.size())) tails_ok++; }
        // check C with this ending: decode, re-encode (same ending), compare with what was received; frame cut by k bytes, no tail
        for (int i = 0; i < 9; i++) { if ((long)il - ks[i] < 5) continue; size_t avail = il - ks[i] - 4; cut_n[i]++;
            size_t r = AprsCompress::decodeCanon(ind, il - ks[i], 4, avail, out, 255, false, &need);
            if (r) { size_t rl = AprsCompress::encodeIndep(out, r, re, sizeof re); if (rl == 4 + avail && !memcmp(re + 4, ind + 4, avail)) { if (r == t.size() && !memcmp(out, tx, r)) cut_ok[i]++; else cut_acc[i]++; } } }
    }
    printf("%ld real packet texts.  Average full-frame bytes:\n   current encoder (shortest ending, trailing zeros stripped) %.1f\n   always 4 closing bytes (what I tested before)            %.1f  (%+.1f)\n   fewest bytes that still decode correctly after ANY following bytes %.1f  (%+.1f)\n", n, (double)sc / n, (double)sk / n, (double)(sk - sc) / n, (double)si / n, (double)(si - sc) / n);
    printf("Independence: intact frame with nothing after it decodes right: %ld of %ld; with 1 to 8 random bytes after it: %ld of %ld.\n", indep_ok, indep_n, tails_ok, tails_n);
    printf("\nCheck C (decode, re-encode with the same ending, compare), no tail, frame cut by k bytes:\n");
    for (int i = 0; i < 9; i++) printf("   %s%-2d: wrongly accepted %ld of %ld (%.3f%%); accepted with correct text %ld; rejected %.2f%%\n", ks[i] ? "cut " : "cut ", ks[i], cut_acc[i], cut_n[i], 100.0 * cut_acc[i] / cut_n[i], cut_ok[i], 100.0 * (cut_n[i] - cut_acc[i] - cut_ok[i]) / cut_n[i]);
}
