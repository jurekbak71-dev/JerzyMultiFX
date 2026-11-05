#include "core/Presentation.h"
#include "plugin-vst3/Plugin.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <windows.h>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace multifx;
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
struct Handler : IComponentHandler {
    ParameterChanges changes{2048};
    int restarts = 0;
    DWORD thread = GetCurrentThreadId();
    bool wrongThread = false;
    tresult PLUGIN_API queryInterface(const TUID iid, void **obj) override {
        if (FUnknownPrivate::iidEqual(iid, IComponentHandler::iid) ||
            FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
            *obj = static_cast<IComponentHandler *>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override {
        return 1;
    }
    uint32 PLUGIN_API release() override {
        return 1;
    }
    tresult PLUGIN_API beginEdit(ParamID) override {
        return kResultOk;
    }
    tresult PLUGIN_API performEdit(ParamID id, ParamValue value) override {
        int32 index = 0, point = 0;
        changes.addParameterData(id, index)->addPoint(0, value, point);
        return kResultOk;
    }
    tresult PLUGIN_API endEdit(ParamID) override {
        return kResultOk;
    }
    tresult PLUGIN_API restartComponent(int32 flags) override {
        if (flags & kLatencyChanged) {
            ++restarts;
            if (GetCurrentThreadId() != thread)
                wrongThread = true;
        }
        return kResultOk;
    }
};
void pump() {
    MSG m{};
    while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
}
void writeWave(const char *path) {
    std::ofstream f(path, std::ios::binary);
    auto u16 = [&](unsigned v) {
        char b[]{char(v), char(v >> 8)};
        f.write(b, 2);
    };
    auto u32 = [&](unsigned v) {
        u16(v & 65535);
        u16(v >> 16);
    };
    f.write("RIFF", 4);
    u32(38);
    f.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(1);
    u32(48000);
    u32(96000);
    u16(2);
    u16(16);
    f.write("data", 4);
    u32(2);
    u16(16384);
}
int main(int argc, char **argv) try {
    check(argc == 3, "plugin_signal DLL and temporary WAV path required");
    HMODULE dll = LoadLibraryA(argv[1]);
    check(dll, "Load actual DLL");
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(dll, "InitDll"));
    auto exit = reinterpret_cast<bool (*)()>(GetProcAddress(dll, "ExitDll"));
    auto getFactory = reinterpret_cast<IPluginFactory *(*)()>(GetProcAddress(dll, "GetPluginFactory"));
    check(init && exit && getFactory && init(), "DLL entry points");
    auto *factory = getFactory();
    HostApplication host;
    Handler handler;
    IComponent *component = nullptr;
    IEditController *controller = nullptr;
    IAudioProcessor *audio = nullptr;
    IConnectionPoint *pc = nullptr, *cc = nullptr;
    check(factory->createInstance(processorId, IComponent::iid, reinterpret_cast<void **>(&component)) ==
              kResultOk,
          "Create real processor");
    check(component->initialize(&host) == kResultOk, "Initialize processor");
    check(component->queryInterface(IAudioProcessor::iid, reinterpret_cast<void **>(&audio)) == kResultOk,
          "Audio interface");
    check(factory->createInstance(controllerId, IEditController::iid,
                                  reinterpret_cast<void **>(&controller)) == kResultOk,
          "Create real controller");
    check(controller->initialize(&host) == kResultOk, "Initialize controller");
    controller->setComponentHandler(&handler);
    component->queryInterface(IConnectionPoint::iid, reinterpret_cast<void **>(&pc));
    controller->queryInterface(IConnectionPoint::iid, reinterpret_cast<void **>(&cc));
    check(pc && cc, "Connection points");
    pc->connect(cc);
    cc->connect(pc);
    SpeakerArrangement stereo = SpeakerArr::kStereo;
    check(audio->setBusArrangements(&stereo, 1, &stereo, 1) == kResultOk, "Main stereo buses");
    ProcessSetup setup{};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.sampleRate = 48000;
    setup.maxSamplesPerBlock = 128;
    check(audio->setupProcessing(setup) == kResultOk, "Processing setup");
    component->activateBus(kAudio, kInput, 0, true);
    component->activateBus(kAudio, kOutput, 0, true);
    check(component->setActive(true) == kResultOk, "Activate DSP");
    audio->setProcessing(true);
    std::array<float, 128> input{}, outL{}, outR{}, retL{}, retR{};
    float *inputs[]{input.data(), input.data()}, *outputs[]{outL.data(), outR.data()},
        *returns[]{retL.data(), retR.data()};
    AudioBusBuffers in[2]{};
    in[0].numChannels = 2;
    in[0].channelBuffers32 = inputs;
    in[1].numChannels = 2;
    in[1].channelBuffers32 = returns;
    AudioBusBuffers output{};
    output.numChannels = 2;
    output.channelBuffers32 = outputs;
    ProcessData data{};
    data.processMode = kRealtime;
    data.symbolicSampleSize = kSample32;
    data.numSamples = 128;
    data.numInputs = 1;
    data.numOutputs = 1;
    data.inputs = in;
    data.outputs = &output;
    data.inputParameterChanges = &handler.changes;
    double phase = 0;
    auto render = [&](int blocks, bool useReturn = false) {
        double energy = 0;
        for (int block = 0; block < blocks; ++block) {
            for (int i = 0; i < 128; ++i) {
                const float x = .1f * std::sin(float(phase));
                phase += .057;
                input[i] = useReturn ? 0 : x;
                retL[i] = retR[i] = x;
            }
            check(audio->process(data) == kResultOk, "Real DLL processes audio");
            handler.changes.clearQueue();
            if (block > blocks / 2)
                for (float x : outL) {
                    check(std::isfinite(x), "Finite DLL output");
                    energy += x * x;
                }
            if (block % 16 == 0) {
                Sleep(2);
                pump();
            }
        }
        return std::sqrt(energy / (128. * (blocks - blocks / 2 - 1)));
    };
    const double clean = render(180);
    check(clean > .06 && clean < .08, "Actual clean gain");
    check(audio->getLatencySamples() == 0, "Clean DLL latency zero");

    // End-to-end control path: the VST3 controller must notify the host,
    // the host queues the parameter change, and the actual DLL DSP must
    // produce a measurably attenuated signal. A moving knob alone is not enough.
    const auto master = ParamID(layout().masterGain);
    const auto originalGain = controller->getParamNormalized(master);
    const auto editMaster = [&](double value) {
        check(handler.beginEdit(master) == kResultOk, "Begin master output edit");
        check(controller->setParamNormalized(master, value) == kResultOk, "Set master output in controller");
        check(handler.performEdit(master, value) == kResultOk, "Host receives master output edit");
        check(handler.endEdit(master) == kResultOk, "End master output edit");
    };
    editMaster(0.75); // -10.5 dB, versus the initial 0 dB.
    const double attenuated = render(180);
    check(attenuated / clean > .27 && attenuated / clean < .34,
          "Real VST3 controller-to-host-to-DSP master output attenuates audio");
    editMaster(originalGain);
    const double restored = render(180);
    check(restored / clean > .98 && restored / clean < 1.02,
          "Restoring the VST3 master output restores actual audio level");
    writeWave(argv[2]);
    auto *message = new HostMessage;
    message->setMessageID("loadIR");
    message->getAttributes()->setBinary("path", argv[2], uint32(std::strlen(argv[2])));
    check(pc->notify(message) == kResultOk, "IR file imported through actual processor notification");
    message->release();
    const double wet = render(180);
    check(wet / clean > .49 && wet / clean < .51,
          "IR import changes actual DLL audio by -6dB without extra GUI actions");
    check(audio->getLatencySamples() == 128, "IR DLL latency128");
    Sleep(130);
    pump();
    check(handler.restarts > 0 && !handler.wrongThread,
          "PDC change reaches host on main thread with editor closed");
    handler.performEdit(layout().blockBypass[4], 1);
    const double bypass = render(180);
    check(bypass / clean > .99 && bypass / clean < 1.01, "IR block bypass restores actual dry DLL signal");
    check(audio->getLatencySamples() == 0, "Bypassed IR latency zero");
    audio->setProcessing(false);
    component->setActive(false);
    SpeakerArrangement pair[]{SpeakerArr::kStereo, SpeakerArr::kStereo};
    check(audio->setBusArrangements(pair, 2, &stereo, 1) == kResultOk, "Return bus arrangement");
    component->activateBus(kAudio, kInput, 1, true);
    handler.performEdit(layout().processorBypass[2], 0);
    handler.performEdit(parameterId("attack.useReturn"), 1);
    handler.performEdit(parameterId("attack.blend"), 1);
    handler.performEdit(parameterId("attack.attack"), 0);
    data.numSamples = 0;
    check(audio->process(data) == kResultOk, "Flush parameters before activation");
    handler.changes.clearQueue();
    data.numSamples = 128;
    data.numInputs = 2;
    check(component->setActive(true) == kResultOk, "Warm Return DSP before activation");
    audio->setProcessing(true);
    const double returned = render(220, true);
    check(returned > .01, "Actual sidechain Return reaches Attack Decay audio");
    check(audio->getLatencySamples() == attackMonoLatency, "MONO Attack latency reported by real DLL");
    handler.changes.clearQueue();
    handler.performEdit(layout().midiMap + 74, double(parameterId("delayA.time") + 1) / layout().controlCount);
    handler.performEdit(layout().delayType[0], .5); // Memory Delay
    handler.performEdit(3000 + 74, .75); // Host-delivered MIDI CC 74
    ParameterChanges routed(2048);
    data.outputParameterChanges = &routed;
    data.numSamples = 0;
    check(audio->process(data) == kResultOk, "Flush routed MIDI CC into real VST3 processor");
    bool memoryTimeUpdated = false;
    for (int i = 0; i < routed.getParameterCount(); ++i) {
        auto *queue = routed.getParameterData(i);
        if (queue && queue->getParameterId() == ParamID(parameterId("delayA.memory.delay"))) {
            int32 offset = 0; ParamValue value = 0;
            memoryTimeUpdated = queue->getPoint(0, offset, value) == kResultTrue &&
                                std::abs(value - .75) < 1.e-6;
        }
    }
    check(memoryTimeUpdated, "Shared DELAY TIME MIDI CC follows selected Memory engine in actual VST3");
    audio->setProcessing(false);
    component->setActive(false);
    pc->disconnect(cc);
    cc->disconnect(pc);
    pc->release();
    cc->release();
    controller->setComponentHandler(nullptr);
    controller->terminate();
    controller->release();
    component->terminate();
    audio->release();
    component->release();
    factory->release();
    check(exit(), "DLL exit");
    FreeLibrary(dll);
    DeleteFileA(argv[2]);
    std::cout << "PASS actual VST3 DLL: clean gain/latency0, IR upload changes audio, block bypass, PDC "
                 "callback on main thread without GUI, connected Return audio\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
}
