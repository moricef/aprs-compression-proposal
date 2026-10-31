/* Truncation and corruption test of the APRS-PPM decoder (loracompress.c), on real packet texts.  Standalone; reads one TNC2 text per line. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "loracompress.h"

static uint64_t rs = 88172645463325252ULL;
static uint32_t rnd (void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }

enum { REJECT, SAME, PREFIX, OTHER, NOUT };
static const char *NAME[NOUT] = { "rejected", "same text", "shorter text (a prefix)", "different text" };

static int classify (lorac_store_t *rx, const uint8_t *fr, int flen, const uint8_t *text, int tlen) {
    uint8_t out[300]; int kind = 0;
    int r = lorac_rx(rx, fr, flen, out, sizeof out, &kind);
    if (r <= 0) return REJECT;
    if (r == tlen && memcmp(out, text, (size_t)r) == 0) return SAME;
    if (r < tlen && memcmp(out, text, (size_t)r) == 0) return PREFIX;
    return OTHER;
}
static void row (const char *what, long c[NOUT]) {
    long n = 0; for (int i = 0; i < NOUT; i++) n += c[i];
    printf("  %-38s n=%-6ld", what, n);
    for (int i = 0; i < NOUT; i++) printf("  %s %5.2f%%", NAME[i], n ? 100.0 * c[i] / n : 0.0);
    printf("   accepted wrongly (shorter+different): %.3f%%\n", n ? 100.0 * (c[PREFIX] + c[OTHER]) / n : 0.0);
}

int main (int argc, char **argv) {
    if (argc < 2) return 2;
    static char line[400]; static char *texts[40000]; static int tl[40000]; int nt = 0;
    FILE *f = fopen(argv[1], "rb");
    while (fgets(line, sizeof line, f) && nt < 40000) { int n = (int)strlen(line); while (n && (line[n-1] == '\n')) line[--n] = 0; if (n < 20) continue; texts[nt] = strdup(line); tl[nt++] = n; }
    fclose(f);
    static lorac_store_t tx, rx;
    long kinds[3] = {0}, intact_ok = 0, full_n = 0;
    long TK[10][NOUT] = {{0}}, RC[NOUT] = {0}, FL[4][NOUT] = {{0}}, RND[NOUT] = {0}, PT[4][NOUT] = {{0}}, PF[4][NOUT] = {{0}}, PR[NOUT] = {0};
    const int ks[8] = { 1, 2, 3, 4, 6, 8, 12, 16 }; const int flips[4] = { 1, 2, 3, 8 };
    long primed_n = 0, primed_intact = 0;
    for (int i = 0; i < nt; i++) {
        uint8_t fr[300], fc[300]; int kind = 0, flen;
        lorac_store_init(&tx); flen = lorac_tx(&tx, 1, (uint8_t *)texts[i], tl[i], fr, sizeof fr, &kind);
        if (flen <= 0) continue; kinds[kind]++;
        if (kind == LORAC_FULL) {
            full_n++;
            lorac_store_init(&rx); if (classify(&rx, fr, flen, (uint8_t *)texts[i], tl[i]) == SAME) intact_ok++;
            for (int a = 0; a < 8; a++) if (flen - ks[a] >= 3) { lorac_store_init(&rx); TK[a][classify(&rx, fr, flen - ks[a], (uint8_t *)texts[i], tl[i])]++; }
            { int cut = 1 + (int)(rnd() % (uint32_t)(flen - 4)); lorac_store_init(&rx); RC[classify(&rx, fr, flen - cut, (uint8_t *)texts[i], tl[i])]++; }
            for (int a = 0; a < 4; a++) { memcpy(fc, fr, (size_t)flen); for (int b = 0; b < flips[a]; b++) { int p = 3 + (int)(rnd() % (uint32_t)(flen - 3)); fc[p] ^= (uint8_t)(1u << (rnd() % 8)); } lorac_store_init(&rx); FL[a][classify(&rx, fc, flen, (uint8_t *)texts[i], tl[i])]++; }
            memcpy(fc, fr, (size_t)flen); for (int p = 3; p < flen; p++) fc[p] = (uint8_t)rnd(); lorac_store_init(&rx); RND[classify(&rx, fc, flen, (uint8_t *)texts[i], tl[i])]++;
        }
        /* primed frame: the same text sent again after a full or plain one */
        {
            uint8_t f1[300], f2[300]; int k1 = 0, k2 = 0, l1, l2;
            lorac_store_init(&tx); l1 = lorac_tx(&tx, 4, (uint8_t *)texts[i], tl[i], f1, sizeof f1, &k1); l2 = lorac_tx(&tx, 4, (uint8_t *)texts[i], tl[i], f2, sizeof f2, &k2);
            if (l1 > 0 && l2 > 0 && k2 == LORAC_PRIMED) {
                primed_n++;
                lorac_store_init(&rx); lorac_rx(&rx, f1, l1, fc, sizeof fc, &k1); if (classify(&rx, f2, l2, (uint8_t *)texts[i], tl[i]) == SAME) primed_intact++;
                for (int a = 0; a < 3; a++) if (l2 - (a + 1) >= 3) { lorac_store_init(&rx); uint8_t t_[300]; int k_; lorac_rx(&rx, f1, l1, t_, sizeof t_, &k_); PT[a][classify(&rx, f2, l2 - (a + 1), (uint8_t *)texts[i], tl[i])]++; }
                for (int a = 0; a < 3; a++) { memcpy(fc, f2, (size_t)l2); for (int b = 0; b < flips[a]; b++) { int p = 3 + (int)(rnd() % (uint32_t)(l2 - 3)); fc[p] ^= (uint8_t)(1u << (rnd() % 8)); } lorac_store_init(&rx); uint8_t t_[300]; int k_; lorac_rx(&rx, f1, l1, t_, sizeof t_, &k_); PF[a][classify(&rx, fc, l2, (uint8_t *)texts[i], tl[i])]++; }
                memcpy(fc, f2, (size_t)l2); for (int p = 3; p < l2; p++) fc[p] = (uint8_t)rnd(); lorac_store_init(&rx); { uint8_t t_[300]; int k_; lorac_rx(&rx, f1, l1, t_, sizeof t_, &k_); } PR[classify(&rx, fc, l2, (uint8_t *)texts[i], tl[i])]++;
            }
        }
    }
    printf("%d real packet texts.  Encoder chose: plain %ld, full compressed %ld, primed %ld.  Intact full frames decoded back exactly: %ld of %ld.\n", nt, kinds[0], kinds[1], kinds[2], intact_ok, full_n);
    printf("\nFULL compressed frames (3C FF 81), %ld of them: bytes cut off the END (the LoRa CRC would still be valid)\n", full_n);
    for (int a = 0; a < 8; a++) { char b[64]; snprintf(b, sizeof b, "cut off %d byte%s", ks[a], ks[a] > 1 ? "s" : ""); row(b, TK[a]); }
    row("cut at a random point", RC);
    printf("\nFULL frames: bits flipped (a corrupted frame that got past the CRC)\n");
    for (int a = 0; a < 4; a++) { char b[64]; snprintf(b, sizeof b, "%d random bit%s flipped", flips[a], flips[a] > 1 ? "s" : ""); row(b, FL[a]); }
    row("payload replaced by random bytes", RND);
    printf("\nPRIMED frames (3C FF C1, with the text check byte), %ld of them (intact decoded exactly: %ld)\n", primed_n, primed_intact);
    for (int a = 0; a < 3; a++) { char b[64]; snprintf(b, sizeof b, "cut off %d byte%s", a + 1, a ? "s" : ""); row(b, PT[a]); }
    for (int a = 0; a < 3; a++) { char b[64]; snprintf(b, sizeof b, "%d random bit%s flipped", flips[a], flips[a] > 1 ? "s" : ""); row(b, PF[a]); }
    row("payload replaced by random bytes", PR);
    return 0;
}
