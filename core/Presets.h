#pragma once
#include "Model.h"
namespace multifx {
inline constexpr const char *presetNames[]{"Clean",  "Warm Tape",    "TS808", "Green Russian Fuzz",
                                           "Chorus", "Digital Echo", "Plate", "Shimmer"};
inline const std::array<State, 8> &factoryPresets() {
    static const auto presets = []() {
        std::array<State, 8> out;
        const auto &l = layout();
        auto set = [&](State &s, const char *key, float value) {
            for (size_t i = 0; i < l.parameters.size(); ++i)
                if (l.parameters[i].key == key)
                    s.values[i] = value;
        };
        for (auto &s : out) {
            set(s, "td.bypass", 1);
            set(s, "td.optoBypass", 1);
        }
        set(out[1], "td.bypass", 0);
        set(out[1], "td.sat", .45);
        set(out[2], "drive.bypass", 0);
        set(out[2], "drive.type", .5);
        set(out[2], "drive.gain", .35);
        set(out[3], "drive.bypass", 0);
        set(out[3], "drive.gain", .4);
        set(out[3], "fuzz.model", 4.f / 19);
        set(out[4], "clone.bypass", 0);
        set(out[4], "clone.mix", .4);
        set(out[5], "delayA.mix", .35);
        set(out[5], "delayA.time", .15);
        set(out[6], "reverb.mix", .3);
        set(out[7], "reverb.engine", 1);
        set(out[7], "reverb.pitch", 1);
        set(out[7], "reverb.mix", .4);
        return out;
    }();
    return presets;
}
inline void selectFactory(State &state, int index) {
    auto &l = layout();
    index = std::clamp(index, 0, 7);
    const auto &source = factoryPresets()[index];
    for (size_t i = 0; i < l.parameters.size(); ++i)
        if (int(i) < l.midiLearn || int(i) > l.factoryProgram)
            state.values[i] = source.values[i];
    state.values[l.factoryProgram] = float(index) / 7;
}
} // namespace multifx
