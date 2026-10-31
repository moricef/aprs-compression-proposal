#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "AprsCompress.h"
extern size_t g_overread;
int main(int argc, char** argv) {
    std::vector<std::string> texts; char line[400]; FILE* f = fopen(argv[1], "rb");
    while (fgets(line, sizeof line, f)) { std::string s(line); while (!s.empty() && s.back() == '\n') s.pop_back(); if (s.size() >= 20) texts.push_back(s); }
    fclose(f);
    std::map<size_t, long> intact; long n = 0; size_t maxi = 0;
    std::vector<std::vector<size_t>> tr(8); const int ks[8] = { 1, 2, 3, 4, 6, 8, 12, 16 };
    for (auto& t : texts) {
        uint8_t fr[300], out[300]; size_t fl = AprsCompress::encodeFrame((const uint8_t*)t.data(), t.size(), fr, sizeof fr); if (!fl) continue;
        n++; if (AprsCompress::decodeFrame(fr, fl, out, 255) == 0) continue; intact[g_overread]++; if (g_overread > maxi) maxi = g_overread;
        for (int a = 0; a < 8; a++) if ((long)fl - ks[a] >= 5 && AprsCompress::decodeFrame(fr, fl - ks[a], out, 255)) tr[a].push_back(g_overread);
    }
    printf("intact full frames: %ld.  Bytes the decoder asks for beyond the end of an INTACT frame (the encoder's short ending relies on zero padding):\n", n);
    for (auto& kv : intact) printf("   %zu byte%s beyond: %ld frames (%.2f%%)\n", kv.first, kv.first == 1 ? "" : "s", kv.second, 100.0 * kv.second / n);
    printf("\nrule \"reject if the decoder asks for more than %zu bytes beyond the end\" (zero false rejections of intact frames by construction):\n", maxi);
    for (int a = 0; a < 8; a++) { long det = 0; for (size_t v : tr[a]) if (v > maxi) det++; printf("   frame cut by %2d byte%s: rejected %5.1f%% of %zu\n", ks[a], ks[a] > 1 ? "s" : " ", tr[a].empty() ? 0.0 : 100.0 * det / tr[a].size(), tr[a].size()); }
    for (size_t T = 0; T < maxi; T++) { long fr_ = 0; for (auto& kv : intact) if (kv.first > T) fr_ += kv.second; printf("   (a stricter rule at %zu would also reject %.2f%% of INTACT frames)\n", T, 100.0 * fr_ / n); }
}
