#include "rpi/ParameterUI.h"
#include "rpi/StateWriter.h"
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
    require(rpi::discreteValue(id("td.shift"),.49f)==.5f,"Snap selectors to original DSP model positions");
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
