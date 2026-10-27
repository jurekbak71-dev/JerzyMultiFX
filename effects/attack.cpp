#include "core/Latency.h"
#include "core/Model.h"
#include "core/jerzy_module.h"
#include "vendor/attack/Engine.h"
#include <memory>
namespace {
using P = mf_attack::Parameters;
struct State {
    mf_attack::Engine engine;
    float expression = -1, previous = 1;
};
static_assert(mf_attack::Engine::latency == multifx::attackLatency);
constexpr float P::*floats[] = {&P::attack,   &P::decay, &P::blend, &P::volume, &P::sens,
                                &P::harmonix, &P::tone,  &P::hvol,  &P::hblend, &P::rblend};
constexpr bool P::*bools[] = {&P::poly,           &P::fuzz,           &P::compMono, &P::compPoly,
                              &P::continuousMono, &P::continuousPoly, &P::bypass,   &P::useReturn,
                              &P::returnBypass,   &P::fuzzOnReturn};
void *create(double sr, uint32_t, const char *) {
    auto s = std::make_unique<State>();
    s->engine.prepare(float(sr));
    return s.release();
}
void destroy(void *p) {
    delete static_cast<State *>(p);
}
void process(void *p, const JAProcess *b) {
    auto &state = *static_cast<State *>(p);
    P v;
    const auto &layout = multifx::layout();
    for (unsigned i = 0; i < 10; ++i)
        v.*floats[i] = b->parameters[i];
    for (unsigned i = 0; i < 10; ++i)
        v.*bools[i] = b->parameters[10 + i] >= .5f;
    v.attackShape = int(b->parameters[20] * 2 + .5f);
    v.decayShape = int(b->parameters[21] * 2 + .5f);
    v.triggerMode = int(b->parameters[22] * 2 + .5f);
    v.useReturn = v.useReturn && b->return_input[0];
    if (b->control_parameters && b->control_parameters[layout.attackExpression] != state.previous) {
        state.previous = b->control_parameters[layout.attackExpression];
        state.expression = -1;
    }
    const P base = v;
    for (unsigned i = 0; i < b->frames; ++i) {
        v = base;
        float l = b->input[0][i], r = b->input[1][i];
        bool trigger = false;
        for (unsigned m = 0; m < b->midi_count; ++m)
            if (b->midi[m].frame == i) {
                const auto &e = b->midi[m];
                if (((e.bytes[0] & 240) == 144 && e.bytes[2]) || (e.bytes[0] == 248 && v.triggerMode == 2))
                    trigger = true;
                if ((e.bytes[0] & 240) == 176 && e.bytes[1] == 11)
                    state.expression = float(e.bytes[2]) / 127;
            }
        if (b->control_parameters && b->control_parameters[layout.attackExpressionEnabled] >= .5f) {
            const float position =
                state.expression >= 0 ? state.expression : b->control_parameters[layout.attackExpression];
            float *targets[]{&v.volume, &v.blend,    &v.attack, &v.decay,
                             &v.sens,   &v.harmonix, &v.tone,   &v.hvol};
            for (int j = 0; j < 8; ++j) {
                const auto &ids = layout.attackExpressionMap[j];
                if (b->control_parameters[ids[0]] >= .5f)
                    *targets[j] = b->control_parameters[ids[1]] +
                                  position * (b->control_parameters[ids[2]] - b->control_parameters[ids[1]]);
            }
        }
        const float retL = b->return_input[0] ? b->return_input[0][i] : 0,
                    retR = b->return_input[1] ? b->return_input[1][i] : retL;
        state.engine.process(l, r, retL, retR, v, trigger);
        b->output[0][i] = l;
        b->output[1][i] = r;
    }
}
} // namespace
extern "C" const JAModule *module_attack() {
    static const JAParameter p[] = {
#define P(k, n, d) {k, n, "normalized", "", 0, 1, d, 0}
        P("attack", "Attack", float(std::log(.65 / .004) / std::log(2000.))),
        P("decay", "Decay", 1),
        P("blend", "Blend", 1),
        P("volume", "Volume", 26.f / 39),
        P("sens", "Sensitivity", .5f),
        P("harmonix", "Harmonix", .3f),
        P("tone", "Tone", .5f),
        P("hvol", "Harmonix Volume", .25f),
        P("hblend", "Harmonix Blend", 1),
        P("rblend", "Return Blend", 1),
        P("poly", "Poly", 0),
        P("fuzz", "Fuzz", 0),
        P("compMono", "Mono Comp", 1),
        P("compPoly", "Poly Comp", 1),
        P("continuousMono", "Continuous Mono", 0),
        P("continuousPoly", "Continuous Poly", 0),
        P("bypass", "Bypass", 0),
        P("useReturn", "Use Return (unconnected: silence)", 0),
        P("returnBypass", "Return Bypass", 0),
        P("fuzzOnReturn", "Fuzz on Return", 0),
        P("attackShape", "Attack Shape", .5f),
        P("decayShape", "Decay Shape", .5f),
        P("triggerMode", "Trigger Auto/Manual/MIDI", 0)
#undef P
    };
    static const JAModule m{
        sizeof(JAModule), JA_ABI,  "attack", "Attack Decay",
        "0.1.0",          0,       23,       p,
        create,           destroy, process,  [](void *) -> uint32_t {return multifx::attackLatency; }};
    return &m;
}
