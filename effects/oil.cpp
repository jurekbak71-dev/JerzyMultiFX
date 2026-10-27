#include "core/jerzy_module.h"
#include "vendor/oil/Engine.h"
#include <memory>
namespace {
struct State {
    mf_oil::Engine engine;
    mf_oil::Settings settings;
};
void *create(double sr, uint32_t, const char *) {
    auto s = std::make_unique<State>();
    s->engine.prepare(sr);
    return s.release();
}
void destroy(void *p) {
    delete static_cast<State *>(p);
}
void process(void *p, const JAProcess *b) {
    auto &s = *static_cast<State *>(p);
    for (size_t i = 0; i < mf_oil::parameters().size(); ++i)
        s.settings.v[i] = b->parameters[i];
    for (unsigned c = 0; c < 2; ++c)
        std::copy_n(b->input[c], b->frames, b->output[c]);
    s.engine.process(b->output[0], b->output[1], int(b->frames), s.settings, b->bpm);
}
} // namespace
extern "C" const JAModule *module_oil() {
    static const auto p = []() {
        std::vector<JAParameter> out;
        for (auto &d : mf_oil::parameters())
            out.push_back({d.id, d.name, "", d.choices, d.min, d.max, d.def, 0});
        return out;
    }();
    static const JAModule m{
        sizeof(JAModule),   JA_ABI,   "oil",  "Oil Drum Echo", "0.1.0", 0,
        uint32_t(p.size()), p.data(), create, destroy,         process, [](void *) -> uint32_t {return 0; }};
    return &m;
}
