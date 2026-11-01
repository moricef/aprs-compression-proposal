// On-air test of the spec 11 hybrid frame (tools/hb) on a LILYGO LoRa32 v2.1 (SX1278, 433.775 MHz, SF12, BW 125, CR 4/5, sync 0x12 as LoRa APRS).
// Every 45 s it sends one hybrid test frame from K6ATV-14 (callsign of this board) and listens in between. Whatever it hears is printed:
// a hybrid frame as decoded TNC2 (and its RXT tuples), a plain text frame as text, anything else as hex.
#include <Arduino.h>
#include <RadioLib.h>
#include "hb.h"
#include "set1.h"

static SX1278 radio = new Module(18, 26, 14);                 // CS, DIO0, RST
static const float FREQ = 433.775f;
static const int TX_POWER = 10;                               // dBm, PA_BOOST
static uint32_t seq = 0;

static void hexdump(const uint8_t *b, size_t n) { for (size_t i = 0; i < n; i++) Serial.printf("%02x", b[i]); }

static void send_test() {
    char tnc2[200]; uint8_t fr[256]; hb_rxt_t rx; hb_info_t info; const hb_rxt_t *prx = nullptr;
    seq++;
    if (seq % 3 == 0) { memset(&rx, 0, sizeof rx); rx.n = 1; rx.idx[0] = 0; rx.t[0][0] = 50; rx.t[0][1] = 20; rx.t[0][2] = 45; rx.t[0][3] = 10; prx = &rx; }   // every third frame carries an RXT tuple
    if (seq % 3 == 2) snprintf(tnc2, sizeof tnc2, "K6ATV-14>APLRT1,WIDE1-1:>hybrid test %lu, LoRa APRS iGate on the hill", (unsigned long)seq);
    else snprintf(tnc2, sizeof tnc2, "K6ATV-14>APLRT1,N0RLY-1*,WIDE2-1:!3722.40N/12158.93W>hybrid test %lu LoRa APRS iGate on the hill", (unsigned long)seq);
    size_t n = hb_encode(tnc2, strlen(tnc2), prx, fr, sizeof fr, &info);
    if (!n) { Serial.printf("TX %lu: not encodable\n", (unsigned long)seq); return; }
    Serial.printf("TX %lu: %s\n    %u B hybrid (plain text frame would be %u B), kind %d, cc %d%s: ", (unsigned long)seq, tnc2, (unsigned)n, (unsigned)(strlen(tnc2) + 3), info.kind, info.cc, prx ? ", with RXT" : "");
    hexdump(fr, n); Serial.println();
    int st = radio.transmit(fr, n);
    Serial.printf("    transmit state %d\n", st);
    radio.startReceive();
}

static void report_rx() {
    uint8_t buf[256]; size_t len = radio.getPacketLength(); if (len > 255) len = 255;
    int st = radio.readData(buf, len);
    Serial.printf("RX %u B state %d rssi %.0f snr %.1f fo %.0f Hz: ", (unsigned)len, st, radio.getRSSI(), radio.getSNR(), radio.getFrequencyError());
    if (st != RADIOLIB_ERR_NONE) { Serial.println("(read error)"); return; }
    if (len > 3 && buf[0] == 0x3C && buf[1] == 0xFF && buf[2] == 0x02 && (buf[3] & 3) >= 2) {
        char tn[700]; hb_info_t in; int r = hb_decode(buf, len, tn, sizeof tn, &in);
        if (r > 0) {
            Serial.printf("HYBRID %s", tn);
            if (in.has_rxt) { Serial.printf("  [RXT"); for (int i = 0; i < in.rxt.n; i++) Serial.printf(" %d:%u/%u/%u/%u", in.rxt.idx[i], in.rxt.t[i][0], in.rxt.t[i][1], in.rxt.t[i][2], in.rxt.t[i][3]); Serial.printf("]"); }
            Serial.println();
        } else Serial.printf("hybrid frame dropped: %s\n    ", hb_reason(r)), hexdump(buf, len), Serial.println();
    } else if (len > 3 && buf[0] == 0x3C && buf[1] == 0xFF && buf[2] == 0x01) {
        buf[len] = 0; Serial.printf("TEXT %s\n", (char *)buf + 3);
    } else { hexdump(buf, len); Serial.println(); }
    radio.startReceive();
}

static void radio_task(void *) {
    static uint32_t last = 0;
    for (;;) {
        if (digitalRead(26)) report_rx();                      // DIO0 high: packet received
        if (millis() - last > 45000) { last = millis(); send_test(); }
        delay(5);
    }
}

void setup() {
    Serial.begin(115200); delay(1500);
    SPI.begin(5, 19, 27);
    int st = radio.begin(FREQ);
    if (st == RADIOLIB_ERR_NONE) st = radio.setSpreadingFactor(12);
    if (st == RADIOLIB_ERR_NONE) st = radio.setBandwidth(125.0);
    if (st == RADIOLIB_ERR_NONE) st = radio.setCodingRate(5);
    if (st == RADIOLIB_ERR_NONE) st = radio.setSyncWord(0x12);
    if (st == RADIOLIB_ERR_NONE) st = radio.setCRC(true);
    if (st == RADIOLIB_ERR_NONE) st = radio.setOutputPower(TX_POWER);
    if (st != RADIOLIB_ERR_NONE) { Serial.printf("radio setup failed %d\n", st); for (;;) delay(1000); }
    if (hb_set_list(SET1, SET1_N) != SET1_N) { Serial.println("destination list failed"); for (;;) delay(1000); }
    Serial.printf("hybrid on-air test: %.3f MHz SF12 BW125 CR4/5, %d dBm; sends every 45 s from K6ATV-14\n", FREQ, TX_POWER);
    radio.startReceive();
    xTaskCreatePinnedToCore(radio_task, "radio", 24576, nullptr, 1, nullptr, 1);   // the frame code needs about 10 KB of stack; the loop task has 8 KB
}

void loop() { delay(10000); }
