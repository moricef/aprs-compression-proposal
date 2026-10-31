// Relay pipeline benchmark for spec 11 on an ESP32: the real frame code (tools/hb), encode, receive (verify), and a complete relay hop, as mean / median / 95th / 99th
// percentile over packets with real-traffic lengths, against the SF7 / BW125 / CR4/5 airtime of the same frame, without and with WiFi running (a soft access point plus background scans: the radio, interrupts and the
// second core are busy as in an iGate, without needing credentials). Prints per-packet rows and a summary; repeats every 10 s.
#include <Arduino.h>
#include <WiFi.h>
#include <stdarg.h>
#include "esp_timer.h"
#include "hb.h"
#include "set1.h"
#include "packets.h"

static String g_out;
static void P(const char *fmt, ...) { char b[300]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); Serial.print(b); g_out += b; }

static double airtime_ms(int payload, int sf) {                           // LoRa, BW 125 kHz, CR 4/5, explicit header, CRC on, 8 preamble symbols
    double tsym = (double)(1 << sf) / 125000.0; int de = sf >= 11 ? 1 : 0;
    double n = 8 + ceil((8.0 * payload - 4 * sf + 28 + 16) / (4.0 * (sf - 2 * de))) * 5; if (n < 8) n = 8;
    return (8 + 4.25 + n) * tsym * 1000.0;
}

struct Row { int len; int frame; double enc, dec, rel; };
static Row rows[PACKETS_N]; static int nrows;
static const uint8_t TUP[4] = { 50, 20, 45, 10 };
static const int REPS = 3;

static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static double pct(double *v, int n, double p) { return v[(int)((n - 1) * p / 100.0 + 0.5)]; }               // v sorted
static void line(const char *name, double *v, int n, const char *unit) {                                   // sorts v
    double s = 0; for (int i = 0; i < n; i++) s += v[i]; qsort(v, n, sizeof(double), cmpd);
    P("  %-26s mean %7.2f   median %7.2f   95th %7.2f   99th %7.2f   max %7.2f  %s\n", name, s / n, pct(v, n, 50), pct(v, n, 95), pct(v, n, 99), v[n - 1], unit);
}
static bool measure(const char *p, Row &w, bool *relayable) {
    uint8_t fr[300], out[300]; char tn[800]; size_t pl = strlen(p);
    size_t fl = hb_encode(p, pl, nullptr, fr, sizeof fr, nullptr); if (!fl) return false;
    int64_t t0 = esp_timer_get_time();
    for (int r = 0; r < REPS; r++) hb_encode(p, pl, nullptr, out, sizeof out, nullptr);
    int64_t t1 = esp_timer_get_time();
    for (int r = 0; r < REPS; r++) hb_decode(fr, fl, tn, sizeof tn, nullptr);
    int64_t t2 = esp_timer_get_time();
    size_t rl = 0; for (int r = 0; r < REPS; r++) rl = hb_relay(fr, fl, "N0RLY-1", TUP, out, sizeof out);
    int64_t t3 = esp_timer_get_time();
    w.len = (int)pl; w.frame = (int)fl; w.enc = (t1 - t0) / (double)REPS / 1000; w.dec = (t2 - t1) / (double)REPS / 1000; w.rel = rl ? (t3 - t2) / (double)REPS / 1000 : 0; *relayable = rl != 0;
    return true;
}

static void run(const char *label) {
    static double e[PACKETS_N], d[PACKETS_N], rl[PACKETS_N], share[PACKETS_N], air[PACKETS_N]; int ne = 0, nr = 0; nrows = 0;
    for (int i = 0; i < PACKETS_N; i++) {
        Row w; bool ok; if (!measure(PACKETS[i], w, &ok)) continue;
        rows[nrows++] = w; e[ne] = w.enc; d[ne] = w.dec; air[ne] = airtime_ms(w.frame + 0, 7); ne++;
        if (ok) { rl[nr] = w.rel; share[nr] = w.rel / airtime_ms(w.frame, 7) * 100; nr++; }
    }
    P("\n%s: %d fabricated packets with the length distribution of real traffic (median 80, 95th 151 characters), %d repetitions each\n", label, ne, REPS);
    line("encode (tracker, iGate TX)", e, ne, "ms"); line("receive + verify", d, ne, "ms"); line("relay hop (digipeater)", rl, nr, "ms (includes its own verify)");
    line("relay hop / SF7 airtime", share, nr, "% of the frame's own time on air"); line("SF7 airtime of the frame", air, ne, "ms");
    // examples by packet size: the set's packet nearest to each length
    static const int SZ[] = { 30, 50, 80, 110, 150, 183 };
    P("  by size (SF7, 125 kHz, CR 4/5; SF12 for comparison):\n  %6s %6s %9s %9s %9s %9s %9s %9s %8s\n", "text", "frame", "plain SF7", "frame SF7", "frame SF12", "encode", "receive", "relay", "relay/SF7");
    for (unsigned k = 0; k < sizeof SZ / sizeof SZ[0]; k++) {
        int best = -1; for (int i = 0; i < nrows; i++) if (rows[i].rel > 0 && (best < 0 || abs(rows[i].len - SZ[k]) < abs(rows[best].len - SZ[k]))) best = i;
        if (best < 0) continue; Row &w = rows[best];
        P("  %5dB %5dB %7.0fms %7.0fms %8.0fms %7.2fms %7.2fms %7.2fms %7.1f%%\n", w.len, w.frame, airtime_ms(w.len + 3, 7), airtime_ms(w.frame, 7), airtime_ms(w.frame, 12), w.enc, w.dec, w.rel, w.rel / airtime_ms(w.frame, 7) * 100);
    }
    P("  stress cases (not part of the percentiles):\n");
    for (int i = 0; i < STRESS_N; i++) { Row w; bool ok; if (!measure(STRESS[i], w, &ok)) continue;
        P("    text %3d chars, frame %3d B, SF7 airtime %.0f ms: encode %.2f, receive %.2f, relay %.2f ms (%.1f%% of airtime)\n", w.len, w.frame, airtime_ms(w.frame, 7), w.enc, w.dec, w.rel, w.rel / airtime_ms(w.frame, 7) * 100); }
}

static void bench_task(void *) {
    P("\nspec 11 relay pipeline benchmark: %s, %u MHz, free heap %u\n", ESP.getChipModel(), (unsigned)getCpuFrequencyMhz(), (unsigned)ESP.getFreeHeap());
    if (hb_set_list(SET1, SET1_N) != SET1_N) { P("destination list failed\n"); vTaskDelete(nullptr); }
    run("WiFi off");
    P("  stack used so far: %u of 24576 bytes\n", (unsigned)(24576 - uxTaskGetStackHighWaterMark(nullptr)));
    WiFi.mode(WIFI_AP_STA); WiFi.softAP("hbbench"); delay(500);
    WiFi.scanNetworks(true);                                                                        // background scan: busy radio and second core
    run("WiFi on (soft AP + scan)");
    P("min free heap %u; stack used by the benchmark task: %u of 24576 bytes (the default Arduino loop task has 8192)\nDONE\n", (unsigned)ESP.getMinFreeHeap(), (unsigned)(24576 - uxTaskGetStackHighWaterMark(nullptr)));
    vTaskDelete(nullptr);
}
void setup() { Serial.begin(115200); delay(1500); xTaskCreatePinnedToCore(bench_task, "bench", 24576, nullptr, 1, nullptr, 1); }
void loop() { delay(10000); Serial.print("\n--- repeat ---\n"); Serial.print(g_out); if (WiFi.scanComplete() >= 0) WiFi.scanNetworks(true); }
