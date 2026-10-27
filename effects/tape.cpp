#include "core/jerzy_module.h"
#include "vendor/tape/tapedrive_dsp.h"
#include <memory>
struct State {
    JerzyAudio::TapeDriveDSP<float> dsp;
};
static void *create(double sr, uint32_t, const char *) {
    try {
        auto s = std::make_unique<State>();
        s->dsp.prepare(sr, 2);
        return s.release();
    } catch (...) {
        return nullptr;
    }
}
static void destroy(void *s) {
    delete static_cast<State *>(s);
}
static constexpr double JerzyAudio::TapeDriveParams::*fields[]{
    &JerzyAudio::TapeDriveParams::sat,         &JerzyAudio::TapeDriveParams::level,
    &JerzyAudio::TapeDriveParams::dry,         &JerzyAudio::TapeDriveParams::gainMode,
    &JerzyAudio::TapeDriveParams::shift,       &JerzyAudio::TapeDriveParams::bypass,
    &JerzyAudio::TapeDriveParams::hpfCutoff,   &JerzyAudio::TapeDriveParams::hpfRes,
    &JerzyAudio::TapeDriveParams::lpfCutoff,   &JerzyAudio::TapeDriveParams::lpfRes,
    &JerzyAudio::TapeDriveParams::wowFlutter,  &JerzyAudio::TapeDriveParams::preampMode,
    &JerzyAudio::TapeDriveParams::preampDrive, &JerzyAudio::TapeDriveParams::flutter,
    &JerzyAudio::TapeDriveParams::tapeAge,     &JerzyAudio::TapeDriveParams::optoAmount,
    &JerzyAudio::TapeDriveParams::optoMakeup,  &JerzyAudio::TapeDriveParams::optoBypass};
static void process(void *ptr, const JAProcess *b) {
    JerzyAudio::TapeDriveParams p;
    for (unsigned i = 0; i < 18; ++i)
        p.*fields[i] = b->parameters[i];
    if (p.bypass >= .5) {
        for (int c = 0; c < 2; ++c)
            std::copy_n(b->input[c], b->frames, b->output[c]);
        return;
    }
    double a = 0, d = 0, e = 0, f = 0;
    static_cast<State *>(ptr)->dsp.process(b->input, b->output, 2, int(b->frames), p, a, d, e, f);
}
static uint32_t latency(void *) {
    return 0;
}
extern "C" const JAModule *module_tape() {
    static const JerzyAudio::TapeDriveParams d;
    static const JAParameter p[] = {
#define P(id, label, field) {id, label, "", "", 0, 1, float(d.field), 0}
        P("sat", "Nasycenie", sat),
        P("level", "Poziom wyjściowy", level),
        P("dry", "Sygnał suchy", dry),
        P("gainMode", "Tryb wzmocnienia", gainMode),
        P("shift", "Charakter taśmy", shift),
        P("bypass", "Bypass oryginału", bypass),
        P("hpfCutoff", "Filtr górnoprzepustowy", hpfCutoff),
        P("hpfRes", "Rezonans HPF", hpfRes),
        P("lpfCutoff", "Filtr dolnoprzepustowy", lpfCutoff),
        P("lpfRes", "Rezonans LPF", lpfRes),
        P("wowFlutter", "Kołysanie taśmy", wowFlutter),
        P("preampMode", "Model przedwzmacniacza", preampMode),
        P("preampDrive", "Nasycenie przedwzmacniacza", preampDrive),
        P("flutter", "Drżenie taśmy", flutter),
        P("tapeAge", "Wiek taśmy", tapeAge),
        P("optoAmount", "Kompresor optyczny", optoAmount),
        P("optoMakeup", "Wzmocnienie kompresora", optoMakeup),
        P("optoBypass", "Bypass kompresora", optoBypass)
#undef P
    };
    static const JAModule m{sizeof(JAModule),
                            JA_ABI,
                            "tape-drive-original",
                            "Jerzy Tape Drive — oryginalny DSP",
                            "0.2.1",
                            0,
                            18,
                            p,
                            create,
                            destroy,
                            process,
                            latency};
    return &m;
}
