#include "core/jerzy_module.h"
#include "vendor/clone/Dsp.h"
#include <memory>
using namespace mf_clone;
static void *create(double sr, uint32_t, const char *) {
    try {
        auto s = std::make_unique<Engine>();
        s->reset(sr, defaults());
        return s.release();
    } catch (...) {
        return nullptr;
    }
}
static void destroy(void *s) {
    delete static_cast<Engine *>(s);
}
static void process(void *s, const JAProcess *b) {
    Values p;
    for (int i = 0; i < NumParams; ++i)
        p[i] = b->parameters[i];
    for (uint32_t i = 0; i < b->frames; ++i) {
        double beat = b->playing ? b->beat + (double(i)) * b->bpm / (60 * b->sample_rate) : NAN;
        auto y = static_cast<Engine *>(s)->process(b->input[0][i], b->input[1][i], p, b->bpm, beat);
        for (int c = 0; c < 2; ++c)
            b->output[c][i] = float(y[c]);
    }
}
static uint32_t latency(void *) {
    return 0;
}
extern "C" const JAModule *module_clone() {
    static const auto defaults = mf_clone::defaults();
    static const JAParameter params[] = {
        {"rate", "Tempo LFO", "0–1", "", 0, 1, float(defaults[0]), 0},
        {"depth", "Głębokość", "0–1", "", 0, 1, float(defaults[1]), 0},
        {"mix", "Proporcja efektu", "0–1", "", 0, 1, float(defaults[2]), 0},
        {"color", "Barwa i sprzężenie", "0–1", "", 0, 1, float(defaults[3]), 0},
        {"level", "Poziom", "0–1", "", 0, 1, float(defaults[4]), 0},
        {"model", "Model obwodu", "0–1", "", 0, 1, float(defaults[5]), 0},
        {"stereo", "Szerokość stereo", "0–1", "", 0, 1, float(defaults[6]), 0},
        {"filter", "Zatrzymanie modulacji", "0–1", "", 0, 1, float(defaults[7]), 0},
        {"bypass", "Bypass oryginału", "0–1", "", 0, 1, float(defaults[8]), 0},
        {"sync", "Synchronizacja tempa", "0–1", "", 0, 1, float(defaults[9]), 0},
        {"division", "Podział rytmiczny", "0–1", "", 0, 1, float(defaults[10]), 0}};
    static const JAModule m{sizeof(JAModule),
                            JA_ABI,
                            "small-clone-original",
                            "Jerzy small clone — oryginalny DSP",
                            "0.2.1",
                            0,
                            NumParams,
                            params,
                            create,
                            destroy,
                            process,
                            latency};
    return &m;
}
