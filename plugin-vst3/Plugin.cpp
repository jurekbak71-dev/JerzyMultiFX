#include "Plugin.h"
#include "base/source/fstreamer.h"
#include "core/Presets.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/main/pluginfactory.h"
#include <codecvt>
#include <cstdio>
#include <cstring>
#include <locale>
#include <sstream>
namespace multifx {
namespace {
bool read(IBStream *stream, Project &project) {
    if (!stream)
        return false;
    IBStreamer s(stream, kLittleEndian);
    int32 size;
    if (!s.readInt32(size) || size < 28 || size > 1024 * 1024)
        return false;
    std::vector<uint8_t> b(size);
    if (s.readRaw(b.data(), size) != size)
        return false;
    return decode(b, project);
}
} // namespace
Processor::Processor() {
    setControllerClass(controllerId);
    (void)factoryPresets();
    State s;
    for (int i = 0; i < maxParameters; ++i)
        values[i].store(s.values[i]);
    processContextRequirements.needTempo().needProjectTimeMusic().needTransportState();
}
tresult PLUGIN_API Processor::initialize(FUnknown *host) {
    auto r = AudioEffect::initialize(host);
    if (r != kResultOk)
        return r;
    addAudioInput(STR16("Audio Input"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Audio Output"), SpeakerArr::kStereo);
    addEventInput(STR16("MIDI Control"), 1);
    return kResultOk;
}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement *in, int32 ni, SpeakerArrangement *out,
                                                 int32 no) {
    if (ni != 1 || no != 1 || !in || !out || !(in[0] == SpeakerArr::kMono || in[0] == SpeakerArr::kStereo) ||
        in[0] != out[0])
        return kResultFalse;
    getAudioInput(0)->setArrangement(in[0]);
    getAudioOutput(0)->setArrangement(out[0]);
    return kResultOk;
}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup &s) {
    if (!std::isfinite(s.sampleRate) || s.sampleRate < 8000 || s.sampleRate > 192000 ||
        s.maxSamplesPerBlock < 1)
        return kResultFalse;
    return AudioEffect::setupProcessing(s);
}
bool Processor::restoreIR() {
    if (assets.irL.empty()) {
        float identity = 1;
        return engine.loadIR(&identity, nullptr, 1);
    }
    double ratio = processSetup.sampleRate / assets.irRate;
    int n = int(std::ceil(assets.irL.size() * ratio));
    if (n > Convolver::maxFrames)
        return false;
    std::vector<float> l(n), r(n);
    for (int i = 0; i < n; ++i) {
        double t = i / ratio;
        size_t a = std::min(size_t(t), assets.irL.size() - 1), b = std::min(a + 1, assets.irL.size() - 1);
        l[i] = assets.irL[a] + (assets.irL[b] - assets.irL[a]) * float(t - a);
        r[i] = assets.irR[a] + (assets.irR[b] - assets.irR[a]) * float(t - a);
    }
    return engine.loadIR(l.data(), r.data(), n);
}
tresult PLUGIN_API Processor::setActive(TBool active) {
    if (active) {
        try {
            engine.prepare(processSetup.sampleRate, processSetup.maxSamplesPerBlock);
            std::lock_guard lock(assetMutex);
            if (!restoreIR())
                return kResultFalse;
        } catch (...) {
            return kResultFalse;
        }
    }
    return AudioEffect::setActive(active);
}
tresult PLUGIN_API Processor::setState(IBStream *stream) {
    Project p;
    if (!read(stream, p))
        return kResultFalse;
    for (size_t i = 0; i < layout().parameters.size(); ++i)
        values[i].store(p.state.values[i]);
    {
        std::lock_guard lock(assetMutex);
        assets = std::move(p);
        if (processSetup.sampleRate > 0 && !restoreIR())
            return kResultFalse;
    }
    return kResultOk;
}
tresult PLUGIN_API Processor::getState(IBStream *stream) {
    if (!stream)
        return kResultFalse;
    std::lock_guard lock(assetMutex);
    for (size_t i = 0; i < layout().parameters.size(); ++i)
        assets.state.values[i] = values[i].load();
    auto b = encode(assets);
    IBStreamer s(stream, kLittleEndian);
    return s.writeInt32(int32(b.size())) && s.writeRaw(b.data(), int32(b.size())) == int32(b.size())
               ? kResultOk
               : kResultFalse;
}
tresult PLUGIN_API Processor::notify(IMessage *message) {
    if (!message)
        return kInvalidArgument;
    const char *id = message->getMessageID();
    if (!id)
        return kInvalidArgument;
    if (!strcmp(id, "meters")) {
        auto *reply = allocateMessage();
        if (!reply)
            return kResultFalse;
        reply->setMessageID("meters");
        auto *a = reply->getAttributes();
        a->setFloat("in", engine.inputPeak.load());
        a->setFloat("out", engine.outputPeak.load());
        a->setFloat("gr", engine.gainReduction.load());
        auto result = sendMessage(reply);
        reply->release();
        return result;
    }
    if (!strcmp(id, "loadPreset") || !strcmp(id, "savePreset")) {
        const void *bytes = nullptr;
        uint32 size = 0;
        if (message->getAttributes()->getBinary("path", bytes, size) != kResultOk || !size || size > 4096)
            return kResultFalse;
        std::string path(static_cast<const char *>(bytes), size);
        std::lock_guard lock(assetMutex);
        if (!strcmp(id, "savePreset")) {
            for (size_t i = 0; i < layout().parameters.size(); ++i)
                assets.state.values[i] = values[i].load();
            return saveProject(path, assets) ? kResultOk : kResultFalse;
        }
        Project p;
        if (!loadProject(path, p))
            return kResultFalse;
        assets = std::move(p);
        for (size_t i = 0; i < layout().parameters.size(); ++i)
            values[i].store(assets.state.values[i]);
        return restoreIR() ? kResultOk : kResultFalse;
    }
    if (!strcmp(id, "loadIR")) {
        const void *bytes = nullptr;
        uint32 size = 0;
        if (message->getAttributes()->getBinary("path", bytes, size) != kResultOk || !size || size > 4096)
            return kResultFalse;
        std::string path(static_cast<const char *>(bytes), size);
        std::lock_guard lock(assetMutex);
        Project p = assets;
        if (!loadWave(path, processSetup.sampleRate > 0 ? processSetup.sampleRate : 48000, p))
            return kResultFalse;
        if (!engine.loadIR(p.irL.data(), p.irR.data(), int(p.irL.size())))
            return kResultFalse;
        assets = std::move(p);
        return kResultOk;
    }
    return AudioEffect::notify(message);
}
tresult PLUGIN_API Processor::process(ProcessData &data) {
    if (data.numSamples < 0)
        return kResultFalse;
    const int count = int(layout().parameters.size());
    State s;
    for (int i = 0; i < count; ++i)
        s.values[i] = values[i].load(std::memory_order_relaxed);
    std::array<int32, 2048> points{};
    int queues =
        data.inputParameterChanges ? std::min(2048, data.inputParameterChanges->getParameterCount()) : 0;
    auto apply = [&](int at) {
        for (int q = 0; q < queues; ++q) {
            auto *queue = data.inputParameterChanges->getParameterData(q);
            if (!queue)
                continue;
            int32 offset = 0;
            ParamValue v = 0;
            while (points[q] < queue->getPointCount() &&
                   queue->getPoint(points[q], offset, v) == kResultTrue && offset <= at) {
                int id = int(queue->getParameterId());
                float value = clean(float(v));
                if (id == layout().factoryProgram) {
                    selectFactory(s, int(std::round(value * 7)));
                    for (int k = 0; k < layout().midiLearn; ++k) {
                        values[k].store(s.values[k]);
                        if (data.outputParameterChanges) {
                            int32 qindex = 0;
                            auto *changed = data.outputParameterChanges->addParameterData(k, qindex);
                            if (changed) {
                                int32 point;
                                changed->addPoint(at, s.values[k], point);
                            }
                        }
                    }
                }

                if (id >= 3000 && id < 3128) {
                    int cc = id - 3000;
                    int target = int(plain(layout().midiLearn, s.values[layout().midiLearn]));
                    if (target >= 0) {
                        int map = layout().midiMap + cc;
                        s.values[map] = float(target + 1) / layout().controlCount;
                        s.values[layout().midiLearn] = 0;
                        values[map].store(s.values[map]);
                        values[layout().midiLearn].store(0);
                        if (data.outputParameterChanges) {
                            int32 index = 0;
                            if (auto *out = data.outputParameterChanges->addParameterData(map, index)) {
                                int32 p;
                                out->addPoint(at, s.values[map], p);
                            }
                        }
                    }
                    int mapped = int(plain(layout().midiMap + cc, s.values[layout().midiMap + cc]));
                    if (mapped >= 0)
                        id = mapped;
                    else
                        id = -1;
                }
                if (id >= 0 && id < count) {
                    s.values[id] = value;
                    values[id].store(value, std::memory_order_relaxed);
                }
                ++points[q];
            }
        }
    };
    apply(0);
    if (data.numSamples == 0)
        return kResultOk;
    if (data.numOutputs != 1 || !data.outputs || data.outputs[0].numChannels < 1 ||
        data.outputs[0].numChannels > 2)
        return kResultOk;
    if (data.symbolicSampleSize != kSample32 && data.symbolicSampleSize != kSample64)
        return kResultFalse;
    auto &out = data.outputs[0];
    auto *in = data.numInputs > 0 && data.inputs ? &data.inputs[0] : nullptr;
    if (data.processContext && (data.processContext->state & ProcessContext::kTempoValid) &&
        std::isfinite(data.processContext->tempo) && data.processContext->tempo > 0)
        bpm = data.processContext->tempo;
    bool playing = data.processContext && (data.processContext->state & ProcessContext::kPlaying);
    double beat = data.processContext && (data.processContext->state & ProcessContext::kProjectTimeMusicValid)
                      ? data.processContext->projectTimeMusic
                      : 0;
    for (int start = 0; start < data.numSamples;) {
        apply(start);
        int end = std::min(start + 1024, data.numSamples);
        for (int q = 0; q < queues; ++q) {
            auto *queue = data.inputParameterChanges->getParameterData(q);
            int32 offset;
            ParamValue v;
            if (queue && points[q] < queue->getPointCount() &&
                queue->getPoint(points[q], offset, v) == kResultTrue && offset > start)
                end = std::min(end, int(offset));
        }
        int n = end - start;
        std::array<JAMidi, 128> midi{};
        unsigned nm = 0;
        if (data.inputEvents)
            for (int k = 0; k < data.inputEvents->getEventCount() && nm < midi.size(); ++k) {
                Event e{};
                if (data.inputEvents->getEvent(k, e) == kResultOk && e.sampleOffset >= start &&
                    e.sampleOffset < end && e.type == Event::kNoteOnEvent)
                    midi[nm++] = {uint32_t(e.sampleOffset - start),
                                  3,
                                  {uint8_t(144 | e.noteOn.channel), uint8_t(e.noteOn.pitch),
                                   uint8_t(e.noteOn.velocity * 127)}};
            }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) {
                float x = 0;
                if (in && in->numChannels > 0) {
                    int ch = std::min(c, in->numChannels - 1);
                    if (data.symbolicSampleSize == kSample32 && in->channelBuffers32 &&
                        in->channelBuffers32[ch])
                        x = in->channelBuffers32[ch][start + i];
                    if (data.symbolicSampleSize == kSample64 && in->channelBuffers64 &&
                        in->channelBuffers64[ch])
                        x = float(in->channelBuffers64[ch][start + i]);
                }
                scratch[c][i] = x;
            }
        engine.process(scratch[0].data(), scratch[1].data(), scratch[2].data(), scratch[3].data(), n, s, bpm,
                       beat + start * bpm / (60 * processSetup.sampleRate), playing, midi.data(), nm);
        for (int c = 0; c < out.numChannels; ++c)
            for (int i = 0; i < n; ++i) {
                if (data.symbolicSampleSize == kSample32) {
                    if (out.channelBuffers32 && out.channelBuffers32[c])
                        out.channelBuffers32[c][start + i] = scratch[c + 2][i];
                } else if (out.channelBuffers64 && out.channelBuffers64[c])
                    out.channelBuffers64[c][start + i] = scratch[c + 2][i];
            }
        start = end;
    }
    out.silenceFlags = 0;
    return kResultOk;
}
tresult PLUGIN_API Controller::initialize(FUnknown *host) {
    auto r = EditControllerEx1::initialize(host);
    if (r != kResultOk)
        return r;
    for (size_t i = 0; i < layout().parameters.size(); ++i) {
        if (int(i) == layout().factoryProgram)
            continue;
        const auto &p = layout().parameters[i];
        String128 title{};
        auto wide = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(
            p.key + " / " + p.name);
        for (size_t k = 0; k < std::min(size_t(127), wide.size()); ++k)
            title[k] = TChar(wide[k]);
        parameters.addParameter(title, nullptr, p.steps, p.initial,
                                ParameterInfo::kCanAutomate |
                                    (int(i) == layout().bypass ? ParameterInfo::kIsBypass : 0),
                                ParamID(i));
    }
    addUnit(new Unit(STR16("Jerzy MultiFX"), kRootUnitId, kNoParentUnitId, layout().factoryProgram));
    String128 programName{};
    UString(programName, 128).fromAscii("Factory Programs");
    auto *list = new ProgramList(programName, layout().factoryProgram, kRootUnitId);
    for (const char *name : presetNames) {
        String128 p{};
        UString(p, 128).fromAscii(name);
        list->addProgram(p);
    }
    addProgramList(list);
    parameters.addParameter(list->getParameter());
    for (int cc = 0; cc < 128; ++cc) {
        String128 name{};
        char buf[48];
        snprintf(buf, sizeof(buf), "MIDI Input CC %d", cc);
        UString(name, 128).fromAscii(buf);
        parameters.addParameter(name, nullptr, 0, 0, ParameterInfo::kIsHidden | ParameterInfo::kIsReadOnly,
                                3000 + cc);
    }
    return kResultOk;
}
tresult PLUGIN_API Controller::queryInterface(const char *iid, void **obj) {
    QUERY_INTERFACE(iid, obj, IMidiMapping::iid, IMidiMapping)
    return EditControllerEx1::queryInterface(iid, obj);
}
tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 bus, int16, CtrlNumber cc, ParamID &id) {
    if (bus == 0 && cc == kCtrlProgramChange) {
        id = layout().factoryProgram;
        return kResultTrue;
    }
    if (bus != 0 || cc < 0 || cc >= 128)
        return kResultFalse;
    id = 3000 + cc;
    return kResultTrue;
}
tresult PLUGIN_API Controller::setComponentState(IBStream *stream) {
    Project p;
    if (!read(stream, p))
        return kResultFalse;
    for (size_t i = 0; i < layout().parameters.size(); ++i)
        setParamNormalized(ParamID(i), p.state.values[i]);
    return kResultOk;
}
IPlugView *PLUGIN_API Controller::createView(FIDString type) {
    return type && !strcmp(type, ViewType::kEditor) ? createEditor(this) : nullptr;
}
tresult PLUGIN_API Controller::getParamStringByValue(ParamID id, ParamValue v, String128 text) {
    if (id >= layout().parameters.size())
        return EditControllerEx1::getParamStringByValue(id, v, text);
    auto &p = layout().parameters[id];
    char buf[160];
    if (!p.choices.empty() && p.steps > 0) {
        size_t begin = 0;
        int index = int(std::round(v * p.steps));
        for (int j = 0; j < index; ++j) {
            auto k = p.choices.find('|', begin);
            if (k == std::string::npos)
                break;
            begin = k + 1;
        }
        auto end = p.choices.find('|', begin);
        auto name = p.choices.substr(begin, end == std::string::npos ? end : end - begin);
        snprintf(buf, sizeof(buf), "%s", name.c_str());
    } else
        snprintf(buf, sizeof(buf), "%.9g %s", plain(int(id), float(v)), p.unit.c_str());
    auto wide = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(buf);
    std::fill_n(text, 128, 0);
    for (size_t k = 0; k < std::min(size_t(127), wide.size()); ++k)
        text[k] = TChar(wide[k]);
    return kResultTrue;
}
tresult PLUGIN_API Controller::getParamValueByString(ParamID id, TChar *input, ParamValue &value) {
    if (!input)
        return kResultFalse;
    if (id >= layout().parameters.size())
        return EditControllerEx1::getParamValueByString(id, input, value);
    char text[128]{};
    for (int i = 0; i < 127 && input[i]; ++i)
        text[i] = char(input[i]);
    auto &definition = layout().parameters[id];
    if (!definition.choices.empty() && definition.steps > 0) {
        std::istringstream choices(definition.choices);
        std::string label;
        int i = 0;
        while (std::getline(choices, label, '|')) {
            if (label == text) {
                value = double(i) / definition.steps;
                return kResultTrue;
            }
            ++i;
        }
    }
    char *end;
    double n = strtod(text, &end);
    if (end == text || !std::isfinite(n))
        return kResultFalse;
    auto &p = layout().parameters[id];
    value = clean(float((n - p.minimum) / (p.maximum - p.minimum)));
    return kResultTrue;
}
void Controller::requestMeters() {
    auto *message = allocateMessage();
    if (!message)
        return;
    message->setMessageID("meters");
    sendMessage(message);
    message->release();
}
tresult PLUGIN_API Controller::notify(IMessage *message) {
    if (message && message->getMessageID() && !strcmp(message->getMessageID(), "meters")) {
        double a = 0, b = 0, c = 0;
        message->getAttributes()->getFloat("in", a);
        message->getAttributes()->getFloat("out", b);
        message->getAttributes()->getFloat("gr", c);
        meterIn.store(float(a));
        meterOut.store(float(b));
        meterGR.store(float(c));
        return kResultOk;
    }
    return EditControllerEx1::notify(message);
}
void Controller::loadFile(const char *path, const char *kind) {
    if (!strcmp(kind, "loadPreset")) {
        Project p;
        if (!loadProject(path, p))
            return;
        for (size_t i = 0; i < layout().parameters.size(); ++i) {
            setParamNormalized(ParamID(i), p.state.values[i]);
            beginEdit(ParamID(i));
            performEdit(ParamID(i), p.state.values[i]);
            endEdit(ParamID(i));
        }
    }
    auto *m = allocateMessage();
    if (!m)
        return;
    m->setMessageID(kind);
    m->getAttributes()->setBinary("path", path, uint32(strlen(path)));
    sendMessage(m);
    m->release();
}
} // namespace multifx
using namespace Steinberg;
using namespace Steinberg::Vst;
bool InitModule() {
    return true;
}
bool DeinitModule() {
    return true;
}
BEGIN_FACTORY_DEF("Jerzy Audio", "https://github.com/jurekbak71-dev/JerzyMultiFX", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(multifx::processorId), PClassInfo::kManyInstances, kVstAudioEffectClass,
           "Jerzy MultiFX", Vst::kDistributable, "Fx|Delay|Reverb|Distortion", "0.1.0", kVstVersionString,
           multifx::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(multifx::controllerId), PClassInfo::kManyInstances,
           kVstComponentControllerClass, "Jerzy MultiFX Controller", 0, "", "0.1.0", kVstVersionString,
           multifx::Controller::create)
END_FACTORY
