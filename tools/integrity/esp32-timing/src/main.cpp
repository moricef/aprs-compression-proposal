// Timing of the LACF ending and the receiver's re-encode check (check C) on an ESP32. Fabricated packets only. Prints to the serial port at 115200.
#include <Arduino.h>
#include "esp_timer.h"
#include "texts.h"
// Hybrid B builds (spec 11): time the PPM-coded part only, so the texts are the information field (HB_INFO, info-native table) or the comment after a
// position block (HB_COMMENT, comment-only table), cut out of the same fabricated packets. The conformance vectors are for the v1 table and are skipped.
#if defined(HB_INFO) || defined(HB_COMMENT)
static const char* g_tx[64]; static int g_ntx;
static void build_texts() {
    for (size_t i = 0; i < sizeof TEXTS / sizeof *TEXTS && g_ntx < 64; i++) {
        const char* s = strchr(TEXTS[i], ':'); if (!s) continue; s++;
#ifdef HB_COMMENT
        if (s[0] == '!' || s[0] == '=') { size_t skip = (s[1] >= '0' && s[1] <= '9') ? 20 : 14; if (strlen(s) <= skip) continue; s += skip; } else continue;
#endif
        if (*s) g_tx[g_ntx++] = s;
    }
}
#define TX(i) g_tx[i]
#define NTX (size_t)g_ntx
#else
#define TX(i) TEXTS[i]
#define NTX (sizeof TEXTS / sizeof *TEXTS)
#endif
#include <stdarg.h>
static String g_out;                       // everything printed, repeated from loop() so a late-opened serial port still sees the result
static void P(const char *fmt, ...) { char b[400]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); Serial.print(b); g_out += b; }
extern "C" {
extern uint32_t ffp[5];
size_t x_model_to_ram(void);
size_t x_encode_current(const uint8_t *t, size_t n, uint8_t *f, size_t cap);
size_t x_decode(const uint8_t *f, size_t n, uint8_t *t, size_t cap);
size_t x_encode_lacf(const uint8_t *t, size_t n, uint8_t *f, size_t cap);
size_t x_receive(const uint8_t *f, size_t n, uint8_t *t, size_t *pay, int tail);
size_t x_receive_two_pass(const uint8_t *f, size_t n, uint8_t *t, size_t *pay, int tail);
uint16_t x_crc16(const uint8_t *t, size_t n);
}
#ifdef USE_FAST
#if defined(MODEL_RAM)
#define TIMING_BUILD "fast build_freq, whole model in RAM"
#elif defined(MODEL_RAM_KEYS)
#ifdef FF_RECIP
#define TIMING_BUILD "fast build_freq, keys in RAM, reciprocal"
#else
#define TIMING_BUILD "fast build_freq, keys in RAM"
#endif
#else
#define TIMING_BUILD "fast build_freq"
#endif
#else
#define TIMING_BUILD "reference build_freq"
#endif
static const int REPS = 20;
static uint8_t frame[300], back[300];

static void hexval(const char *h, uint8_t *out, size_t *n) { *n = 0; for (size_t i = 0; h[i] && h[i + 1]; i += 2) { char b[3] = {h[i], h[i + 1], 0}; out[(*n)++] = (uint8_t)strtoul(b, 0, 16); } }

