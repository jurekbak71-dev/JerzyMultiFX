#include "rpi/ParameterUI.h"
#include "core/DelayMidiRoutes.h"
#include "rpi/StateWriter.h"
#include "core/Presentation.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace multifx;
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
int main() try {
    const auto &ui=rpi::parameterUI();const auto &original=layout().parameters;
    require(ui.size()==original.size(),"RPi hints must preserve the exact VST parameter count");
    for(size_t i=0;i<ui.size();++i) {
        require(ui[i].definition.key==original[i].key,"Stable VST/RPi parameter keys");
        require(ui[i].definition.initial==original[i].initial,"Hints must not change VST defaults");
        if(ui[i].definition.steps>0 || !ui[i].definition.choices.empty())
            require(std::string_view(ui[i].control)!="knob","Discrete options must never be potentiometers");
        if(ui[i].bypass)require(std::string_view(ui[i].control)=="switch","Every bypass is a switch");
    }
    auto id=[&](const char *key){for(size_t i=0;i<original.size();++i)if(original[i].key==key)return int(i);throw std::runtime_error(key);};
    require(ui[id("td.shift")].definition.choices=="Neutral|Bright|Thick","Tape mode labels");
    require(std::string_view(ui[id("delayA.memory.boost")].control)=="knob","Continuous boost remains continuous");
    require(controlTitle(id("delayA.mx.slot0_time")).starts_with("DELAY A / MX 1:"),"DELAY A internal MX label");
    require(controlTitle(id("delayB.mx.slot0_time")).starts_with("DELAY B / MX 1:"),"DELAY B must not show DELAY A for its first MX engine");
    require(controlTitle(id("delayB.mx.slot1_time")).starts_with("DELAY B / MX 2:"),"DELAY B second MX engine label");
    require(std::string_view(ui[id("drive.gate.on")].control)=="switch","Noise gate has an ON/OFF switch");
    for(const std::string bank:{"delayA.","delayB."}) {
        for(const char* key:{"reel.enabled","reel.short","reel.long","reel.sync","reel.wetOnly",
                             "reel.stop","reel.dub","reel.sos","reel.freeze","reel.tails","reel.autoAge",
                             "memory.bypass","memory.modEnable","memory.tempoSync","oil.h1on","oil.h2on",
                             "oil.h3on","oil.h4on","oil.mechanicson","oil.springon"})
            require(std::string_view(ui[id((bank+key).c_str())].control)=="switch",
                    "Delay binary controls use switches in VST and RPi");
        require(std::string_view(ui[id((bank+"reel.seed").c_str())].control)=="number",
                "Reel seed is a numeric field, not a potentiometer");
    }
    require(layout().attackMakeup==id("attack.makeup") &&
            plain(layout().attackMakeup,0)==-12 && plain(layout().attackMakeup,.5f)==0 &&
            plain(layout().attackMakeup,1)==12,
            "Attack compressor makeup uses a centred bipolar dB control");
    require(parameterText(id("td.optoMakeup"),0)=="-12.0 dB" &&
            parameterText(id("td.optoMakeup"),.5f)=="+0.0 dB" &&
            parameterText(id("td.optoMakeup"),1)=="+12.0 dB", "Tape optical makeup uses bipolar dB labels");
    for(const char *key:{"comp.makeup","delayA.oil.icmakeup","delayA.oil.ocmakeup",
                         "delayB.oil.icmakeup","delayB.oil.ocmakeup"})
        require(parameterText(id(key),0)=="-24.0 dB" &&
                parameterText(id(key),.5f)=="+0.0 dB" &&
                parameterText(id(key),1)=="+24.0 dB", "Makeup labels show the full bipolar range");
    {
        State state;
        auto controls=panelParameters(PanelSelection{0,2},state);
        require(std::find(controls.begin(),controls.end(),id("drive.gate.on"))!=controls.end(),
                "Noise gate is present in DRIVE panel");
        controls=panelParameters(PanelSelection{1,1,0,true},state);
        require(std::find(controls.begin(),controls.end(),layout().attackMakeup)!=controls.end(),
                "Attack compressor makeup is present in advanced VST/RPi panel");
        for(int bank=0;bank<2;++bank)for(int engine=0;engine<5;++engine)for(bool advanced:{false,true}) {
            const std::string prefix=bank?"delayB.":"delayA.";
            state.values[layout().delayType[bank]]=float(engine)/4;
            controls=panelParameters(PanelSelection{2,bank,0,advanced},state);
            const auto position=[&](const std::string& key){
                return std::find(controls.begin(),controls.end(),id((prefix+key).c_str()));
            };
            const std::array<const char*,5> firstEngineKey{"time","mx.mix","memory.model","reel.mode","oil.master"};
            const auto mix=position("mix"),level=position("level"),first=position(firstEngineKey[engine]);
            require(mix!=controls.end()&&level!=controls.end()&&first!=controls.end()&&
                    mix<first&&level<first,
                    "Bank MIX and LEVEL stay above all delay engines in basic and advanced views");
            require(std::count(controls.begin(),controls.end(),id((prefix+"mix").c_str()))==1,
                    "Common delay bank MIX appears once");
        }
    }
    require(rpi::discreteValue(id("td.shift"),.49f)==.5f,"Snap selectors to original DSP model positions");
    DelayMidiRoutes routes;
    State routeState;
    for(int bank=0;bank<2;++bank) {
        const std::string prefix=bank?"delayB.":"delayA.";
        for(int engine=0;engine<5;++engine) {
            routeState.values[layout().delayType[bank]]=float(engine)/4;
            auto time=routes.resolve(routeState,id((prefix+"time").c_str()));
            auto feedback=routes.resolve(routeState,id((prefix+"feedback").c_str()));
            const std::array<const char*,5> timeKeys{"time","mx.slot0_time","memory.delay","reel.time","oil.time"};
            const std::array<const char*,5> feedbackKeys{"feedback","mx.slot0_feedback","memory.feedback","reel.feedback","oil.feedback"};
            require(time.first==id((prefix+timeKeys[engine]).c_str()),
                    "Shared MIDI TIME follows the selected delay engine");
            require(feedback.first==id((prefix+feedbackKeys[engine]).c_str()),
                    "Shared MIDI FEEDBACK follows the selected delay engine");
            require(routes.resolve(routeState,id((prefix+"mix").c_str())).first==id((prefix+"mix").c_str()),
                    "Shared MIDI MIX retains the bank level");
        }
        routeState.values[layout().delayType[bank]]=.25f;
        routeState.values[id((prefix+"mx.slot1_enable").c_str())]=1;
        require(routes.resolve(routeState,id((prefix+"time").c_str())).second==id((prefix+"mx.slot1_time").c_str()),
                "MX second engine receives common MIDI TIME when active");
    }
    const auto directory=std::filesystem::temp_directory_path()/("multifx-rpi-control-"+std::to_string(getpid()));
    std::filesystem::create_directories(directory);const auto path=(directory/"last-state.jmfx").string();
    Project p;
    {rpi::StateWriter writer(path);for(int i=0;i<20;++i){p.state.values[id("cpd.gain")]=float(i)/20;writer.submit(p);}}
    Project restored;require(loadProject(path,restored),"Background state writer flushes at shutdown");
    require(restored.state.values[id("cpd.gain")]==19.f/20,"Most recent snapshot survives coalescing");
    std::filesystem::remove_all(directory);
    std::cout<<"PASS RPi option types, VST IDs/defaults and background state writer\n";
    return 0;
} catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
