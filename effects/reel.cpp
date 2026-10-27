#include "core/jerzy_module.h"
#include "vendor/reel/TapeEngine.h"
#include <algorithm>
#include <memory>
#include <vector>
namespace {
void *create(double sr, uint32_t, const char *) {
    auto p = std::make_unique<mf_reel::TapeEngine>();
    p->prepare(sr);
    return p.release();
}
void destroy(void *p) {
    delete static_cast<mf_reel::TapeEngine *>(p);
}
void process(void *p, const JAProcess *b) {
    mf_reel::Values v{};
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = b->parameters[i];
    auto &e = *static_cast<mf_reel::TapeEngine *>(p);
    e.set(v, b->bpm);
    for (int c = 0; c < 2; ++c)
        std::copy_n(b->input[c], b->frames, b->output[c]);
    e.process(b->output[0], b->output[1], int(b->frames));
}
} // namespace
extern "C" const JAModule *module_reel() {
    static const auto p = []() {
        std::vector<JAParameter> out;
        for (auto &d : mf_reel::descriptors)
            out.push_back({d.id, d.name, d.unit, "", d.min, d.max, d.initial, 0});
        return out;
    }();
    static const JAModule m{
        sizeof(JAModule),   JA_ABI,   "reel", "Reel Echo", "0.1.0", 0,
        uint32_t(p.size()), p.data(), create, destroy,     process, [](void *) -> uint32_t {return 0; }};
    return &m;
}
