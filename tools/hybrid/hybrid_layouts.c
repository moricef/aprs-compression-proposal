/* Size comparison of three frame layouts, measured with the real reference codec (reference/aprs_ppm.c) and a model table.
 *
 *   hybrid_layouts TSV [N...]     N = full frame every Nth transmission per station (1 = full frames only); default 2
 *
 * Layouts (see README.md):
 *   0 whole packet   PPM of "SRC>DST[,PATH]:INFO"                       (APRS-PPM v2.0.0)
 *   1 hybrid A       native path block + PPM of "SRC>DST:INFO"            (the relay edits the path block, copies the PPM bytes)
 *   2 hybrid B       native source, destination code, path block + PPM of ":INFO"
 * Run each layout with a model trained for it (tools/hybrid/train_layout_model.py --mode whole|nopath|info).
 *
 * Environment (all optional):
 *   LAYOUTS=mask   bit 0 whole, bit 1 hybrid A, bit 2 hybrid B (default 7 = all)
 *   PACKPATH=0     old path cost (2 + text length) instead of packed callsign elements
 *   NATIVE=1       cost source, destination and path exactly as moricef's native encoder does (moricef/LoRa_APRS_iGate,
 *                  branch rxt, src/native_aprs.cpp, encElement): no path code table, path block = 1 header byte + its elements, destination as an element (DSTCODE=1 keeps the code table)
 *   DUMPCOST=1     print "source destination path" native bytes for each row and exit (to check against the real encoder)
 *   NOHOP=1        skip the "after one relay hop" pass
 *   RXT=R          each of R relays adds one 4-byte RXT tuple, plus one mask byte once (relayed frames only; default 0)
 *   RXTFULL=1      add the RXT bytes to full frames only, not to primed frames
 *
 * Every frame is decoded by a receiver holding only what it received and compared with the original ("decode failures" must be 0).
 * Bytes are counted from the frame the codec produces (it includes the 3-byte radio prefix) plus the native bytes of the layout.
 * Plain frames are the text plus the 3-byte prefix; a text the codec declines to shrink goes out as a plain frame. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "aprs_ppm.h"

#define MAXROWS 400000
typedef struct { char src[16]; int rf; char kind; char path[96]; uint8_t text[256]; int tl; } Row;
static Row *rows; static int nrows;

static const char *CODES[] = { "", "WIDE1-1", "WIDE1-1,WIDE2-1", "WIDE2-1", "WIDE2-2", "WIDE1-1,WIDE2-2", "TCPIP*", NULL };
static const char *DCODES[] = { "APLRG1","APLRT1","APLWS2","APLRFD","APLC13","APLOX1","APLT00","APLRFT","APLHM0","APLETK","APLC12","APLG01","APLHI0","APL",NULL };
static int dst_code(const char *d){ for(int i=0;DCODES[i];i++) if(!strcmp(DCODES[i],d)) return i; return -1; }
/* A callsign packs into 5 bytes when its base is 1 to 6 letters or digits and its SSID is absent or a number 0 to 15 (moricef's encoder keeps other SSIDs as text). */
static int call_packable(const char *s, int l) {
    int b = 0; while (b < l && s[b] != '-') b++;
    if (b < 1 || b > 6) return 0;
    for (int i = 0; i < b; i++) { char c = s[i]; if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return 0; }
    if (b == l) return 1;
    int v = 0, n = l - b - 1; if (n < 1 || n > 2) return 0;
    for (int i = b + 1; i < l; i++) { if (s[i] < '0' || s[i] > '9') return 0; v = v * 10 + s[i] - '0'; }
    return v <= 15;
}
static int src_packable(const char *s, int l) { return call_packable(s, l); }
/* Native path block for a path outside the code table: one header byte, then per element 1 byte for WIDEn-N, 5 bytes for a packable callsign
   (the used mark * is one of its bits), otherwise 1 + the element's text length. PACKPATH=0 restores the old 2 + text length. */
static int PACKPATH = 1;
static int path_native(const char *p) {
    if (!PACKPATH) return 2 + (int)strlen(p);
    int tot = 1; const char *e = p;
    while (*e) {
        const char *c = strchr(e, ','); int l = c ? (int)(c - e) : (int)strlen(e);
        if (l >= 7 && !strncmp(e, "WIDE", 4) && e[4] >= '1' && e[4] <= '7' && e[5] == '-' && l == 7) tot += 1;
        else { int cl = l; if (cl && e[cl - 1] == '*') cl--; tot += call_packable(e, cl) ? 5 : 1 + l; }
        e += l; if (*e == ',') e++;
    }
    return tot;
}
static int path_code(const char *p) { for (int i = 0; CODES[i]; i++) if (!strcmp(CODES[i], p)) return i; return -1; }