void setup() {
    Serial.begin(115200); delay(1500);
#if defined(HB_INFO) || defined(HB_COMMENT)
    build_texts();
#endif
    P("\nLACF timing (%s): %s, %u MHz, %d texts, %d reps each\n", TIMING_BUILD, ESP.getChipModel(), (unsigned)getCpuFrequencyMhz(), (int)NTX, REPS);
    { size_t b = x_model_to_ram(); if (b) P("model copied to RAM: %u bytes, free heap now %u\n", (unsigned)b, (unsigned)ESP.getFreeHeap()); }
    int ok = 0;
#if defined(HB_INFO) || defined(HB_COMMENT)
    P("model: hybrid B table, %d texts, vectors skipped\n", g_ntx);
    for (int i = 0; i < 0; i++) {
#else
    for (int i = 0; i < N_VECTORS; i++) {
#endif                                    // correctness first: the frame the board makes must equal the published vector
        uint8_t want[300]; size_t wl, tl = strlen(TEXTS[i]);
        hexval(VECTOR_HEX[i], want, &wl);
        size_t fl = x_encode_lacf((const uint8_t *)TEXTS[i], tl, frame, sizeof frame);
        size_t pay, r = fl ? x_receive(frame, fl, back, &pay, 0) : 0;
        if (fl == wl && !memcmp(frame, want, wl) && (!fl || (r == tl && !memcmp(back, TEXTS[i], tl)))) ok++;
    }
    #if !(defined(HB_INFO) || defined(HB_COMMENT))
    P("vectors: %d of %d identical to the host's, and accepted by the receiver\n", ok, N_VECTORS);
#endif
    P("len,frame,encode_current_us,encode_lacf_us,decode_us,receive_two_pass_us,receive_one_pass_us,crc16_us\n");
    double s_dec = 0, s_c = 0, s_enc = 0, s_one = 0; int cnt = 0;
    for (size_t i = 0; i < NTX; i++) {
        const uint8_t *t = (const uint8_t *)TX(i); size_t tl = strlen(TX(i)), fl, pay;
        fl = x_encode_lacf(t, tl, frame, sizeof frame);
        if (!fl) continue;                                                   // not smaller than the plain frame: never sent compressed
        int64_t t0 = esp_timer_get_time();
        for (int k = 0; k < REPS; k++) x_encode_current(t, tl, back, sizeof back);
        int64_t t1 = esp_timer_get_time();
        for (int k = 0; k < REPS; k++) x_encode_lacf(t, tl, back, sizeof back);
        int64_t t2 = esp_timer_get_time();
        for (int k = 0; k < REPS; k++) x_decode(frame, fl, back, sizeof back);
        int64_t t3 = esp_timer_get_time();
        for (int k = 0; k < REPS; k++) x_receive_two_pass(frame, fl, back, &pay, 0);
        int64_t t4 = esp_timer_get_time();
        for (int k = 0; k < REPS; k++) x_receive(frame, fl, back, &pay, 0);
        int64_t t4b = esp_timer_get_time();
        volatile uint16_t sink = 0;
        for (int k = 0; k < REPS; k++) sink ^= x_crc16(t, tl);
        int64_t t5 = esp_timer_get_time();
        double one = (double)(t4b - t4) / REPS;
        s_one += one;
        double enc = (double)(t2 - t1) / REPS, dec = (double)(t3 - t2) / REPS, c = (double)(t4 - t3) / REPS;
        P("%u,%u,%.0f,%.0f,%.0f,%.0f,%.0f,%.1f\n", (unsigned)tl, (unsigned)fl, (double)(t1 - t0) / REPS, enc, dec, c, one, (double)(t5 - t4b) / REPS);
        s_enc += enc; s_dec += dec; s_c += c; cnt++;
    }
    P("MEAN over %d frames: encode %.0f us, plain decode %.0f us, receive with check C two passes %.0f us (C adds %.0f us), one pass %.0f us (C adds %.0f us)\n", cnt, s_enc / cnt, s_dec / cnt, s_c / cnt, (s_c - s_dec) / cnt, s_one / cnt, (s_one - s_dec) / cnt);
#ifdef FF_PROF
    { memset(ffp, 0, sizeof ffp); size_t chars = 0; for (size_t i = 0; i < sizeof TEXTS / sizeof *TEXTS; i++) { size_t tl = strlen(TEXTS[i]), fl = x_encode_lacf((const uint8_t *)TEXTS[i], tl, frame, sizeof frame); if (!fl) continue; memset(ffp, 0, sizeof ffp); x_decode(frame, 0, back, 0); }
      for (size_t i = 0; i < sizeof TEXTS / sizeof *TEXTS; i++) { size_t tl = strlen(TEXTS[i]), fl = x_encode_lacf((const uint8_t *)TEXTS[i], tl, frame, sizeof frame); if (!fl) continue; x_decode(frame, fl, back, sizeof back); chars += tl; }
      P("PROFILE per character (cycles): clear %u, find_context %u, entries %u, final loop %u\n", (unsigned)(ffp[0] / chars), (unsigned)(ffp[1] / chars), (unsigned)(ffp[2] / chars), (unsigned)(ffp[3] / chars)); }
#endif
    P("DONE\n");
}
void loop() { delay(6000); Serial.print("\n--- repeat of the result ---\n"); Serial.print(g_out); }
