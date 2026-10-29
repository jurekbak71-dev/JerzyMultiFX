#pragma once
#include "core/Model.h"
#include <string_view>

namespace multifx {
// Shared RPi/VST presentation hints; DSP ranges and stable parameter IDs stay untouched.
struct ParameterUI {
    Parameter definition;
    bool bypass = false;
    const char *control = "knob";
};
inline const std::vector<ParameterUI> &controlUI() {
    static const auto result = [] {
        std::vector<ParameterUI> out;
        for (const auto &p : layout().parameters) out.push_back({p});
        auto choices = [&](const std::string &key, const std::string &labels) {
            for (auto &p : out) if (p.definition.key == key) {
                p.definition.choices = labels;
                p.definition.steps = int(std::count(labels.begin(), labels.end(), '|'));
            }
        };
        auto toggle = [&](const std::string &key) {
            for (auto &p : out) if (p.definition.key == key) {
                p.definition.steps = 1; p.definition.choices.clear(); p.control = "switch";
            }
        };
        choices("td.gainMode", "Warm|Hot");
        choices("td.shift", "Neutral|Bright|Thick");
        choices("td.preampMode", "Off|Tube|Transistor");
        toggle("td.optoBypass"); toggle("atomic.smooth"); toggle("atomic.flat");
        for (const std::string bank : {"delayA", "delayB"}) {
            choices(bank+".reel.mode", "ORIGINAL|STUDIO|EXPERIMENTAL");
            choices(bank+".reel.cassette", "FRESH TAPE|VINTAGE TAPE|DAMAGED TAPE");
            choices(bank+".reel.division", "1/16|1/8T|1/8|1/4|1/4D|1/2|1/2D|1/1");
            choices(bank+".memory.wave", "Sine|Triangle");
            for (const std::string key : {"bypass","modEnable","tempoSync"}) toggle(bank+".memory."+key);
            toggle(bank+".mx.spill");
            for(int slot=0;slot<2;++slot) {
                const auto prefix=bank+".mx.slot"+std::to_string(slot)+"_";
                toggle(prefix+"enable"); toggle(prefix+"sync");
                for(int head=6;head<14;++head) toggle(prefix+"algo0_"+std::to_string(head));
            }
            for(const std::string key : {"master","sync","h1on","h2on","h3on","h4on","inbypass","outbypass","mechanicson","springon"}) toggle(bank+".oil."+key);
            for(auto &p:out) if(p.definition.key==bank+".oil.voices")p.definition.steps=4;
        }
        for(auto &p:out) {
            auto &d=p.definition;
            p.bypass=d.key.ends_with("bypass") || d.key.ends_with("Bypass");
            if(p.bypass) {d.steps=1; d.choices.clear(); p.control="switch";}
            if(std::string_view(p.control)=="switch")continue;
            if(!d.choices.empty())p.control="select";
            else if(d.steps==1)p.control="switch";
            else if(d.steps>0)p.control=d.steps<=64?"select":"number";
        }
        return out;
    }();
    return result;
}
inline float discreteValue(int id, float value) {
    const int steps=controlUI()[size_t(id)].definition.steps;
    return steps ? std::round(value*steps)/steps : value;
}
} // namespace multifx