/* NATIVE=1: one address element as moricef's encoder writes it (encElement in moricef/LoRa_APRS_iGate, branch rxt, src/native_aprs.cpp).
   WIDEn[-N][*] with n, N 1..7: 1 byte. 1 to 6 of A-Z0-9, optional -SSID 1..255 without a leading zero, optional *: 5 bytes,
   6 when the SSID is above 30. Anything else: 1 + text length, up to 31 characters. Returns -1 for an element it cannot carry. */
static int NATIVE = 0, DSTCODE = 0;
static int elem_native(const char *e, int l) {
    int i = 5;
    if (l >= 5 && !strncmp(e, "WIDE", 4) && e[4] >= '1' && e[4] <= '7') {
        if (i < l && e[i] == '-') i = (i + 1 < l && e[i + 1] >= '1' && e[i + 1] <= '7') ? i + 2 : -1;
        if (i > 0 && i < l && e[i] == '*') i++;
        if (i == l) return 1;
    }
    i = 0; while (i < l && i < 6 && ((e[i] >= 'A' && e[i] <= 'Z') || (e[i] >= '0' && e[i] <= '9'))) i++;
    if (i > 0) {
        int ssid = 0, ok = 1;
        if (i < l && e[i] == '-') {
            int j = i + 1; while (j < l && j < i + 4 && e[j] >= '0' && e[j] <= '9') { ssid = ssid * 10 + e[j] - '0'; j++; }
            if (j == i + 1 || e[i + 1] == '0' || ssid > 255) ok = 0; else i = j;
        }
        if (ok && i < l && e[i] == '*') i++;
        if (ok && i == l) return ssid > 30 ? 6 : 5;
    }
    return (l >= 1 && l <= 31) ? 1 + l : -1;
}
/* path block: one header byte (element count), then each element. -1 if an element cannot be carried or there are more than 15. */
static int path_block_native(const char *p) {
    int tot = 1, n = 0; const char *e = p;
    while (*e) {
        const char *c = strchr(e, ','); int l = c ? (int)(c - e) : (int)strlen(e);
        int b = elem_native(e, l); if (b < 0 || ++n > 15) return -1;
        tot += b; e += l; if (*e == ',') e++;
    }
    return tot;
}

/* LoRa airtime (s), explicit header, CRC, CR 4/5, 8-symbol preamble, low-data-rate-optimise at SF11/12 */
static double airtime2(int bytes, int sf) {
    double tsym = pow(2, sf) / 125000.0; int de = sf >= 11;
    double x = ceil((8.0 * bytes - 4 * sf + 28 + 16) / (4.0 * (sf - 2 * de)));
    if (x < 0) x = 0;
    return (8 + 4.25) * tsym + (8 + x * 5) * tsym;
}

/* per-station sender and receiver state: last full/plain text, count of transmissions */
typedef struct { char key[16]; int used; uint8_t stx[256]; int stl; int seq; uint8_t srx[256]; int srl; } St;
#define HT 65536
static St *tab;
static St *get(const char *k) { unsigned h = 5381; for (const char *p = k; *p; p++) h = h * 33 + (unsigned char)*p; h %= HT;
    while (tab[h].used && strcmp(tab[h].key, k)) h = (h + 1) % HT;
    if (!tab[h].used) { tab[h].used = 1; strncpy(tab[h].key, k, 15); } return &tab[h]; }

/* groups */
enum { G_ALL, G_RF, G_STAT, G_MOV, G_HOP, NG };
static const char *GN[NG] = { "all LoRa packets (held-out stations)", "heard over RF (qAR/qAO)", "stationary stations", "moving stations", "RF, path starts WIDE1-1 (hop subset)" };
typedef struct { long n; double plain, bytes, ap12, ac12, ap7, ac7; long primed, full, plainfr; } Acc;

/* relayed text: WIDE1-1 replaced by "DIGI*" (fill-in digipeater), rest unchanged. returns 1 if applicable */
static int hop(const Row *r, char *outpath) {
    if (!r->rf || strncmp(r->path, "WIDE1-1", 7)) return 0;
    snprintf(outpath, 96, "%s%s", "DIGI-10*", r->path + 7);
    return 1;
}
static int in_group(const Row *r, int g, int hopped) {
    switch (g) { case G_ALL: return 1; case G_RF: return r->rf; case G_STAT: return r->kind == 'S'; case G_MOV: return r->kind == 'M';
                 case G_HOP: return hopped; } return 0; }

