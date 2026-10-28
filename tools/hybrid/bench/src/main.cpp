// Encode and decode time of APRS-PPM on an ESP32-PICO-D4 (TTGO LoRa32 V2.1), one build per layout.
// FORM_WHOLE: whole-packet text; FORM_NOPATH: source>dest:info (Hybrid A); FORM_INFO: ":"+info (Hybrid B).
#include <Arduino.h>
#include <string.h>
#include <algorithm>
#include <vector>
#if defined(FORM_WHOLE)
#include "data_whole.h"
#define FORM "whole packet"
#elif defined(FORM_NOPATH)
#include "data_nopath.h"
#define FORM "hybrid A (source>dest:info)"
#else
#include "data_info.h"
#define FORM "hybrid B (info field only)"
#endif
#include "AprsCompress.h"

static void stats(const char* name, std::vector<float>& v, double avgBytes = -1) {
    if (v.empty()) { Serial.printf("RESULT %s none\n", name); return; }
    std::sort(v.begin(), v.end());
    double s = 0; for (float x : v) s += x;
    Serial.printf("RESULT %s n=%u mean_ms=%.2f p95_ms=%.2f max_ms=%.2f min_ms=%.2f frame_bytes=%.1f\n", name, (unsigned)v.size(), s / v.size(),
                  v[(size_t)(v.size() * 0.95)], v.back(), v.front(), avgBytes);
}

void setup() {
    Serial.begin(115200); delay(1500);
    Serial.printf("BENCH start form=%s chip %s rev %d, %u MHz, model version %u\n", FORM, ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(), (unsigned)AprsCompress::modelVersion());
    static uint8_t text[300], frame[400], back[400], ref[300];
    std::vector<float> encF, decF, encP, decP; double bytesF = 0, bytesP = 0; int bad = 0, skF = 0, skP = 0;
    for (int pass = 0; pass < 2; pass++) {                       // pass 0 warms the flash cache, pass 1 is measured
        encF.clear(); decF.clear(); encP.clear(); decP.clear(); bytesF = bytesP = 0; bad = skF = skP = 0;
        for (int i = 0; i < NFULL; i++) {
            size_t n = FULL[i].n; memcpy_P(text, FULL[i].p, n);
            uint32_t t0 = micros(); size_t fl = AprsCompress::encodeFrame(text, n, frame, sizeof frame); uint32_t t1 = micros();
            if (!fl) { skF++; continue; }
            size_t dn = AprsCompress::decodeFrame(frame, fl, back, sizeof back); uint32_t t2 = micros();
            if (dn != n || memcmp(back, text, n)) bad++;
            encF.push_back((t1 - t0) / 1000.0f); decF.push_back((t2 - t1) / 1000.0f); bytesF += fl;
        }
        for (int i = 0; i < NPAIR; i++) {
            size_t rn = REFS[i].n, n = NEXTS[i].n; memcpy_P(ref, REFS[i].p, rn); memcpy_P(text, NEXTS[i].p, n);
            uint32_t t0 = micros(); size_t fl = AprsCompress::encodePrimed(ref, rn, text, n, frame, sizeof frame); uint32_t t1 = micros();
            if (!fl) { skP++; continue; }
            size_t dn = AprsCompress::decodePrimed(ref, rn, frame, fl, back, sizeof back); uint32_t t2 = micros();
            if (dn != n || memcmp(back, text, n)) bad++;
            encP.push_back((t1 - t0) / 1000.0f); decP.push_back((t2 - t1) / 1000.0f); bytesP += fl;
        }
    }
    stats("encode_full", encF, bytesF / (encF.size() ? encF.size() : 1)); stats("decode_full", decF);
    stats("encode_primed", encP, bytesP / (encP.size() ? encP.size() : 1)); stats("decode_primed", decP);
    Serial.printf("RESULT checks mismatches=%d declined_full=%d declined_primed=%d heap_free=%u min_heap=%u\n", bad, skF, skP, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    Serial.println("BENCH done");
}
void loop() { delay(1000); }
