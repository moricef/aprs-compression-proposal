/* The LACF ending and the receiver's re-encode check (spec/10). Include AFTER reference/aprs_ppm.c (it uses that file's static coder functions). */
/* --- spec/10 "Ending": the sender --- */
static void lacf_finish(encoder_t *e)
{
    uint64_t hi, v, blk; int nb, b;
    if (e->low >> 32) { enc_carry(e); e->low &= 0xFFFFFFFFull; }          /* 1. apply the pending carry */
    hi = e->low + e->range - 1;                                           /* 2. the final interval is [low, hi] */
    for (nb = 0; nb <= 4; nb++) {                                         /* 3. the fewest bytes nb for which EVERY continuation lands inside it */
        blk = 1ull << (8 * (4 - nb));                                     /*    the continuations of nb bytes fill a block of this many values */
        v = ((e->low + blk - 1) / blk) * blk;                             /*    the first block boundary at or above low */
        if (v + blk - 1 <= hi) {                                          /*    the whole block fits */
            if (v >> 32) { enc_carry(e); v &= 0xFFFFFFFFull; }
            for (b = 0; b < nb; b++) enc_put(e, (uint8_t)(v >> (24 - 8 * b)));
            break;                                                        /* 4. nothing is stripped: a zero last byte is sent */
        }
    }
}

/* --- spec/10 "Frame": 3C FF 81 <L> <crc16 low> <crc16 high> <payload>.  CRC-16/X-25 (the AX.25 frame check), over the L text bytes. --- */
#define LACF_HDR 6
static int lacf_crc_on = 1;                                               /* tests switch the CRC-16 off to measure the re-encode check alone */
static uint16_t lacf_crc16(const uint8_t *p, size_t n)
{
    uint16_t c = 0xFFFF; size_t i; int b;
    for (i = 0; i < n; i++) { c ^= p[i]; for (b = 0; b < 8; b++) c = (c & 1) ? (uint16_t)((c >> 1) ^ 0x8408) : (uint16_t)(c >> 1); }
    return (uint16_t)~c;
}

static size_t lacf_encode(const uint8_t *text, size_t tl, uint8_t *fr, size_t cap)
{
    encoder_t e; uint16_t freq[256], crc; size_t i, fl;
    if (!tl || tl > APRS_PPM_MAX_TEXT || cap < LACF_HDR + 1) return 0;
    e.out = fr + LACF_HDR; e.cap = cap - LACF_HDR; e.n = 0; e.overflow = 0; e.low = 0; e.range = 0xFFFFFFFFu;
    for (i = 0; i < tl; i++) {
        uint32_t total = build_freq(text, i, freq, NULL, 0), cum = 0; int s;
        for (s = 0; s < text[i]; s++) cum += freq[s];
        enc_symbol(&e, cum, freq[text[i]], total);
    }
    lacf_finish(&e);
    if (e.overflow) return 0;
    fl = LACF_HDR + e.n;
    if (fl >= 3 + tl) return 0;
    crc = lacf_crc16(text, tl);
    fr[0] = 0x3C; fr[1] = 0xFF; fr[2] = (uint8_t)(APRS_PPM_VERSION_FLAG | aprs_ppm_model_version_value); fr[3] = (uint8_t)tl;
    fr[4] = (uint8_t)(crc & 0xFF); fr[5] = (uint8_t)(crc >> 8);
    return fl;
}

/* Decode the payload only (no checks): the range decoder reads fr + LACF_HDR, zero-padded past the end. Returns the text length or 0. */
static size_t lacf_decode_raw(const uint8_t *fr, size_t fl, uint8_t *text)
{
    uint8_t tmp[420];
    if (fl < LACF_HDR || fl > sizeof tmp - 2 + LACF_HDR - 4 || fr[0] != 0x3C || fr[1] != 0xFF || fr[2] != (uint8_t)(APRS_PPM_VERSION_FLAG | aprs_ppm_model_version_value)) return 0;
    memcpy(tmp, fr, 4); memcpy(tmp + 4, fr + LACF_HDR, fl - LACF_HDR);       /* the reference decoder wants the 4 byte header */
    return aprs_ppm_decode_frame(tmp, fl - LACF_HDR + 4, text, 300);
}

/* The receiver as the spec states it: decode, check the CRC-16, re-encode, compare (two passes over the model). Returns the text length and sets *payload to the payload length, or returns 0 to drop. --- */
static size_t lacf_receive_two_pass(const uint8_t *fr, size_t fl, uint8_t *text, size_t *payload, int allow_tail)
{
    uint8_t again[400]; size_t tl = lacf_decode_raw(fr, fl, text), al;
    if (!tl) return 0;
    if (lacf_crc_on && lacf_crc16(text, tl) != (uint16_t)(fr[4] | (fr[5] << 8))) return 0;     /* the cheap check first */
    al = lacf_encode(text, tl, again, sizeof again);
    if (!al || al > fl || memcmp(again + LACF_HDR, fr + LACF_HDR, al - LACF_HDR)) return 0;     /* the re-encoding must be the start of what was received */
    if (!allow_tail && al != fl) return 0;
    *payload = al - LACF_HDR;
    return tl;
}


/* The same receiver in ONE pass: the re-encoder needs exactly the (cum, freq, total) the decoder just used for the same character of the same text, so the decoder
 * feeds the encoder as it goes and the model is evaluated once instead of twice. Same answers as lacf_receive_two_pass (tested on every case in lacf_ending.c). */
static size_t lacf_receive(const uint8_t *fr, size_t fl, uint8_t *text, size_t *payload, int allow_tail)
{
    decoder_t d; encoder_t e; uint16_t freq[256]; uint8_t out[400]; size_t i, tl, al;
    if (fl < LACF_HDR || fr[0] != 0x3C || fr[1] != 0xFF || fr[2] != (uint8_t)(APRS_PPM_VERSION_FLAG | aprs_ppm_model_version_value)) return 0;
    tl = fr[3];
    if (tl == 0 || tl > APRS_PPM_MAX_TEXT) return 0;
    dec_init(&d, fr + LACF_HDR, fl - LACF_HDR);
    e.out = out + LACF_HDR; e.cap = sizeof out - LACF_HDR; e.n = 0; e.overflow = 0; e.low = 0; e.range = 0xFFFFFFFFu;
    for (i = 0; i < tl; i++) {
        uint32_t total = build_freq(text, i, freq, NULL, 0), v = dec_target(&d, total), cum = 0; int s = 0;
        while (s < 255 && cum + freq[s] <= v) { cum += freq[s]; s++; }
        dec_consume(&d, cum, freq[s]);
        text[i] = (uint8_t)s;
        enc_symbol(&e, cum, freq[s], total);                               /* what the re-encoder would do for this character */
    }
    if (lacf_crc_on && lacf_crc16(text, tl) != (uint16_t)(fr[4] | (fr[5] << 8))) return 0;
    lacf_finish(&e);
    if (e.overflow) return 0;
    al = LACF_HDR + e.n;
    if (al >= 3 + tl || al > fl) return 0;                                   /* an encoder would not have sent this as a compressed frame */
    if (memcmp(out + LACF_HDR, fr + LACF_HDR, e.n)) return 0;
    if (!allow_tail && al != fl) return 0;
    *payload = e.n;
    return tl;
}
