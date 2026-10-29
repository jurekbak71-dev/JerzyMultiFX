#pragma once
#include "core/Presentation.h"
#include <array>
namespace multifx {
// Stable MIDI Learn addresses: the existing digital TIME/FEEDBACK and slot MIX.
// Resolve only on an incoming CC, so preset-specific values of other engines stay intact.
struct DelayMidiRoutes {
    struct Target { int first=-1, second=-1; };
    std::array<std::array<Target, 3>, 2> canonical{};
    std::array<std::array<std::array<Target, 3>, 5>, 2> engines{};
    std::array<int, 2> mxSecondEnable{};
    DelayMidiRoutes() {
        for(int bank=0;bank<2;++bank){
            const std::string root=bank?"delayB.":"delayA.";
            const int mix=parameterId(root+"mix");
            canonical[bank]={Target{parameterId(root+"time")},Target{parameterId(root+"feedback")},Target{mix}};
            engines[bank][0]=canonical[bank];
            engines[bank][1]={Target{parameterId(root+"mx.slot0_time"),parameterId(root+"mx.slot1_time")},
                              Target{parameterId(root+"mx.slot0_feedback"),parameterId(root+"mx.slot1_feedback")},Target{mix}};
            engines[bank][2]={Target{parameterId(root+"memory.delay")},Target{parameterId(root+"memory.feedback")},Target{mix}};
            engines[bank][3]={Target{parameterId(root+"reel.time")},Target{parameterId(root+"reel.feedback")},Target{mix}};
            engines[bank][4]={Target{parameterId(root+"oil.time")},Target{parameterId(root+"oil.feedback")},Target{mix}};
            mxSecondEnable[bank]=parameterId(root+"mx.slot1_enable");
        }
    }
    Target resolve(const State& state,int mapped) const {
        for(int bank=0;bank<2;++bank)
            for(int kind=0;kind<3;++kind)
                if(mapped==canonical[bank][kind].first){
                    const int engine=std::clamp(int(plain(layout().delayType[bank],state.values[layout().delayType[bank]])),0,4);
                    Target target=engines[bank][engine][kind];
                    if(engine==1 && mxSecondEnable[bank]>=0 && state.values[mxSecondEnable[bank]]<.5f)
                        target.second=-1;
                    return target;
                }
        return {mapped,-1};
    }
};
}
