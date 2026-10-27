#pragma once
#include "core/Engine.h"
#include "core/Project.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <mutex>
namespace multifx {
using namespace Steinberg;
using namespace Steinberg::Vst;
inline const FUID processorId(0x4A4D4658, 0x710148A5, 0xB81D27AE, 0xC3D64001);
inline const FUID controllerId(0x4A4D4658, 0x710148A5, 0xB81D27AE, 0xC3D64002);
class Processor final : public AudioEffect {
    std::array<std::atomic<float>, maxParameters> values;
    Engine engine;
    Project assets;
    std::mutex assetMutex;
    double bpm = 120;
    std::array<std::array<float, 1024>, 4> scratch{};
    bool restoreIR();

  public:
    Processor();
    static FUnknown *create(void *) {
        return static_cast<IAudioProcessor *>(new Processor);
    }
    tresult PLUGIN_API initialize(FUnknown *) override;
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement *, int32, SpeakerArrangement *, int32) override;
    tresult PLUGIN_API canProcessSampleSize(int32 s) override {
        return s == kSample32 || s == kSample64 ? kResultTrue : kResultFalse;
    }
    tresult PLUGIN_API setupProcessing(ProcessSetup &) override;
    tresult PLUGIN_API setActive(TBool) override;
    tresult PLUGIN_API process(ProcessData &) override;
    uint32 PLUGIN_API getLatencySamples() override {
        return Engine::latency;
    }
    uint32 PLUGIN_API getTailSamples() override {
        return uint32(processSetup.sampleRate * 20);
    }
    tresult PLUGIN_API setState(IBStream *) override;
    tresult PLUGIN_API getState(IBStream *) override;
    tresult PLUGIN_API notify(IMessage *) override;
};
class Controller final : public EditControllerEx1, public IMidiMapping {
  public:
    std::atomic<float> meterIn{0}, meterOut{0}, meterGR{0};
    void requestMeters();
    tresult PLUGIN_API notify(IMessage *) override;

  public:
    static FUnknown *create(void *) {
        return static_cast<IEditController *>(new Controller);
    }
    DELEGATE_REFCOUNT(EditControllerEx1)
    tresult PLUGIN_API queryInterface(const char *, void **) override;
    tresult PLUGIN_API getMidiControllerAssignment(int32, int16, CtrlNumber, ParamID &) override;
    tresult PLUGIN_API initialize(FUnknown *) override;
    tresult PLUGIN_API setComponentState(IBStream *) override;
    IPlugView *PLUGIN_API createView(FIDString) override;
    tresult PLUGIN_API getParamStringByValue(ParamID, ParamValue, String128) override;
    tresult PLUGIN_API getParamValueByString(ParamID, TChar *, ParamValue &) override;
    void loadFile(const char *path, const char *kind);
};
IPlugView *createEditor(Controller *);
} // namespace multifx
