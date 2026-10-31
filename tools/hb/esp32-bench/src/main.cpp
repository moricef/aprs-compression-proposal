// Relay pipeline benchmark for spec 11 on an ESP32: the real frame code (tools/hb), encode, receive (verify), and a complete relay hop, per packet, against the
// SF7 / BW125 / CR4/5 airtime of the same frame, without and with WiFi running (a soft access point plus background scans: the radio, interrupts and the
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
static const int REPS = 10;

static void run(const char *label) {
    uint8_t fr[300], out[300]; char tn[800]; double se = 0, sd = 0, sr = 0, mxe = 0, mxd = 0, mxr = 0, worst_ratio = 0; int worst_i = -1; nrows = 0;
    for (int i = 0; i < PACKETS_N; i++) {
        const char *p = PACKETS[i]; size_t pl = strlen(p);
        size_t fl = hb_encode(p, pl, nullptr, fr, sizeof fr, nullptr);
        if (!fl) continue;
        int64_t t0 = esp_timer_get_time();
        for (int r = 0; r < REPS; r++) hb_encode(p, pl, nullptr, out, sizeof out, nullptr);
        int64_t t1 = esp_timer_get_time();
        for (int r = 0; r < REPS; r++) hb_decode(fr, fl, tn, sizeof tn, nullptr);
        int64_t t2 = esp_timer_get_time();
        size_t rl = 0; for (int r = 0; r < REPS; r++) rl = hb_relay(fr, fl, "N0RLY-1", TUP, out, sizeof out);
        int64_t t3 = esp_timer_get_time();
        Row &w = rows[nrows++]; w.len = (int)pl; w.frame = (int)fl; w.enc = (t1 - t0) / (double)REPS; w.dec = (t2 - t1) / (double)REPS; w.rel = rl ? (t3 - t2) / (double)REPS : 0;
        se += w.enc; sd += w.dec; sr += w.rel; if (w.enc > mxe) mxe = w.enc; if (w.dec > mxd) mxd = w.dec; if (w.rel > mxr) mxr = w.rel;
        double ratio = (w.dec + w.rel) / 1000.0 / airtime_ms((int)fl, 7);                      // verify (receive) + relay build, as a share of the SF7 airtime of that frame
        if (ratio > worst_ratio) { worst_ratio = ratio; worst_i = nrows - 1; }
    }
    int nr = 0; for (int i = 0; i < nrows; i++) if (rows[i].rel > 0) nr++;
    P("%s: %d packets; mean encode %.2f ms, receive %.2f ms, relay %.2f ms (over %d relayable); max encode %.2f, receive %.2f, relay %.2f ms\n", label, nrows, se / nrows / 1000, sd / nrows / 1000, nr ? sr / nr / 1000 : 0.0, nr, mxe / 1000, mxd / 1000, mxr / 1000);
    if (worst_i >= 0) P("  worst receive + relay against SF7 airtime: %.1f%% (frame %d B, %.0f ms on air; receive %.2f ms, relay %.2f ms); SF7 airtime of the longest frame (255 B) is %.0f ms\n", worst_ratio * 100, rows[worst_i].frame, airtime_ms(rows[worst_i].frame, 7), rows[worst_i].dec / 1000, rows[worst_i].rel / 1000, airtime_ms(255, 7));
    { int big = 0; for (int i = 1; i < nrows; i++) if (rows[i].frame > rows[big].frame) big = i;
      P("  longest frame in the set: %d B (text %d chars): encode %.2f, receive %.2f, relay %.2f ms; SF7 airtime %.0f ms\n", rows[big].frame, rows[big].len, rows[big].enc / 1000, rows[big].dec / 1000, rows[big].rel / 1000, airtime_ms(rows[big].frame, 7)); }
}

static void bench_task(void *) {
    P("\nspec 11 relay pipeline benchmark: %s, %u MHz, %d packets, free heap %u\n", ESP.getChipModel(), (unsigned)getCpuFrequencyMhz(), (int)PACKETS_N, (unsigned)ESP.getFreeHeap());
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
