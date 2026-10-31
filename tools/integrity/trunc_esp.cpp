// Truncation and corruption test of the ESP32 iGate firmware's decoder (lib/AprsCompress), on real packet texts.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include "AprsCompress.h"

static uint64_t rs = 88172645463325252ULL;
static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
enum { REJECT, SAME, PREFIX, OTHER, NOUT };
static const char* NAME[NOUT] = { "rejected", "same text", "shorter text (a prefix)", "different text" };

static int classifyFull(const uint8_t* fr, size_t flen, const std::string& t) {
    uint8_t out[300]; size_t r = AprsCompress::decodeFrame(fr, flen, out, 255);
    if (r == 0) return REJECT;
    if (r == t.size() && !memcmp(out, t.data(), r)) return SAME;
    if (r < t.size() && !memcmp(out, t.data(), r)) return PREFIX;
    return OTHER;
}
static int classifyPrimed(const uint8_t* ref, size_t rl, const uint8_t* fr, size_t flen, const std::string& t) {
    uint8_t out[300]; size_t r = AprsCompress::decodePrimed(ref, rl, fr, flen, out, 255);
    if (r == 0) return REJECT;
    if (r == t.size() && !memcmp(out, t.data(), r)) return SAME;
    if (r < t.size() && !memcmp(out, t.data(), r)) return PREFIX;
    return OTHER;
}
static void row(const char* what, long c[NOUT]) {
    long n = 0; for (int i = 0; i < NOUT; i++) n += c[i];
    printf("  %-38s n=%-6ld", what, n);
    for (int i = 0; i < NOUT; i++) printf("  %s %5.2f%%", NAME[i], n ? 100.0 * c[i] / n : 0.0);
    printf("   accepted wrongly (shorter+different): %.3f%%\n", n ? 100.0 * (c[PREFIX] + c[OTHER]) / n : 0.0);
}
int main(int argc, char** argv) {
    std::vector<std::string> texts; char line[400]; FILE* f = fopen(argv[1], "rb");
    while (fgets(line, sizeof line, f)) { std::string s(line); while (!s.empty() && s.back() == '\n') s.pop_back(); if (s.size() >= 20) texts.push_back(s); }
    fclose(f);
    long full_n = 0, intact_ok = 0, primed_n = 0, primed_intact = 0, skipped_not_smaller = 0;
    long TK[8][NOUT] = {{0}}, RC[NOUT] = {0}, FL[4][NOUT] = {{0}}, RND[NOUT] = {0}, PT[3][NOUT] = {{0}}, PF[3][NOUT] = {{0}}, PR[NOUT] = {0};
    const int ks[8] = { 1, 2, 3, 4, 6, 8, 12, 16 }; const int flips[4] = { 1, 2, 3, 8 };
    for (auto& t : texts) {
        uint8_t fr[300], fc[300]; const uint8_t* tx = (const uint8_t*)t.data();
        size_t fl = AprsCompress::encodeFrame(tx, t.size(), fr, sizeof fr);
        if (fl == 0) { skipped_not_smaller++; continue; }
        full_n++; if (classifyFull(fr, fl, t) == SAME) intact_ok++;
        for (int a = 0; a < 8; a++) if ((long)fl - ks[a] >= 3) TK[a][classifyFull(fr, fl - ks[a], t)]++;
        { size_t cut = 1 + rnd() % (uint32_t)(fl - 4); RC[classifyFull(fr, fl - cut, t)]++; }
        for (int a = 0; a < 4; a++) { memcpy(fc, fr, fl); for (int b = 0; b < flips[a]; b++) { size_t p = 3 + rnd() % (uint32_t)(fl - 3); fc[p] ^= (uint8_t)(1u << (rnd() % 8)); } FL[a][classifyFull(fc, fl, t)]++; }
        memcpy(fc, fr, fl); for (size_t p = 3; p < fl; p++) fc[p] = (uint8_t)rnd(); RND[classifyFull(fc, fl, t)]++;
        uint8_t pf[300]; size_t pl = AprsCompress::encodePrimed(tx, t.size(), tx, t.size(), pf, sizeof pf);   // the same text sent again, primed on itself
        if (pl == 0) continue;
        primed_n++; if (classifyPrimed(tx, t.size(), pf, pl, t) == SAME) primed_intact++;
        for (int a = 0; a < 3; a++) if ((long)pl - (a + 1) >= 3) PT[a][classifyPrimed(tx, t.size(), pf, pl - (a + 1), t)]++;
        for (int a = 0; a < 3; a++) { memcpy(fc, pf, pl); for (int b = 0; b < flips[a]; b++) { size_t p = 3 + rnd() % (uint32_t)(pl - 3); fc[p] ^= (uint8_t)(1u << (rnd() % 8)); } PF[a][classifyPrimed(tx, t.size(), fc, pl, t)]++; }
        memcpy(fc, pf, pl); for (size_t p = 3; p < pl; p++) fc[p] = (uint8_t)rnd(); PR[classifyPrimed(tx, t.size(), fc, pl, t)]++;
    }
    printf("ESP32 iGate library (AprsCompress, model version %d).  %zu real packet texts; full frames made: %ld (%ld not smaller than plain, skipped).  Intact full frames decoded back exactly: %ld of %ld.\n", AprsCompress::modelVersion(), texts.size(), full_n, skipped_not_smaller, intact_ok, full_n);
    printf("\nFULL compressed frames, %ld of them: bytes cut off the END (the LoRa CRC would still be valid)\n", full_n);
    for (int a = 0; a < 8; a++) { char b[64]; snprintf(b, sizeof b, "cut off %d byte%s", ks[a], ks[a] > 1 ? "s" : ""); row(b, TK[a]); }
    row("cut at a random point", RC);
    printf("\nFULL frames: bits flipped (a corrupted frame that got past the CRC)\n");
    for (int a = 0; a < 4; a++) { char b[64]; snprintf(b, sizeof b, "%d random bit%s flipped", flips[a], flips[a] > 1 ? "s" : ""); row(b, FL[a]); }
    row("payload replaced by random bytes", RND);
    printf("\nPRIMED frames (with the text check byte), %ld of them (intact decoded exactly: %ld)\n", primed_n, primed_intact);
    for (int a = 0; a < 3; a++) { char b[64]; snprintf(b, sizeof b, "cut off %d byte%s", a + 1, a ? "s" : ""); row(b, PT[a]); }
    for (int a = 0; a < 3; a++) { char b[64]; snprintf(b, sizeof b, "%d random bit%s flipped", flips[a], flips[a] > 1 ? "s" : ""); row(b, PF[a]); }
    row("payload replaced by random bytes", PR);
    return 0;
}