/* returns frame bytes; layout 0 = whole packet, 1 = path outside. extra = path block bytes (layout 1). Verifies decode. */
static long fails, escapes, pathrows, unencodable; static int RXTH = 0, RXTFULL = 0; /* relays that each append a 4-byte tuple; +1 mask byte once */
static int send_one(int layout, int N, const Row *r, int hopped, const char *hp, int *kindout) {
    uint8_t t[300]; int tl; int extra = 0; char dst[64];
    const char *path = hopped ? hp : r->path;
    /* build the text this station-hop transmits */
    const char *gt = strchr((char *)r->text, '>'); const char *colon = strchr((char *)r->text, ':');
    int srcl = (int)(gt - (char *)r->text); const char *dstp = gt + 1;
    const char *c = dstp; while (*c && *c != ',' && *c != ':') c++;
    int dl = (int)(c - dstp); memcpy(dst, dstp, dl); dst[dl] = 0;
    const char *info = colon + 1;
    int pcode = path_code(r->path); pathrows++;
    int pathextra = hopped ? 6 : (pcode >= 0 ? 1 : path_native(path)); if (!hopped && pcode < 0) escapes++;
    if (NATIVE) { pathextra = path_block_native(path); if (pathextra < 0) { unencodable++; return -1; } }
    if (layout == 0) { tl = snprintf((char *)t, 300, "%.*s>%s%s%s:%s", srcl, r->text, dst, *path ? "," : "", path, info); }
    else if (layout == 1) { tl = snprintf((char *)t, 300, "%.*s>%s:%s", srcl, r->text, dst, info); extra = pathextra; }
    else { tl = snprintf((char *)t, 300, ":%s", info);               /* leading ':' so the model sees a familiar start */
           int dc = dst_code(dst);
           if (NATIVE) {
               int sb = elem_native((const char *)r->text, srcl), db = DSTCODE && dc >= 0 ? 1 : elem_native(dst, dl);
               if (sb < 0 || db < 0) { unencodable++; return -1; }
               extra = sb + db + pathextra;
           } else
           extra = (src_packable(r->text, srcl) ? 5 : 1 + srcl) + (dc >= 0 ? 1 : 1 + (int)strlen(dst)) + pathextra; }
    if (tl > 255) return -1;
    St *s = get(r->src);
    uint8_t f[400], back[400]; int fl = 0, kind = 0; size_t n;
    int wantfull = (s->stl == 0) || (N <= 1) || (s->seq % N == 0);
    s->seq++;
    if (!wantfull) {
        n = aprs_ppm_encode_primed(s->stx, s->stl, t, tl, f, sizeof f);
        if (n) { fl = (int)n; kind = 1; } else wantfull = 1;
    }
    if (wantfull) {
        n = aprs_ppm_encode_frame(t, tl, f, sizeof f);
        if (n) { fl = (int)n; kind = 0; } else { memcpy(f, "\x3c\xff\x01", 3); memcpy(f + 3, t, tl); fl = 3 + tl; kind = 2; }
        memcpy(s->stx, t, tl); s->stl = tl;                     /* a full or plain frame becomes the reference */
    }
    /* receiver */
    if (kind == 2) { memcpy(back, f + 3, tl); n = tl; }
    else if (kind == 0) n = aprs_ppm_decode_frame(f, fl, back, sizeof back);
    else n = aprs_ppm_decode_primed(s->srx, s->srl, f, fl, back, sizeof back);
    if (n != (size_t)tl || memcmp(back, t, tl)) { fails++; return -1; }
    if (kind != 1) { memcpy(s->srx, back, n); s->srl = (int)n; }
    /* path block round trip (layout 1): code table must give the original path back */
    if (layout == 1 && !hopped && !NATIVE) { int code = path_code(r->path); if (code >= 0 && strcmp(CODES[code], r->path)) fails++; }
    *kindout = kind;
    if (layout == 2 && kind == 1) extra -= 1;                    /* station id not needed: source is readable */
    if (hopped && RXTH > 0 && (!RXTFULL || kind != 1)) extra += 4 * RXTH + 1;               /* RXT: one tuple per relay + mask byte */
    return fl + extra;
}

