/* fast_freq.h must give the reference's build_freq output exactly: compared for every position of every text, with and without a reference text,
 * plus the model property it relies on (symbols distinct within a context, counts > 0).   build/fast_freq_test texts.txt */
#include "../../reference/aprs_ppm.c"
#define build_freq build_freq_fast
#include "fast_freq.h"
#undef build_freq
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    static char lines[20000][400]; static size_t len[20000]; int n = 0, i, k; FILE *f = argc > 1 ? fopen(argv[1], "r") : NULL; long cmp = 0, bad = 0, ctx = 0, dup = 0;
    if (!f) { fprintf(stderr, "usage: fast_freq_test texts.txt\n"); return 2; }
    while (n < 20000 && fgets(lines[n], sizeof lines[n], f)) { len[n] = strcspn(lines[n], "\r\n"); lines[n][len[n]] = 0; if (len[n]) n++; }
    fclose(f);
    {   /* ff_scale against the 64 bit definition */
        uint32_t x = 12345; long i2, badf = 0, badd = 0;  (void)badd;
        for (i2 = 0; i2 < 20000000; i2++) {
            uint32_t p; x = x * 1664525u + 1013904223u; p = (i2 < 4) ? (i2 == 0 ? 0 : i2 == 1 ? 0xFFFFFFFFu : i2 == 2 ? 0xFFFFu : 0x10000u) : (x ^ (i2 & 1 ? 0 : x << 13));
            if (ff_scale(p) != (uint32_t)(((uint64_t)p * 65280u) >> 32)) badf++;
        }
        printf("ff_scale: 20000000 values, %ld differ\n", badf);
        if (badf || badd) return 1;
    }
    for (k = 0; k <= APRS_PPM_ORDER; k++) {
        uint32_t c, a;
        for (c = 0; c < aprs_ppm_orders[k].num_contexts; c++) {
            uint32_t from = aprs_ppm_orders[k].offsets[c], to = aprs_ppm_orders[k].offsets[c + 1]; ctx++;
            for (a = from; a < to; a++) { uint32_t b; if (!aprs_ppm_counts[a]) dup++; for (b = a + 1; b < to; b++) if (aprs_ppm_symbols[a] == aprs_ppm_symbols[b]) dup++; }
        }
    }
    for (i = 0; i < n; i++) {
        size_t pos; const uint8_t *ref = i ? (const uint8_t *)lines[i - 1] : NULL;
        for (pos = 0; pos < len[i]; pos++) {
            uint16_t a[256], b[256]; uint32_t ta, tb; int use_ref;
            for (use_ref = 0; use_ref < 2; use_ref++) {
                if (use_ref && !ref) continue;
                ta = build_freq((uint8_t *)lines[i], pos, a, use_ref ? ref : NULL, use_ref ? len[i - 1] : 0);
                tb = build_freq_fast((uint8_t *)lines[i], pos, b, use_ref ? ref : NULL, use_ref ? len[i - 1] : 0);
                cmp++; if (ta != tb || memcmp(a, b, sizeof a)) bad++;
            }
        }
    }
    printf("model: %ld contexts checked, %ld entries with a zero count or a repeated symbol\n%ld positions compared (with and without a reference text), %ld differ\n", ctx, dup, cmp, bad);
    return (bad || dup) ? 1 : 0;
}
