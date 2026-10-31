#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "AprsCompress.h"
namespace AprsCompress {
    size_t encodeCanon(const uint8_t*, size_t, uint8_t*, size_t, bool);
    size_t decodeCanon(const uint8_t*, size_t, size_t, size_t, uint8_t*, size_t, bool, size_t*);
}
static uint64_t rs = 88172645463325252ULL;
static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
enum { REJ, OKTEXT, WRONG, N };
static void row(const char* name, long c[N]) {
    long n = c[REJ] + c[OKTEXT] + c[WRONG];
    printf("      %-44s rejected %6.2f%%   accepted, text correct %6.2f%%   accepted, WRONG text %7.3f%%  (%ld of %ld)\n", name, 100.0 * c[REJ] / n, 100.0 * c[OKTEXT] / n, 100.0 * c[WRONG] / n, c[WRONG], n);
}
int main(int argc, char** argv) {
    std::vector<std::string> texts; char line[400]; FILE* f = fopen(argv[1], "rb");
    while (fgets(line, sizeof line, f)) { std::string s(line); while (!s.empty() && s.back() == '\n') s.pop_back(); if (s.size() >= 20) texts.push_back(s); }
    fclose(f);
    const int R = 6;                                                        // a relay tail (RXT) of 6 bytes after the payload
    const int ks[9] = { 0, 1, 2, 3, 4, 6, 8, 12, 16 }; const int NK = 9;
    long cur_n = 0, cur_len = 0, can_len = 0, chk_len = 0, intact_rxt_cur = 0, intact_rxt_cur_ok = 0;
    long R0[3][9][N] = {{{0}}}, R1[3][9][N] = {{{0}}};                      // [check A,B,C][k][outcome]  without / with RXT
    long BASE[9][N] = {{0}}, ROUNDERR = 0, used = 0; double t_dec = 0, t_rt = 0;
    for (auto& t : texts) {
        const uint8_t* tx = (const uint8_t*)t.data();
        uint8_t cur[300], a[300], b[300], out[300], re[300];
        size_t cl = AprsCompress::encodeFrame(tx, t.size(), cur, sizeof cur); if (!cl) continue;
        size_t al = AprsCompress::encodeCanon(tx, t.size(), a, sizeof a, false), bl = AprsCompress::encodeCanon(tx, t.size(), b, sizeof b, true);
        if (!al || !bl) continue;
        used++; cur_len += cl; can_len += al; chk_len += bl;
        // the CURRENT format with 6 bytes appended: does it still decode?
        { uint8_t x[300]; memcpy(x, cur, cl); for (int i = 0; i < R; i++) x[cl + i] = (uint8_t)rnd(); size_t r = AprsCompress::decodeFrame(x, cl + R, out, 255); intact_rxt_cur++; if (r == t.size() && !memcmp(out, tx, r)) intact_rxt_cur_ok++; }
        // baseline: the current decoder on a frame cut by k bytes (no RXT)
        for (int i = 0; i < NK; i++) if ((long)cl - ks[i] >= 5) { size_t r = AprsCompress::decodeFrame(cur, cl - ks[i], out, 255); BASE[i][r == 0 ? REJ : (r == t.size() && !memcmp(out, tx, r)) ? OKTEXT : WRONG]++; }
        for (int withRxt = 0; withRxt < 2; withRxt++) {
            size_t rx = withRxt ? R : 0;
            uint8_t fa[320], fb[320]; memcpy(fa, a, al); memcpy(fb, b, bl);
            for (size_t i = 0; i < rx; i++) { uint8_t v = (uint8_t)rnd(); fa[al + i] = v; fb[bl + i] = v; }
            for (int i = 0; i < NK; i++) {
                // The receiver does NOT know where the payload ends: with a tail, the bytes after what the decoder uses are the tail.
                // ---- check A: canonical flush, decoder must end exactly at the payload's last byte (with a tail: it must not NEED more than is there)
                { size_t Lc = al + rx - ks[i]; if (Lc < 5) goto skipA; size_t avail = Lc - 4, need = 0; size_t r = AprsCompress::decodeCanon(fa, Lc, 4, avail, out, 255, false, &need);
                  bool bad = withRxt ? need > avail : need != avail;
                  int o = (r == 0 || bad) ? REJ : (r == t.size() && !memcmp(out, tx, r)) ? OKTEXT : WRONG; (withRxt ? R1 : R0)[0][i][o]++; }
                skipA:;
                // ---- check B: CRC-8 over the rebuilt text (1 byte in the header)
                { size_t Lc = bl + rx - ks[i]; if (Lc < 6) goto skipB; size_t avail = Lc - 5, need = 0; size_t r = AprsCompress::decodeCanon(fb, Lc, 5, avail, out, 255, true, &need);
                  int o = (r == 0) ? REJ : (r == t.size() && !memcmp(out, tx, r)) ? OKTEXT : WRONG; (withRxt ? R1 : R0)[1][i][o]++; }
                skipB:;
                // ---- check C: decode, re-encode, compare with the start of what was received (the payload; anything after it is the tail)
                { size_t Lc = al + rx - ks[i]; if (Lc < 5) goto skipC; size_t avail = Lc - 4, need = 0; size_t r = AprsCompress::decodeCanon(fa, Lc, 4, avail, out, 255, false, &need);
                  int o = REJ;
                  if (r) { size_t rl = AprsCompress::encodeCanon(out, r, re, sizeof re, false); size_t pl = rl - 4; bool same = withRxt ? (pl <= avail && !memcmp(re + 4, fa + 4, pl)) : (pl == avail && !memcmp(re + 4, fa + 4, avail)); if (same) o = (r == t.size() && !memcmp(out, tx, r)) ? OKTEXT : WRONG; }
                  (withRxt ? R1 : R0)[2][i][o]++; }
                skipC:;
            }
        }
    }
    printf("%ld real packet texts (full frames).  Average frame bytes: current encoder %.1f | canonical flush %.1f (+%.1f) | canonical flush + 1 check byte %.1f (+%.1f)\n", used, (double)cur_len / used, (double)can_len / used, (double)(can_len - cur_len) / used, (double)chk_len / used, (double)(chk_len - cur_len) / used);
    printf("The CURRENT format with a 6 byte relay tail appended decodes to the right text in only %ld of %ld intact frames (%.1f%%).\n", intact_rxt_cur_ok, intact_rxt_cur, 100.0 * intact_rxt_cur_ok / intact_rxt_cur);
    const char* nm[3] = { "A  decoder must end exactly at the payload's last byte (0 bytes)", "B  CRC-8 of the rebuilt text (1 byte)", "C  decode, re-encode, compare (0 bytes)" };
    printf("\nBASELINE, the current decoder (no check), frame cut by k bytes, no tail:\n");
    for (int i = 0; i < NK; i++) { char b[40]; snprintf(b, sizeof b, ks[i] ? "cut by %d byte%s" : "not cut (intact)", ks[i], ks[i] > 1 ? "s" : ""); row(b, BASE[i]); }
    for (int w = 0; w < 2; w++) {
        printf("\n===== %s =====\n", w ? "WITH a 6 byte RXT tail after the payload; the receiver must find the payload end itself (cuts eat the tail first)" : "WITHOUT a tail");
        for (int c = 0; c < 3; c++) { printf("   check %s\n", nm[c]); for (int i = 0; i < NK; i++) { char b[40]; snprintf(b, sizeof b, ks[i] ? "cut by %d byte%s" : "not cut (intact)", ks[i], ks[i] > 1 ? "s" : ""); row(b, (w ? R1 : R0)[c][i]); } }
    }
    // cost of check C on this machine
    std::vector<std::vector<uint8_t>> fr; for (auto& t : texts) { uint8_t x[300]; size_t l = AprsCompress::encodeCanon((const uint8_t*)t.data(), t.size(), x, sizeof x, false); if (l) fr.push_back(std::vector<uint8_t>(x, x + l)); }
    uint8_t out[300], re[300]; size_t need;
    auto t0 = std::chrono::steady_clock::now(); for (auto& x : fr) AprsCompress::decodeCanon(x.data(), x.size(), 4, x.size() - 4, out, 255, false, &need);
    auto t1 = std::chrono::steady_clock::now(); for (auto& x : fr) { size_t r = AprsCompress::decodeCanon(x.data(), x.size(), 4, x.size() - 4, out, 255, false, &need); AprsCompress::encodeCanon(out, r, re, sizeof re, false); }
    auto t2 = std::chrono::steady_clock::now();
    double d = std::chrono::duration<double, std::micro>(t1 - t0).count() / fr.size(), dr = std::chrono::duration<double, std::micro>(t2 - t1).count() / fr.size();
    printf("\nTime per frame on this Mac: decode %.0f us; decode + re-encode %.0f us (check C adds %.0f us).  (The ESP32 is a few tens of times slower; not measured.)\n", d, dr, dr - d);
}