int main(int argc, char **argv) {
    if (getenv("PACKPATH")) PACKPATH = atoi(getenv("PACKPATH"));
    if (getenv("RXTFULL")) RXTFULL = atoi(getenv("RXTFULL"));
    if (getenv("RXT")) RXTH = atoi(getenv("RXT"));
    if (getenv("NATIVE")) NATIVE = atoi(getenv("NATIVE"));
    if (getenv("DSTCODE")) DSTCODE = atoi(getenv("DSTCODE"));
    FILE *f = fopen(argv[1], "r"); char line[1024]; rows = calloc(MAXROWS, sizeof(Row)); tab = calloc(HT, sizeof(St));
    while (fgets(line, sizeof line, f) && nrows < MAXROWS) {
        char *p[5]; char *q = line; int i;
        for (i = 0; i < 5; i++) { p[i] = q; q = strchr(q, '\t'); if (!q) break; *q++ = 0; if (i == 3) { p[4] = q; q = strchr(q, '\n'); if (q) *q = 0; i = 4; break; } }
        if (i < 4) continue;
        Row *r = &rows[nrows++]; strncpy(r->src, p[0], 15); r->rf = p[1][0] == '1'; r->kind = p[2][0]; strncpy(r->path, p[3], 95);
        r->tl = (int)strlen(p[4]); memcpy(r->text, p[4], r->tl + 1);
    }
    fclose(f);
    if (getenv("DUMPCOST")) {
        for (int i = 0; i < nrows; i++) {
            const char *t = (const char *)rows[i].text, *gt = strchr(t, '>'), *d = gt + 1, *c = d;
            while (*c && *c != ',' && *c != ':') c++;
            printf("%d %d %d\n", elem_native(t, (int)(gt - t)), elem_native(d, (int)(c - d)), path_block_native(rows[i].path));
        }
        return 0;
    }
    printf("%d packets\n", nrows);
    int Ns[8], nN = 0; for (int i = 2; i < argc; i++) Ns[nN++] = atoi(argv[i]); if (!nN) Ns[nN++] = 2;
    int lmask = getenv("LAYOUTS") ? atoi(getenv("LAYOUTS")) : 7, hpmax = getenv("NOHOP") ? 1 : 2;
    for (int hp_on = 0; hp_on < hpmax; hp_on++) {
      for (int ni = 0; ni < nN; ni++) for (int layout = 0; layout < 3; layout++) { if (!(lmask >> layout & 1)) continue;
        Acc a[NG]; memset(a, 0, sizeof a); memset(tab, 0, sizeof(St) * HT); fails = 0; escapes = 0; pathrows = 0; unencodable = 0;
        for (int i = 0; i < nrows; i++) {
            Row *r = &rows[i]; char hpth[96]; int hopped = hp_on ? hop(r, hpth) : 0; int kind = 0;
            int fl = send_one(layout, Ns[ni], r, hopped, hpth, &kind);
            if (fl < 0) continue;
            for (int g = 0; g < NG; g++) if (in_group(r, g, hopped)) {
                int plain = 3 + r->tl + 0;
                a[g].n++; a[g].plain += plain; a[g].bytes += fl; a[g].ap12 += airtime2(plain, 12); a[g].ac12 += airtime2(fl, 12);
                a[g].ap7 += airtime2(plain, 7); a[g].ac7 += airtime2(fl, 7);
                if (kind == 1) a[g].primed++; else if (kind == 0) a[g].full++; else a[g].plainfr++;
            }
        }
        printf("\n%s, N=%d, %s   (decode failures: %ld%s)\n", layout == 2 ? "HYBRID B (native src,dst,path; PPM info)" : layout ? "HYBRID A (native path; PPM src,dst,info)" : "WHOLE PACKET ", Ns[ni], hp_on ? "after one relay hop (where applicable)" : "heard direct", fails,
               layout ? "" : "");
        for (int g = 0; g < NG; g++) { if (hp_on == 0 && g == G_HOP) continue; if (!a[g].n) continue;
            printf("  %-40s n=%6ld  avg %5.1f B (plain %5.1f)  saved %5.1f%%  SF12 airtime saved %5.1f%%  SF7 %5.1f%%  primed %4.1f%%\n",
                GN[g], a[g].n, a[g].bytes / a[g].n, a[g].plain / a[g].n, 100 * (1 - a[g].bytes / a[g].plain),
                100 * (1 - a[g].ac12 / a[g].ap12), 100 * (1 - a[g].ac7 / a[g].ap7), 100.0 * a[g].primed / a[g].n); }
        if (layout == 1) printf("  path block: %ld rows, %ld needed the escape (%.1f%%)\n", pathrows, escapes, 100.0 * escapes / (pathrows ? pathrows : 1));
        if (NATIVE && unencodable) printf("  skipped: %ld rows the native encoder cannot carry\n", unencodable);
      }
    }
    return 0;
}
