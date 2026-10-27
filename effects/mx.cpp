#include "core/jerzy_module.h"
#include "vendor/mx/mxdelay_dsp.h"
#include <memory>
using DSP = JerzyAudio::MXDelayDSP<float>;
static void *create(double sr, uint32_t, const char *) {
    try {
        auto s = std::make_unique<DSP>();
        s->prepare(sr, 2);
        return s.release();
    } catch (...) {
        return nullptr;
    }
}
static void destroy(void *s) {
    delete static_cast<DSP *>(s);
}
static void process(void *ptr, const JAProcess *b) {
    JerzyAudio::MXDelayParams p;
#include "mx_assign.inc"
    float *in[]{const_cast<float *>(b->input[0]), const_cast<float *>(b->input[1])};
    float *out[]{b->output[0], b->output[1]};
    double peak = 0;
    static_cast<DSP *>(ptr)->process(in, out, 2, int(b->frames), p, b->bpm, peak);
}
static uint32_t latency(void *) {
    return 0;
}
extern "C" const JAModule *module_mx() {
    static const JerzyAudio::MXDelayParams p;
    static const JAParameter params[] = {
#include "mx_metadata.inc"
    };
    static const JAModule m{sizeof(JAModule),
                            JA_ABI,
                            "mx-delay-original",
                            "Jerzy MX Analog Delay — oryginalny DSP",
                            "0.2.1",
                            0,
                            uint32_t(sizeof(params) / sizeof(params[0])),
                            params,
                            create,
                            destroy,
                            process,
                            latency};
    return &m;
}
