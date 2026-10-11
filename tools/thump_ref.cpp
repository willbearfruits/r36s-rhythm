// Reference renders from thump's own C++ engine, to check the C port against: every factory drum, raw (no level),
// one hit at 48 kHz, velocity 1, seed 1, in blocks of 256, as 32-bit float mono files. Also prints thump's level for
// each drum (normalizeLevel), which core/drum_levels.c bakes in.
// Build: make ref   (reads ~/Projects/thump/src/dsp, writes build/ref/)
#include "Presets.h"
#include "Render.h"
#include "Slot.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace thump;

int main(int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : "build/ref";
    const auto& list = drumPresets();
    std::FILE* lv = std::fopen((out + "/levels.txt").c_str(), "w");
    for (size_t i = 0; i < list.size(); ++i) {
        Values v = list[i].values();
        v[pPan] = 0;
        Slot s;
        s.prepare(48000, 256);
        s.setValues(v);
        s.trigger(1.0f, 1);
        std::vector<float> L(256), R(256), all;
        const int maxN = 48000 * 4;
        while ((int) all.size() < maxN) {
            std::fill(L.begin(), L.end(), 0.0f);
            std::fill(R.begin(), R.end(), 0.0f);
            s.render(L.data(), R.data(), 256);
            all.insert(all.end(), L.begin(), L.end());
            if (!s.active()) break;
        }
        char name[64];
        std::snprintf(name, sizeof name, "%s/%02zu.f32", out.c_str(), i);
        std::FILE* f = std::fopen(name, "wb");
        std::fwrite(all.data(), sizeof(float), all.size(), f);
        std::fclose(f);
        const Values lvl = normalizeLevel(list[i].values());
        std::fprintf(lv, "%.1f\n", lvl[pLevel]);
    }
    std::fclose(lv);
    std::printf("%zu drums -> %s\n", list.size(), out.c_str());
    return 0;
}
