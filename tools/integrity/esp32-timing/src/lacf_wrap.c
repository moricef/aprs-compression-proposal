/* The repository's reference codec plus the LACF ending, compiled as C and exposed to the Arduino sketch. */
#ifdef USE_FAST
#define APRS_PPM_FAST_BUILD_FREQ "../tools/integrity/fast_freq.h"   /* relative to reference/ */
#endif
#include <stdint.h>
#ifdef FF_PROF
uint32_t ffp[5];
#endif
#include "../../../../reference/aprs_ppm.c"
#if defined(HB_INFO)
#include "../../../../tools/hybrid/tables/model_hybridB_info_native.c"
#elif defined(HB_COMMENT)
#include "../../../../tools/hybrid/tables/model_hybridB_comment.c"
#else
#include "../../../../model/aprs_ppm_model_v1.c"
#endif
#include "../../lacf_core.h"
#if defined(MODEL_RAM) || defined(MODEL_RAM_KEYS)
#include <stdlib.h>
#include <string.h>
const uint8_t *ff_keys[APRS_PPM_ORDER + 1], *ff_sym, *ff_cnt;
const uint16_t *ff_offs[APRS_PPM_ORDER + 1];
/* copy the whole model into RAM; returns the bytes used (0 if there was no memory) */
size_t x_model_to_ram(void)
{
    size_t total = 0, entries = aprs_ppm_orders[APRS_PPM_ORDER].offsets[aprs_ppm_orders[APRS_PPM_ORDER].num_contexts]; int k;
#ifdef MODEL_RAM
    uint8_t *sym = (uint8_t *)malloc(entries), *cnt = (uint8_t *)malloc(entries);
    if (!sym || !cnt) return 0;
    memcpy(sym, aprs_ppm_symbols, entries); memcpy(cnt, aprs_ppm_counts, entries); ff_sym = sym; ff_cnt = cnt; total = 2 * entries;
#else
    (void)entries;
#endif
    for (k = 0; k <= APRS_PPM_ORDER; k++) {
        size_t nc = aprs_ppm_orders[k].num_contexts;
        uint8_t *kk = (uint8_t *)malloc(3 * nc);
        if (!kk) return 0;
        memcpy(kk, aprs_ppm_orders[k].keys, 3 * nc); ff_keys[k] = kk; total += 3 * nc;
#ifdef MODEL_RAM
        { uint16_t *oo = (uint16_t *)malloc(2 * (nc + 1)); if (!oo) return 0; memcpy(oo, aprs_ppm_orders[k].offsets, 2 * (nc + 1)); ff_offs[k] = oo; total += 2 * (nc + 1); }
#endif
    }
    return total;
}
#else
size_t x_model_to_ram(void) { return 0; }
#endif
size_t x_encode_current(const uint8_t *t, size_t n, uint8_t *f, size_t cap) { return aprs_ppm_encode_frame(t, n, f, cap); }
size_t x_decode(const uint8_t *f, size_t n, uint8_t *t, size_t cap) { return aprs_ppm_decode_frame(f, n, t, cap); }
size_t x_encode_lacf(const uint8_t *t, size_t n, uint8_t *f, size_t cap) { return lacf_encode(t, n, f, cap); }
size_t x_receive_two_pass(const uint8_t *f, size_t n, uint8_t *t, size_t *pay, int tail) { return lacf_receive_two_pass(f, n, t, pay, tail); }
size_t x_receive(const uint8_t *f, size_t n, uint8_t *t, size_t *pay, int tail) { return lacf_receive(f, n, t, pay, tail); }
uint16_t x_crc16(const uint8_t *t, size_t n) { return lacf_crc16(t, n); }
