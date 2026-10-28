/* Mixed direct + relayed reception: a receiver that hears a station directly AND through a relay.
 *   mixed TSV N pd ph pr      pd = receiver hears the originator, ph = relay hears it, pr = receiver hears the relay
 * Whole packet: the relay decodes, edits the path, re-encodes with its own priming state (as the iGate firmware does).
 * Hybrid: path in a native block, PPM on SRC>DST:INFO; the relay copies the PPM bytes and rewrites the path block.
 * The receiver keeps ONE reference per source (the last full or plain frame it decoded), as AprsCompressStore does. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "aprs_ppm.h"

typedef struct { char src[16]; int rf; char path[96]; uint8_t text[256]; int tl; } Row;
static Row *rows; static int nrows;

typedef struct { char key[16]; int used;
    uint8_t otx[256]; int otl; int oseq;            /* originator sender reference */
    uint8_t rrx[256]; int rrl;                      /* relay receive reference (whole packet only) */
    uint8_t rtx[256]; int rtl; int rseq;            /* relay sender reference (whole packet only) */
    uint8_t rcv[256]; int rcl;                      /* receiver reference */
} St;
#define HT 65536
static St *tab;
static St *get(const char *k) { unsigned h = 5381; for (const char *p = k; *p; p++) h = h * 33 + (unsigned char)*p; h %= HT;
    while (tab[h].used && strcmp(tab[h].key, k)) h = (h + 1) % HT;
    if (!tab[h].used) { tab[h].used = 1; strncpy(tab[h].key, k, 15); } return &tab[h]; }

static double urand(int i, int link) { uint32_t x = (uint32_t)i * 2654435761u ^ (uint32_t)link * 40503u * 2246822519u; x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16; return (x & 0xFFFFFF) / 16777216.0; }

/* sender: next frame for text t using ref/seq; returns length, sets kind (0 full 1 primed 2 plain), updates ref */
static int send_frame(uint8_t *ref, int *rl, int *seq, int N, const uint8_t *t, int tl, uint8_t *f, int *kind, uint8_t *refused, int *refusedl) {
    int wantfull = (*rl == 0) || (N <= 1) || (*seq % N == 0); size_t n; (*seq)++;
    if (!wantfull) { n = aprs_ppm_encode_primed(ref, *rl, t, tl, f, 400); if (n) { *kind = 1; memcpy(refused, ref, *rl); *refusedl = *rl; return (int)n; } }
    n = aprs_ppm_encode_frame(t, tl, f, 400);
    if (n) *kind = 0; else { memcpy(f, "\x3c\xff\x01", 3); memcpy(f + 3, t, tl); n = 3 + tl; *kind = 2; }
    memcpy(ref, t, tl); *rl = tl; return (int)n;
}
/* receiver: decode with its reference; returns 1 ok, 0 dropped (no/wrong reference), -1 wrong text */
static int recv_frame(uint8_t *ref, int *rl, const uint8_t *f, int fl, int kind, const uint8_t *t, int tl) {
    uint8_t back[400]; size_t n;
    if (kind == 2) { memcpy(back, f + 3, tl); n = tl; }
    else if (kind == 0) n = aprs_ppm_decode_frame(f, fl, back, sizeof back);
    else { if (*rl == 0) return 0; n = aprs_ppm_decode_primed(ref, *rl, f, fl, back, sizeof back); if (!n) return 0; }
    if (n != (size_t)tl || memcmp(back, t, tl)) return -1;
    if (kind != 1) { memcpy(ref, back, n); *rl = (int)n; }
    return 1;
}

