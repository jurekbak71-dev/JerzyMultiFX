#include "core/Engine.h"
#include "core/Presentation.h"
#include "core/Project.h"
#include "vendor/atomic/Dsp.h"
#include "vendor/attack/Engine.h"
#include "vendor/clone/Dsp.h"
#include "vendor/tape/tapedrive_dsp.h"
#include "vendor/oil/Engine.h"
#include "effects/Native.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace multifx;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
State isolated(int block) {
    State s;
    for (int i = 0; i < 5; ++i)
        s.values[layout().blockBypass[i]] = i == block ? 0 : 1;
    s.values[layout().outputLimiter] = 0;
    return s;
}
void impulse(State s, int expected, float amplitude = 1) {
    Engine e;
    e.prepare(48000);
    require(e.prepareState(s), "Warm active source modules outside audio");
    float kernel = .25f;
    require(e.loadIR(&kernel, nullptr, 1), "Load non-identity IR");
    std::array<float, 128> input{}, l{}, r{};
    for (int k = 0; k < 40; ++k)
        e.process(input.data(), input.data(), l.data(), r.data(), 128, s);
    float largest = 0;
    int peak = -1;
    for (int offset = 0; offset < 8192; offset += 128) {
        input.fill(0);
        if (offset == 0)
            input[0] = .1f;
        e.process(input.data(), input.data(), l.data(), r.data(), 128, s);
        for (int i = 0; i < 128; ++i)
            if (std::abs(l[i]) > largest) {
                largest = std::abs(l[i]);
                peak = offset + i;
            }
    }
    require(peak == expected, "Actual impulse latency must match active route");
    require(Engine::latencyFor(s) == expected && e.currentLatency() == expected,
            "Reported latency matches impulse");
    if (amplitude > 0)
        require(std::abs(largest - .1f * amplitude) < .0001f, "IR changes actual audio amplitude");
}
int main() try {
    auto &lay = layout();
    {
        auto settled=[](float trim){mf_attack::Compressor c;c.prepare(48000);float gain=1;
            for(int n=0;n<48000;++n)gain=c.process(.01f,trim);return gain;};
        const float neutral=settled(0);
        require(std::abs(settled(-12)/neutral-db(-12))<.001f &&
                std::abs(settled(12)/neutral-db(12))<.001f,
                "Attack compressor makeup changes actual gain symmetrically around 0 dB");
    }
    {
        State s;
        s.values[parameterId("comp.bypass")] = 0;
        s.values[parameterId("comp.limiter")] = 0;
        s.values[parameterId("comp.amount")] = 0; // Ratio 1: isolate makeup gain.
        auto analog = std::make_unique<Analog>();
        analog->prepare(48000);
        for (const auto [control, expected] :
             {std::pair{0.f, db(-24.f)}, std::pair{.5f, 1.f}, std::pair{1.f, db(24.f)}}) {
            s.values[parameterId("comp.makeup")] = control;
            analog->configure(s);
            float l = .01f, r = .01f;
            for (int n = 0; n < 4800; ++n) {
                l = r = .01f;
                analog->comp(l, r, s);
            }
            if (std::abs(l / .01f - expected) >= .002f)
                throw std::runtime_error("Main compressor makeup: expected " + std::to_string(expected) +
                                         ", got " + std::to_string(l / .01f));
        }
        mf_oil::Compressor oil;
        for (float amount : {-24.f, 0.f, 24.f}) {
            oil.configure(48000, 2, 0, 1, 10, 100, 0, amount);
            float l = .01f, r = .01f;
            oil.process(l, r);
            require(std::abs(l / .01f - db(amount)) < .0001f,
                    "Oil input/output compressor applies signed makeup in DSP");
        }
    }
    {
        Project old;
        std::vector<uint8_t> bytes;
        u32(bytes, 0x31464d4a);
        u32(bytes, 1);
        u32(bytes, 919);
        for (int i = 0; i < 919; ++i) {
            u32(bytes, keyHash(lay.parameters[i].key));
            float v = old.state.values[i];
            if (i == lay.midiMap + 74)
                v = float(lay.preamp + 2) / lay.midiLearn;
            u32(bytes, std::bit_cast<uint32_t>(v));
        }
        u32(bytes, 0);
        u32(bytes, 48000);
        u32(bytes, 0);
        u32(bytes, crc32(bytes.data(), bytes.size()));
        Project migrated;
        require(decode(bytes, migrated), "Read actual version-1 layout");
        require(int(plain(lay.midiMap + 74, migrated.state.values[lay.midiMap + 74])) == lay.preamp + 1,
                "Preserve old CC target after appending parameters");
    }
    State clean;
    impulse(clean, 0);
    for(float next:{.15f,.3f,.6f}){
        mf_attack::PluckDetector detector;detector.prepare(48000);
        int triggers=0;
        for(int n=0;n<48000;++n){
            const float t=n/48000.f;
            float x=.2f*std::exp(-2*t)*std::sin(2*mf_attack::pi*110*t);
            if(t>=next){
                const float u=t-next;
                x+=.12f*std::exp(-3*u)*std::sin(2*mf_attack::pi*164.8f*u);
            }
            triggers+=detector.process(x,.5f);
        }
        require(triggers==2,"Attack Decay detects a new overlapping note only once");
    }
    {
        mf_attack::Engine attack;
        attack.prepare(48000);
        mf_attack::Parameters p;
        p.attack=0;p.decay=1;p.compMono=p.compPoly=false;p.volume=26.f/39;
        double before=0,after=0;
        for(int n=0;n<96000;++n){
            if(n==48000)p.poly=true;
            if(n==72000)p.fuzz=true;
            if(n==84000)p.fuzz=false;
            float l=.15f*std::sin(2*mf_attack::pi*220*n/48000.f),r=l;
            attack.process(l,r,0,0,p);
            require(std::isfinite(l)&&std::abs(l)<2,"Attack Decay remains bounded across mode switches");
            if(n>45000&&n<47000)before+=l*l;
            if(n>49500&&n<51500)after+=l*l;
        }
        require(after>before*.5,"POLY activation must not create an audio gap");
    }
    {
        State s;
        s.values[lay.gateEnabled]=1;
        Analog gate;gate.prepare(48000);gate.configure(s);
        float l=.0004f,r=.0004f;
        for(int n=0;n<48000;++n){l=r=.0004f;gate.noiseGate(l,r);}
        require(std::abs(l)<1e-5f,"Noise gate attenuates quiet input");
        for(int n=0;n<4800;++n){l=r=.2f;gate.noiseGate(l,r);}
        require(l>.15f && r>.15f,"Noise gate opens for a note without lookahead latency");
        for(int n=0;n<48000;++n){l=r=.0004f;gate.noiseGate(l,r);}
        require(std::abs(l)<1e-5f,"Noise gate closes smoothly after release");
    }
    State ir = isolated(4);
    ir.values[lay.ir] = 1;
    impulse(ir, 128, .25);
    State spectral = isolated(1);
    spectral.values[lay.processorBypass[2]] = 0;
    spectral.values[parameterId("attack.blend")] = 0;
    impulse(spectral, attackMonoLatency, -1);
    spectral.values[parameterId("attack.poly")] = 1;
    impulse(spectral, attackLatency, -1);
    spectral = isolated(1);
    spectral.values[lay.processorBypass[3]] = 0;
    spectral.values[parameterId("atomic.blend")] = 0;
    impulse(spectral, atomicLatency, -1);
    spectral.values[lay.blockBypass[1]] = 1;
    impulse(spectral, 0);
    // Independent original engine renders verify that the adapter and chain preserve the source signal.
    for (int engine : {1, 3}) {
        State s = isolated(1);
        s.values[lay.processorBypass[engine]] = 0;
        Engine integrated;
        integrated.prepare(48000);
        require(integrated.prepareState(s), "Warm original comparison module");
        mf_clone::Engine chorus;
        chorus.reset(48000, mf_clone::defaults());
        atomiccluster::Engine atomic;
        atomic.reset(48000, atomiccluster::defaults());
        mf_clone::Values cp{};
        atomiccluster::Values ap{};
        const auto &g = lay.processors[engine];
        for (unsigned k = 0; k < g.api->parameter_count; ++k) {
            if (engine == 1)
                cp[k] = s.values[g.start + k];
            else
                ap[k] = s.values[g.start + k];
        }
        std::array<float, 128> input{}, l{}, r{};
        double maximum = 0;
        for (int offset = 0; offset < 16384; offset += 128) {
            for (int i = 0; i < 128; ++i)
                input[i] = .08f * std::sin((offset + i) * .057f);
            integrated.process(input.data(), input.data(), l.data(), r.data(), 128, s);
            for (int i = 0; i < 128; ++i) {
                auto reference = engine == 1 ? chorus.process(input[i], input[i], cp, 120, NAN)
                                             : atomic.process(input[i], input[i], ap, 120, NAN);
                maximum = std::max(maximum, std::abs(double(l[i]) - reference[0]));
            }
        }
        require(maximum < .00001, "Integrated Chorus/Atomic differs from original DSP at matching controls");
    }
    // Decay must track actual reflection length in every mode/Size.
    for(int mode=0;mode<5;++mode)
        for(float size : {0.f,.5f,1.f}){
            State s;
            s.values[lay.reverb]=float(mode)/4;
            s.values[lay.reverb+3]=size;
            Reverb reverb;reverb.prepare(48000);reverb.configure(s);
            for(int line=0;line<8;++line){
                const double rt60=-6.907755*reverb.lineBase[line]/
                    (48000*std::log(reverb.lineFeedback[line]));
                require(std::abs(rt60-plain(lay.reverb+1,s.values[lay.reverb+1]))<.002,
                        "Reverb Size must not change nominal decay calibration");
            }
        }
    // Algorithm identity: the same impulse must produce different wet paths.
    {
        std::array<std::array<float, 12000>, 5> renders{};
        for(int mode=0;mode<5;++mode){
            State s;
            s.values[lay.reverb]=float(mode)/4;
            s.values[lay.reverb+9]=1;
            Reverb reverb;reverb.prepare(48000);reverb.configure(s);
            for(int n=0;n<12000;++n){
                const auto y=reverb.tick(n==0?.5f:0,n==0?.25f:0,s);
                require(std::isfinite(y[0]) && std::isfinite(y[1]),"Reverb mode finite impulse");
                renders[mode][n]=y[0];
            }
        }
        for(int a=0;a<5;++a)
            for(int b=a+1;b<5;++b){
                double difference=0,energy=0;
                for(int n=0;n<12000;++n){
                    const double delta=renders[a][n]-renders[b][n];
                    difference+=delta*delta;
                    energy+=renders[a][n]*renders[a][n]+renders[b][n]*renders[b][n];
                }
                require(difference>energy*.015,"Reverb engines must have distinct impulse responses");
            }
    }
    // An octave-up shimmer must create 880 Hz energy from a 440 Hz input.
    {
        auto overtone=[&](float feedback){
            State s;
            s.values[lay.reverb]=1;
            s.values[lay.reverb+9]=1;
            s.values[lay.reverb+11]=1;
            s.values[lay.reverb+12]=feedback;
            Reverb reverb;reverb.prepare(48000);reverb.configure(s);
            double energy=0;
            std::array<float,48000> capture{};
            for(int n=0;n<144000;++n){
                const float input=n<96000?.2f*std::sin(6.283185307179586*440*n/48000):0;
                const auto y=reverb.tick(input,input,s);
                require(std::isfinite(y[0]) && std::abs(y[0])<2,"Shimmer finite headroom");
                if(n>=48000 && n<96000){
                    capture[n-48000]=y[0];
                    energy+=y[0]*y[0];
                }
            }
            require(energy>1e-5,"Shimmer produces wet output");
            reverb.reset();
            for(int n=0;n<24000;++n){
                const auto y=reverb.tick(0,0,s);
                require(y[0]==0 && y[1]==0,"Shimmer reset invalidates pitch memory");
            }
            double band=0;
            for(int frequency=840;frequency<=920;frequency+=4){
                double real=0,imag=0;
                for(int n=0;n<48000;++n){
                    const double angle=6.283185307179586*frequency*n/48000;
                    real+=capture[n]*std::cos(angle);
                    imag+=capture[n]*std::sin(angle);
                }
                band+=real*real+imag*imag;
            }
            return std::sqrt(band)/48000;
        };
        const double dryOvertone=overtone(0);
        const double shimmerOvertone=overtone(.8f);
        require(shimmerOvertone>.0002 && shimmerOvertone>dryOvertone*4,
                "Shimmer must audibly shift feedback by an octave");
    }
    // Regression coverage for the new WARM and CLOUD algorithms.
    for(double rate : {44100.,48000.,96000.}) {
        double harmonic[2]{};
        for(int setting=0;setting<2;++setting){
            JerzyAudio::TapeDriveDSP<double> tape;
            tape.prepare(rate);
            JerzyAudio::TapeDriveParams p;
            p.sat=double(setting); p.level=2./3.; p.shift=0.;
            p.optoBypass=1.; p.dry=0.;
            double real[2]{},imag[2]{};
            for(int n=0;n<int(rate)*2;++n){
                double x=.18*std::sin(6.283185307179586*375.*n/rate),y=0.;
                const double* input[]{&x}; double* output[]{&y};
                double a,b,c,d;
                tape.process(input,output,1,1,p,a,b,c,d);
                require(std::isfinite(y) && std::abs(y)<1., "Warm finite headroom");
                if(n>=int(rate))
                    for(int h=0;h<2;++h){
                        const double phase=6.283185307179586*375.*(h+2)*n/rate;
                        real[h]+=y*std::cos(phase); imag[h]+=y*std::sin(phase);
                    }
            }
            harmonic[setting]=(std::hypot(real[0],imag[0])+std::hypot(real[1],imag[1]))/rate;
        }
        require(harmonic[1]>.0001 && harmonic[1]>harmonic[0]*5.,
                "Warm adds measurable low-order harmonics without hard clipping");
        State state;
        state.values[lay.reverb]=.75f; // Cloud
        state.values[lay.reverb+9]=1.f;
        Reverb cloud;
        cloud.prepare(rate); cloud.configure(state);
        double energy=0,stereoDifference=0;
        int occupied=0;
        for(int n=0;n<int(rate)*3;++n){
            auto y=cloud.tick(n==0?.2f:0.f,n==0?.2f:0.f,state);
            require(std::isfinite(y[0]) && std::isfinite(y[1]) &&
                    std::abs(y[0])<2.f && std::abs(y[1])<2.f,"Cloud stable impulse tail");
            if(n>int(rate)/2){
                energy+=double(y[0])*y[0]+double(y[1])*y[1];
                stereoDifference+=double(y[0]-y[1])*(y[0]-y[1]);
                occupied+=std::abs(y[0])+std::abs(y[1])>1e-9f;
            }
        }
        require(energy>1e-10 && occupied>int(rate),"Cloud dense sustained late tail");
        require(stereoDifference>energy*.01,"Cloud stereo decorrelation");
        cloud.reset();
        for(int n=0;n<int(rate)/2;++n){
            auto y=cloud.tick(0,0,state);
            require(y[0]==0 && y[1]==0,"Cloud reset invalidates all diffusion memories");
        }
    }
    // Every block must return the same zero-latency clean signal when bypassed.
    for (int block = 0; block < 5; ++block) {
        State s = isolated(block);
        s.values[lay.blockBypass[block]] = 1;
        s.values[lay.drive + 5] = 0;
        s.values[lay.reverb + 9] = .8f;
        s.values[lay.ir] = 1;
        impulse(s, 0);
    }
    // Limiter must protect physical outputs, while clean signals remain unchanged.
    {
        Engine e;
        e.prepare(48000);
        State s;
        std::array<float, 128> in{}, l{}, r{};
        in.fill(2);
        e.process(in.data(), in.data(), l.data(), r.data(), 128, s);
        for (float x : l)
            require(std::abs(x) <= .98001f, "Output limiter ceiling");
        require(e.clips.load() > 0, "Clips before limiter counted");
    }
    std::cout << "PASS real signal: clean0 / IR128 / Attack MONO16 POLY2064 / Atomic2048, all block bypasses0; original "
                 "Chorus/Atomic sample comparison; physical-output limiter\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
}
