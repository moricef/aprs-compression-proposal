/* APRS-PPM reference codec (draft). See spec/01-compressed-frames.md.
 *
 * Integer-only on purpose: every implementation must produce and consume exactly the same bits. */
#ifndef APRS_PPM_H
#define APRS_PPM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APRS_PPM_HEADER_0      0x3C
#define APRS_PPM_HEADER_1      0xFF
#define APRS_PPM_VERSION_FLAG  0x80   /* third header byte = 0x80 | model version */
#define APRS_PPM_MAX_TEXT      255

/* Model version this build holds (the low 7 bits of the third header byte). */
uint8_t aprs_ppm_model_version(void);

/* 1 if frame starts with the compressed-frame header for the model held here. */
int aprs_ppm_is_compressed_frame(const uint8_t *frame, size_t len);

/* Encode text (the packet without the 3-byte LoRa header) into a complete on-air frame.
 * Returns the frame length, or 0 if it does not fit or is not smaller than 3 + text_len
 * (the caller then sends the plain frame 3C FF 01 + text). */
size_t aprs_ppm_encode_frame(const uint8_t *text, size_t text_len, uint8_t *frame, size_t frame_cap);

/* Decode a complete on-air frame back to text. Returns the text length, or 0 on error. */
size_t aprs_ppm_decode_frame(const uint8_t *frame, size_t frame_len, uint8_t *text, size_t text_cap);

#ifdef __cplusplus
}
#endif
#endif
