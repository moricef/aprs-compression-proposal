/* Truncation and corruption of FULL compressed frames, with check C (decode, re-encode with the same encoder, compare), using only this repository's
 * reference codec.   make integrity      or      build/trunc_ref [texts.txt]
 * With no argument it makes 3000 fabricated packets (made-up callsigns and positions), so anyone can run it. With a file (one APRS text per line) it uses
 * those, which is how the numbers in docs/truncated-payloads-and-integrity.md were made (with real texts that are not in this repository).
 * Note: this uses the CURRENT frame ending of the reference codec. The "independent ending" experiments need the private library copy (see README.txt). */
#include "../../reference/aprs_ppm.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng = 20261031;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static int synth(char *line, int i)
{
    static const char *cm[] = {"LoRa Tracker", "iGate hill", "balloon", "T-Deck", "", "Bat 3.9V", "QRV 433.775"};
    int lat = 3200 + rnd() % 400, lon = 11000 + rnd() % 500;
    return snprintf(line, 300, "N0CALL-%d>APLRT1%s:!%02d%02d.%02dN/%03d%02d.%02dW>%s %d.%dV", i % 12, (rnd() & 1) ? ",WIDE1-1" : "", lat / 100, lat % 100,
                    (int)(rnd() % 100), lon / 100, lon % 100, (int)(rnd() % 100), cm[rnd() % 7], 3 + (int)(rnd() % 2), (int)(rnd() % 10));
}

/* check C on a received payload: decode, re-encode, compare. Returns the text length if the frame is accepted, else 0. */
static size_t decode_c(const uint8_t *fr, size_t fl, uint8_t *out, int use_c)
{
    uint8_t again[400]; size_t tl = aprs_ppm_decode_frame(fr, fl, out, 300), al;
    if (!tl || !use_c) return tl;
    al = aprs_ppm_encode_frame(out, tl, again, sizeof again);
    return (al == fl && !memcmp(again, fr, fl)) ? tl : 0;
}

typedef struct { long n, plain_wrong, c_wrong, c_rejected_good; } tally_t;
static void classify(tally_t *t, const uint8_t *fr, size_t fl, const uint8_t *text, size_t tl)
{
    uint8_t back[300]; size_t a = decode_c(fr, fl, back, 0), c = decode_c(fr, fl, back, 1);
    t->n++;
    if (a) { uint8_t b2[300]; size_t k = aprs_ppm_decode_frame(fr, fl, b2, 300); if (k != tl || memcmp(b2, text, tl)) t->plain_wrong++; }
    if (c) { if (c != tl || memcmp(back, text, tl)) t->c_wrong++; }
}

int main(int argc, char **argv)
{
    static char lines[20000][400]; static size_t len[20000]; int n = 0, i, j, k; FILE *f = NULL;
    static const int cuts[] = {1, 2, 3, 4, 6, 8, 12, 16}, flips[] = {1, 2, 3, 8};
    long intact = 0, intact_c = 0, made = 0;
    if (argc > 1) { f = fopen(argv[1], "r"); if (!f) { perror(argv[1]); return 2; } }
    if (f) { while (n < 20000 && fgets(lines[n], sizeof lines[n], f)) { len[n] = strcspn(lines[n], "\r\n"); lines[n][len[n]] = 0; if (len[n]) n++; } fclose(f); }
    else for (n = 0; n < 3000; n++) len[n] = (size_t)synth(lines[n], n);
    printf("%d texts (%s)\n", n, f ? argv[1] : "fabricated");
    {
        tally_t cut[8], flip[4], rb; memset(cut, 0, sizeof cut); memset(flip, 0, sizeof flip); memset(&rb, 0, sizeof rb);
        for (i = 0; i < n; i++) {
            uint8_t fr[400], tmp[400], back[300]; size_t fl = aprs_ppm_encode_frame((uint8_t *)lines[i], len[i], fr, sizeof fr);
            if (!fl) continue;
            made++;
            { size_t t = decode_c(fr, fl, back, 1); intact++; if (t == len[i] && !memcmp(back, lines[i], t)) intact_c++; }
            for (j = 0; j < 8; j++) if (fl > (size_t)cuts[j] + 4) classify(&cut[j], fr, fl - cuts[j], (uint8_t *)lines[i], len[i]);
            for (j = 0; j < 4; j++) {
                memcpy(tmp, fr, fl);
                for (k = 0; k < flips[j]; k++) { size_t p = 4 + rnd() % (fl - 4); tmp[p] ^= (uint8_t)(1u << (rnd() % 8)); }
                if (!memcmp(tmp, fr, fl)) continue;
                classify(&flip[j], tmp, fl, (uint8_t *)lines[i], len[i]);
            }
            memcpy(tmp, fr, fl); for (k = 4; (size_t)k < fl; k++) tmp[k] = (uint8_t)rnd();
            classify(&rb, tmp, fl, (uint8_t *)lines[i], len[i]);
        }
        printf("full frames made: %ld; intact frames accepted by check C with the right text: %ld\n\n", made, intact_c);
        printf("%-28s %8s  %-28s %-28s\n", "damage (LoRa CRC still valid)", "frames", "plain decoder: wrong accepted", "check C: wrong accepted");
        for (j = 0; j < 8; j++) printf("cut off %2d bytes              %8ld  %6ld (%6.2f%%)             %6ld (%.3f%%)\n", cuts[j], cut[j].n, cut[j].plain_wrong, 100.0 * cut[j].plain_wrong / cut[j].n, cut[j].c_wrong, 100.0 * cut[j].c_wrong / cut[j].n);
        for (j = 0; j < 4; j++) printf("%2d random bits flipped         %8ld  %6ld (%6.2f%%)             %6ld (%.3f%%)\n", flips[j], flip[j].n, flip[j].plain_wrong, 100.0 * flip[j].plain_wrong / flip[j].n, flip[j].c_wrong, 100.0 * flip[j].c_wrong / flip[j].n);
        printf("payload replaced by random    %8ld  %6ld (%6.2f%%)             %6ld (%.3f%%)\n", rb.n, rb.plain_wrong, 100.0 * rb.plain_wrong / rb.n, rb.c_wrong, 100.0 * rb.c_wrong / rb.n);
    }
    return intact_c == intact ? 0 : 1;
}