static int hop(const Row *r, char *outpath) { if (!r->rf || strncmp(r->path, "WIDE1-1", 7)) return 0; snprintf(outpath, 96, "DIGI-10*%s", r->path + 7); return 1; }
static int build(const Row *r, const char *path, int withpath, uint8_t *t) {
    const char *gt = strchr((char *)r->text, '>'), *colon = strchr((char *)r->text, ':'); int srcl = (int)(gt - (char *)r->text);
    const char *c = gt + 1; while (*c && *c != ',' && *c != ':') c++;
    return snprintf((char *)t, 300, "%.*s>%.*s%s%s:%s", srcl, r->text, (int)(c - gt - 1), gt + 1, withpath && *path ? "," : "", withpath ? path : "", colon + 1);
}

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r"); char line[1024]; int N = atoi(argv[2]); double pd = atof(argv[3]), ph = atof(argv[4]), pr = atof(argv[5]);
    rows = calloc(400000, sizeof(Row)); tab = calloc(HT, sizeof(St));
    while (fgets(line, sizeof line, f) && nrows < 400000) {
        char *p[5]; char *q = line; int i;
        for (i = 0; i < 5; i++) { p[i] = q; q = strchr(q, '\t'); if (!q) break; *q++ = 0; if (i == 3) { p[4] = q; q = strchr(q, '\n'); if (q) *q = 0; i = 4; break; } }
        if (i < 4) continue;
        char hp[96]; Row *r = &rows[nrows]; strncpy(r->src, p[0], 15); r->rf = p[1][0] == '1'; strncpy(r->path, p[3], 95);
        r->tl = (int)strlen(p[4]); memcpy(r->text, p[4], r->tl + 1);
        if (hop(r, hp)) nrows++;                        /* only packets a WIDE1-1 relay would repeat */
    }
    fclose(f);
    for (int layout = 0; layout < 2; layout++) {
        memset(tab, 0, sizeof(St) * HT);
        long heard_any = 0, delivered = 0, lost_ref = 0, relay_drop = 0, wrong = 0, orig_tx = 0, relay_tx = 0; double bytes = 0, plainb = 0;
        for (int i = 0; i < nrows; i++) {
            Row *r = &rows[i]; char hp[96]; hop(r, hp); St *s = get(r->src);
            uint8_t t[300], t2[300], fo[400], fr[400], ru[256], ru2[256]; int ru1 = 0, ru2l = 0, ko, kr = 0, tl, tl2;
            tl = build(r, r->path, layout == 0, t); tl2 = build(r, hp, layout == 0, t2);
            int flo = send_frame(s->otx, &s->otl, &s->oseq, N, t, tl, fo, &ko, ru, &ru1);
            int extra_o = layout ? 1 : 0; orig_tx++; bytes += flo + extra_o;
            plainb += 3 + r->tl;
            int got_direct = urand(i, 1) < pd, relay_hears = urand(i, 2) < ph, rcv_relay_hears = urand(i, 3) < pr, dl = 0, any_copy = 0;
            if (got_direct) { any_copy = 1; int k = recv_frame(s->rcv, &s->rcl, fo, flo, ko, t, tl); if (k == 1) dl = 1; else if (k < 0) wrong++; }
            if (relay_hears) {
                if (layout == 0) {                       /* relay decodes, edits, re-encodes */
                    int k = recv_frame(s->rrx, &s->rrl, fo, flo, ko, t, tl);
                    if (k == 1) {
                        int flr = send_frame(s->rtx, &s->rtl, &s->rseq, N, t2, tl2, fr, &kr, ru2, &ru2l); relay_tx++; bytes += flr;
                        if (rcv_relay_hears) { any_copy = 1; int k2 = recv_frame(s->rcv, &s->rcl, fr, flr, kr, t2, tl2); if (k2 == 1) dl = 1; else if (k2 < 0) wrong++; }
                    } else relay_drop++;                 /* relay lacks the reference: cannot forward */
                } else {                                 /* relay copies the PPM bytes, rewrites the path block (6 B) */
                    relay_tx++; bytes += flo + 6;
                    if (rcv_relay_hears) { any_copy = 1; int k2 = recv_frame(s->rcv, &s->rcl, fo, flo, ko, t, tl); if (k2 == 1) dl = 1; else if (k2 < 0) wrong++; }
                }
            }
            if (any_copy) { heard_any++; if (dl) delivered++; else lost_ref++; }
        }
        printf("%-13s N=%d pd=%.1f ph=%.1f pr=%.1f: packets %d | heard at least once %ld | decoded %ld (%.1f%%) | lost to missing reference %ld | relay could not forward %ld | wrong text %ld | tx: originator %ld, relay %ld | air bytes per packet sent %.1f (plain %.1f)\n",
               layout ? "HYBRID" : "WHOLE PACKET", N, pd, ph, pr, nrows, heard_any, delivered, 100.0 * delivered / (heard_any ? heard_any : 1), lost_ref, relay_drop, wrong, orig_tx, relay_tx,
               bytes / nrows, plainb / nrows);
    }
    return 0;
}
