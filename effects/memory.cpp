#include "core/jerzy_module.h"
#include "vendor/memory/dsp.h"
#include <memory>
static void *create(double sr, uint32_t, const char *) {
    try {
        auto s = std::make_unique<Jerzy::Engine>();
        s->prepare(sr);
        return s.release();
    } catch (...) {
        return nullptr;
    }
}
static void destroy(void *s) {
    delete static_cast<Jerzy::Engine *>(s);
}
static void process(void *s, const JAProcess *b) {
    Jerzy::Parameters p;
    for (int i = 0; i < Jerzy::Count; ++i)
        p.value[i] = b->parameters[i];
    for (uint32_t i = 0; i < b->frames; ++i) {
        double x[]{b->input[0][i], b->input[1][i]}, y[2];
        static_cast<Jerzy::Engine *>(s)->tick(x, y, 2, p, b->bpm);
        for (int c = 0; c < 2; ++c)
            b->output[c][i] = float(y[c]);
    }
}
static uint32_t latency(void *) {
    return 0;
}
extern "C" const JAModule *module_memory() {
    static const JAParameter p[] = {
        {"blend", "Proporcja efektu", "0–1", "", 0, 1, float(Jerzy::defaults[0]), 0},
        {"level", "Poziom", "0–1", "", 0, 1, float(Jerzy::defaults[1]), 0},
        {"feedback", "Sprzężenie", "0–1", "", 0, 1, float(Jerzy::defaults[2]), 0},
        {"modRate", "Tempo modulacji", "0–1", "", 0, 1, float(Jerzy::defaults[3]), 0},
        {"depth", "Głębokość", "0–1", "", 0, 1, float(Jerzy::defaults[4]), 0},
        {"delay", "Czas opóźnienia", "0–1", "", 0, 1, float(Jerzy::defaults[5]), 0},
        {"model", "Model", "0–1", "", 0, 1, float(Jerzy::defaults[6]), 0},
        {"wave", "Kształt modulacji", "0–1", "", 0, 1, float(Jerzy::defaults[7]), 0},
        {"bypass", "Bypass oryginału", "0–1", "", 0, 1, float(Jerzy::defaults[8]), 0},
        {"boost", "Wzmocnienie", "0–1", "", 0, 1, float(Jerzy::defaults[9]), 0},
        {"squelch", "Bramka", "0–1", "", 0, 1, float(Jerzy::defaults[10]), 0},
        {"modEnable", "Modulacja", "0–1", "", 0, 1, float(Jerzy::defaults[11]), 0},
        {"tempoSync", "Synchronizacja tempa", "0–1", "", 0, 1, float(Jerzy::defaults[12]), 0},
        {"division", "Podział rytmiczny", "0–1", "", 0, 1, float(Jerzy::defaults[13]), 0}};
    static const JAModule m{sizeof(JAModule),
                            JA_ABI,
                            "memory-delay-original",
                            "Jerzy Memory Delay — oryginalny DSP",
                            "0.2.1",
                            0,
                            Jerzy::Count,
                            p,
                            create,
                            destroy,
                            process,
                            latency};
    return &m;
}
