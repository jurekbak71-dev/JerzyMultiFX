#pragma once
#include "Catalog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>

namespace muff::circuit {
// Global nodal transient solution. All four transistors, clipping branches,
// pots, coupling capacitors and the output load participate in the same solve.
class AnalogCircuit {
public:
    static constexpr int maxNodes = 32;
    static constexpr int ground = -1, supply = -2, source = -3;
    static constexpr double vt = 8.617333262145e-5*(273.15+temperatureC); // kT/q
    struct Resistor { int a, b; double r; };
    struct Capacitor { int a, b; double c, voltage = 0, current = 0; };
    struct Transistor { int b, e, c; double beta, inverseBeta; };
    struct DiodePair { int a, b; };
    struct Pot { int top, wiper, bottom; double total; int control; bool logarithmic; };
    struct Statistics { uint64_t samples=0, iterations=0, failures=0, refinedSamples=0; int worstIterations=0; };
private:
    using Vector = std::array<double,maxNodes>;
    static constexpr int maxBand=4;
    struct Matrix {
        std::array<std::array<double,2*maxBand+1>,maxNodes> data{};
        struct Row {
            double* values; size_t row;
            double& operator[](size_t col) {return values[static_cast<size_t>(int(col)-int(row)+maxBand)];}
        };
        Row operator[](size_t row){return {data[row].data(),row};}
    };
    std::array<Resistor,80> resistors{}; int nr=0;
    std::array<Capacitor,40> capacitors{}; int nc=0;
    std::array<Transistor,4> transistors{}; int nq=0;
    std::array<DiodePair,2> diodes{}; int nd=0;
    std::array<Pot,4> pots{}; int np=0;
    int nodes=0, band=0, output=0;
    size_t revisionIndex=0;
    double fs=192000, rail=9, sign=1;
    double sustain=.5, tone=.5, volume=.5;
    Matrix passive{};
    Vector constant{}, inputCoefficient{}, state{}, previousState{};
    bool prepared=false, dcMode=false;
    double lastInput=0,previousDt=1./192000;
    unsigned controlTick=0;
    Statistics stats{};

    int node() { return nodes++; }
    void linkBand(int a,int b) { if(a>=0&&b>=0) band=std::max(band,std::abs(a-b)); }
    void r(int a,int b,double value) { resistors[nr++]={a,b,value};linkBand(a,b); }
    void c(int a,int b,double value) {
        if(value<=0)return;
        capacitors[nc++]={a,b,value,0,0};linkBand(a,b);
    }
    int transistor(const Revision& spec,int stage) {
        const int b=node(),e=node(),col=node();
        transistors[nq++]={b,e,col,spec.beta[static_cast<size_t>(stage)],1/spec.beta[static_cast<size_t>(stage)]};
        linkBand(b,col);
        r(b,ground,spec.rb[static_cast<size_t>(stage)]);
        r(e,ground,spec.re[static_cast<size_t>(stage)]);
        r(col,supply,spec.rc[static_cast<size_t>(stage)]);
        r(b,col,spec.rf[static_cast<size_t>(stage)]);
        // Explicit constant junction capacitances, also present in the reference netlist.
        c(b,e,junctionBE);c(b,col,junctionBC);
        if(stage<3)c(b,col,spec.cFeedback[static_cast<size_t>(stage)]);
        return b;
    }
    void pot(int top,int wiper,int bottom,int control,bool logarithmic) {
        pots[np++]={top,wiper,bottom,100000,control,logarithmic};
        linkBand(top,wiper);linkBand(wiper,bottom);
    }
    void build(const Revision& s) {
        nr=nc=nq=nd=np=nodes=band=0;
        const int input=node(), afterInputR=node();
        r(source,input,inputSourceResistance);r(input,afterInputR,s.rin);
        c(input,afterInputR,s.inputBypassCap);
        const int first=transistor(s,0);
        c(afterInputR,first,s.cIn);
        int feed=node();c(first+2,feed,s.cCouple1);
        if(s.postInputLoad>0)r(feed,ground,s.postInputLoad);
        if(!s.boost) {
            const int bottom=node(),wiper=node();
            r(bottom,ground,s.sustainFoot);pot(feed,wiper,bottom,0,s.sustainLog);
            int previous=wiper;
            for(int stage=1;stage<=2;++stage) {
                const int coupled=node();c(previous,coupled,stage==1?s.cCouple2:s.cCouple3);
                const int b=transistor(s,stage);
                r(coupled,b,s.rclip[static_cast<size_t>(stage-1)]);
                if(s.clip[static_cast<size_t>(stage-1)]) {
                    const int junction=node();c(b,junction,s.cClip[static_cast<size_t>(stage-1)]);
                    diodes[nd++]={junction,b+2};linkBand(junction,b+2);
                    c(junction,b+2,2*s.diodeC);
                }
                previous=b+2;
            }
            feed=previous;
            if(s.toneInputCap>0){const int isolated=node();c(feed,isolated,s.toneInputCap);feed=isolated;}
        }
        const int hp=node(),lp=node(),wiper=node();
        c(feed,hp,s.toneC[0]);r(hp,ground,s.toneR[0]);
        r(feed,lp,s.toneR[1]);c(lp,ground,s.toneC[1]);
        pot(s.toneReversed?lp:hp,wiper,s.toneReversed?hp:lp,1,s.toneLog);
        int outputFeed=wiper;
        if(s.volumeBeforeOutput) {
            outputFeed=node();pot(wiper,outputFeed,ground,2,s.volumeLog);
        }
        const int last=transistor(s,3);
        c(outputFeed,last,s.cToneOutput);
        const int coupled=node();c(last+2,coupled,s.cOutput);
        if(s.volumeBeforeOutput) {
            output=coupled;r(output,ground,s.outputShunt);
        } else {
            output=node();pot(coupled,output,ground,2,s.volumeLog);
        }
        r(output,ground,outputLoadResistance);
    }
    double known(int n) const { return n==supply?rail:0; }
    double voltage(int n,const Vector& x,double input) const {
        return n>=0?x[static_cast<size_t>(n)]:(n==source?input:known(n));
    }
    void stampLinear(int a,int b,double g) {
        if(a>=0) {
            passive[static_cast<size_t>(a)][static_cast<size_t>(a)]+=g;
            if(b>=0)passive[static_cast<size_t>(a)][static_cast<size_t>(b)]-=g;
            else if(b==source)inputCoefficient[static_cast<size_t>(a)]+=g;
            else constant[static_cast<size_t>(a)]+=g*known(b);
        }
        if(b>=0) {
            passive[static_cast<size_t>(b)][static_cast<size_t>(b)]+=g;
            if(a>=0)passive[static_cast<size_t>(b)][static_cast<size_t>(a)]-=g;
            else if(a==source)inputCoefficient[static_cast<size_t>(b)]+=g;
            else constant[static_cast<size_t>(b)]+=g*known(a);
        }
    }
    static double taper(double x,bool log) {
        x=std::clamp(x,0.,1.);return log?std::pow(x,3.321928094887362):x;
    }
    void rebuild() {
        passive={};constant={};inputCoefficient={};
        for(int i=0;i<nodes;++i)passive[static_cast<size_t>(i)][static_cast<size_t>(i)]=1e-12;
        for(int i=0;i<nr;++i){auto a=resistors[static_cast<size_t>(i)];stampLinear(a.a,a.b,1/a.r);}
        if(!dcMode)for(int i=0;i<nc;++i){auto a=capacitors[static_cast<size_t>(i)];stampLinear(a.a,a.b,2*a.c*fs);}
        for(int i=0;i<np;++i) {
            const auto& a=pots[static_cast<size_t>(i)];
            double x=taper(a.control==0?sustain:(a.control==1?tone:volume),a.logarithmic);
            stampLinear(a.top,a.wiper,1/(1+a.total*(1-x)));
            stampLinear(a.wiper,a.bottom,1/(1+a.total*x));
        }
    }
    // No-pivot band elimination is fast for this grounded circuit. Reject tiny
    // pivots rather than returning a NaN; convergence failures are counted.
    template<int bandwidth> bool solveBanded(Matrix& a,Vector& b) const {
        Vector inverse{};
        for(int k=0;k<nodes;++k) {
            const double pivot=a[static_cast<size_t>(k)][static_cast<size_t>(k)];
            if(!std::isfinite(pivot)||std::abs(pivot)<1e-18)return false;
            const double reciprocal=1/pivot;inverse[static_cast<size_t>(k)]=reciprocal;
            const int end=std::min(nodes,k+bandwidth+1);
            for(int i=k+1;i<end;++i) {
                double m=a[static_cast<size_t>(i)][static_cast<size_t>(k)]*reciprocal;
                if(m==0)continue;
                for(int j=k+1;j<end;++j)a[static_cast<size_t>(i)][static_cast<size_t>(j)]-=m*a[static_cast<size_t>(k)][static_cast<size_t>(j)];
                b[static_cast<size_t>(i)]-=m*b[static_cast<size_t>(k)];
            }
        }
        for(int i=nodes-1;i>=0;--i) {
            double value=b[static_cast<size_t>(i)];
            for(int j=i+1;j<std::min(nodes,i+bandwidth+1);++j)value-=a[static_cast<size_t>(i)][static_cast<size_t>(j)]*b[static_cast<size_t>(j)];
            b[static_cast<size_t>(i)]=value*inverse[static_cast<size_t>(i)];
            if(!std::isfinite(b[static_cast<size_t>(i)]))return false;
        }
        return true;
    }
    template<int offset> static void eliminate3(Matrix& a,Vector& b,int k,double inverse) {
        auto& row=a.data[static_cast<size_t>(k+offset)];const auto& pivot=a.data[static_cast<size_t>(k)];
        double m=row[static_cast<size_t>(maxBand-offset)]*inverse;
        if(m==0)return;
        row[static_cast<size_t>(maxBand+1-offset)]-=m*pivot[maxBand+1];
        row[static_cast<size_t>(maxBand+2-offset)]-=m*pivot[maxBand+2];
        row[static_cast<size_t>(maxBand+3-offset)]-=m*pivot[maxBand+3];
        b[static_cast<size_t>(k+offset)]-=m*b[static_cast<size_t>(k)];
    }
    bool solve3(Matrix& a,Vector& b) const {
        Vector inverse{};
        for(int k=0;k<nodes;++k){
            double pivot=a.data[static_cast<size_t>(k)][maxBand];if(!std::isfinite(pivot)||std::abs(pivot)<1e-18)return false;
            double reciprocal=1/pivot;inverse[static_cast<size_t>(k)]=reciprocal;
            if(k+3<nodes){eliminate3<1>(a,b,k,reciprocal);eliminate3<2>(a,b,k,reciprocal);eliminate3<3>(a,b,k,reciprocal);}
            else for(int i=k+1;i<nodes;++i){double m=a[static_cast<size_t>(i)][static_cast<size_t>(k)]*reciprocal;for(int j=k+1;j<nodes;++j)a[static_cast<size_t>(i)][static_cast<size_t>(j)]-=m*a[static_cast<size_t>(k)][static_cast<size_t>(j)];b[static_cast<size_t>(i)]-=m*b[static_cast<size_t>(k)];}
        }
        for(int i=nodes-1;i>=0;--i){
            const auto& row=a.data[static_cast<size_t>(i)];double v=b[static_cast<size_t>(i)];
            if(i+3<nodes)v-=row[maxBand+1]*b[static_cast<size_t>(i+1)]+row[maxBand+2]*b[static_cast<size_t>(i+2)]+row[maxBand+3]*b[static_cast<size_t>(i+3)];
            else for(int j=i+1;j<nodes;++j)v-=a[static_cast<size_t>(i)][static_cast<size_t>(j)]*b[static_cast<size_t>(j)];
            b[static_cast<size_t>(i)]=v*inverse[static_cast<size_t>(i)];if(!std::isfinite(b[static_cast<size_t>(i)]))return false;
        }
        return true;
    }
    bool solve(Matrix& a,Vector& b) const {
        if(band==3)return solve3(a,b);
        if(band==2)return solveBanded<2>(a,b);
        return solveBanded<4>(a,b);
    }
public:
    static double exponential(double v) {
        // Range reduction + degree-8 Taylor polynomial; relative error < 4e-10
        // on [-80,40]. No approximate device curve or lookup-table clipping.
        v=std::clamp(v,-80.,40.);
        double scaled=v*1.4426950408889634074;
        int k=int(scaled+(scaled>=0?.5:-.5));
        double x=v-k*.69314718055994530942;
        double p=1+x*(1+x*(.5+x*(1./6+x*(1./24+x*(1./120+x*(1./720+x*(1./5040+x/40320)))))));
        uint64_t bits=uint64_t(k+1023)<<52;double scale;
        std::memcpy(&scale,&bits,sizeof(scale));return p*scale;
    }
private:
    void nonlinear(Matrix& m,Vector& rhs,const Vector& x) const {
        for(int q=0;q<nq;++q) {
            auto t=transistors[static_cast<size_t>(q)];
            const double be=sign*(x[static_cast<size_t>(t.b)]-x[static_cast<size_t>(t.e)]);
            const double bc=sign*(x[static_cast<size_t>(t.b)]-x[static_cast<size_t>(t.c)]);
            const double ef=exponential(be/vt),er=exponential(bc/vt);
            const double f=saturationCurrent*(ef-1),r=saturationCurrent*(er-1),gf=saturationCurrent*ef/vt,gr=saturationCurrent*er/vt;
            const double invBeta=t.inverseBeta,invReverse=1/reverseBeta;
            double vbe=x[static_cast<size_t>(t.b)]-x[static_cast<size_t>(t.e)];
            double vbc=x[static_cast<size_t>(t.b)]-x[static_cast<size_t>(t.c)];
            double forward=gf*vbe-sign*f,reverse=gr*vbc-sign*r;
            // Gummel-Poon base-width modulation with VAF=100V, no IKF rolloff.
            double early=1-bc/forwardEarlyVoltage,transport=(f-r)*early;
            double gbe=gf*early,gbc=-gr*early-(f-r)/forwardEarlyVoltage;
            double transportEquivalent=gbe*vbe+gbc*vbc-sign*transport;
            auto b=static_cast<size_t>(t.b),e=static_cast<size_t>(t.e),c=static_cast<size_t>(t.c);
            m[b][b]+=gf*invBeta+gr*invReverse;m[b][e]-=gf*invBeta;m[b][c]-=gr*invReverse;
            m[e][b]+=-gbe-gbc-gf*invBeta;m[e][e]+=gbe+gf*invBeta;m[e][c]+=gbc;
            m[c][b]+=gbe+gbc-gr*invReverse;m[c][e]-=gbe;m[c][c]+=-gbc+gr*invReverse;
            rhs[b]+=invBeta*forward+invReverse*reverse;
            rhs[e]+=-transportEquivalent-invBeta*forward;
            rhs[c]+=transportEquivalent-invReverse*reverse;
        }
        const auto& s=revisions[revisionIndex];
        for(int i=0;i<nd;++i) {
            auto d=diodes[static_cast<size_t>(i)];
            double v=x[static_cast<size_t>(d.a)]-x[static_cast<size_t>(d.b)];
            double f=exponential(v/(s.diodeN*vt)),r=exponential(-v/(s.diodeN*vt));
            double current=s.diodeIs*(f-r),g=s.diodeIs*(f+r)/(s.diodeN*vt),eq=g*v-current;
            m[static_cast<size_t>(d.a)][static_cast<size_t>(d.a)]+=g;m[static_cast<size_t>(d.b)][static_cast<size_t>(d.b)]+=g;
            m[static_cast<size_t>(d.a)][static_cast<size_t>(d.b)]-=g;m[static_cast<size_t>(d.b)][static_cast<size_t>(d.a)]-=g;
            rhs[static_cast<size_t>(d.a)]+=eq;rhs[static_cast<size_t>(d.b)]-=eq;
        }
    }
    double damping(const Vector& a,const Vector& b) const {
        double largest=0;
        for(int i=0;i<nq;++i){auto q=transistors[static_cast<size_t>(i)];for(int n:{q.e,q.c}){double oldV=sign*(a[static_cast<size_t>(q.b)]-a[static_cast<size_t>(n)]),newV=sign*(b[static_cast<size_t>(q.b)]-b[static_cast<size_t>(n)]);if(std::max(oldV,newV)>.2)largest=std::max(largest,std::abs(newV-oldV));}}
        for(int i=0;i<nd;++i){auto d=diodes[static_cast<size_t>(i)];largest=std::max(largest,std::abs((b[static_cast<size_t>(d.a)]-b[static_cast<size_t>(d.b)])-(a[static_cast<size_t>(d.a)]-a[static_cast<size_t>(d.b)])));}
        return largest>.10?.10/largest:1.;
    }
    bool step(double input,int maximum) {
        Vector target=constant;
        for(int i=0;i<nodes;++i)target[static_cast<size_t>(i)]+=inputCoefficient[static_cast<size_t>(i)]*input;
        if(!dcMode)for(int i=0;i<nc;++i){auto a=capacitors[static_cast<size_t>(i)];double hist=-2*a.c*fs*a.voltage-a.current;if(a.a>=0)target[static_cast<size_t>(a.a)]-=hist;if(a.b>=0)target[static_cast<size_t>(a.b)]+=hist;}
        Vector x=state;
        if(!dcMode)for(int i=0;i<nodes;++i)x[static_cast<size_t>(i)]+=std::clamp((state[static_cast<size_t>(i)]-previousState[static_cast<size_t>(i)])/(fs*previousDt),-.5,.5);
        for(int iteration=0;iteration<maximum;++iteration) {
            Matrix jac=passive;Vector next=target;nonlinear(jac,next,x);
            if(!solve(jac,next))return false;
            double factor=damping(x,next),delta=0;
            for(int i=0;i<nodes;++i){double change=(next[static_cast<size_t>(i)]-x[static_cast<size_t>(i)])*factor;x[static_cast<size_t>(i)]+=change;delta=std::max(delta,std::abs(change));}
            if(!dcMode)++stats.iterations;
            if(delta<(dcMode?1e-11:1e-7)) {
                if(!dcMode){previousState=state;previousDt=1/fs;}
                state=x;
                if(!dcMode){stats.worstIterations=std::max(stats.worstIterations,iteration+1);for(int i=0;i<nc;++i){auto& a=capacitors[static_cast<size_t>(i)];double v=voltage(a.a,x,input)-voltage(a.b,x,input);a.current=2*a.c*fs*(v-a.voltage)-a.current;a.voltage=v;}}
                return true;
            }
        }
        return false;
    }
public:
    bool prepare(double sampleRate,size_t model,double s=.5,double t=.5,double v=.5) {
        if(sampleRate<8000||model>=revisions.size())return false;
        fs=sampleRate;revisionIndex=model;sustain=s;tone=t;volume=v;
        const auto& spec=revisions[model];sign=spec.pnp?-1.:1.;build(spec);state={};
        if(nodes>maxNodes||band>maxBand)return false;
        dcMode=true;rail=0;rebuild();
        for(int i=0;i<=90;++i){rail=sign*nominalSupply*i/90;rebuild();if(!step(0,100)){prepared=false;return false;}}
        for(int i=0;i<nc;++i){auto& a=capacitors[static_cast<size_t>(i)];a.voltage=voltage(a.a,state,0)-voltage(a.b,state,0);a.current=0;}
        previousState=state;previousDt=1/fs;lastInput=0;dcMode=false;rebuild();stats={};controlTick=0;prepared=true;return true;
    }
    float process(float input,float s,float t,float v) {
        if(!prepared)return 0;
        if((controlTick++&31u)==0u&&(std::abs(s-sustain)>1e-6||std::abs(t-tone)>1e-6||std::abs(v-volume)>1e-6)){sustain=s;tone=t;volume=v;rebuild();}
        ++stats.samples;
        double in=std::isfinite(input)?input:0;
        // Resolve rapid, large input changes on two/four substeps. The capacitor
        // currents/voltages are continuous when the trapezoidal step changes.
        double change=std::abs(in-lastInput);
        if(change>.006) {
            const double normalFs=fs;int subdivisions=change>.012?4:2;
            fs*=subdivisions;rebuild();++stats.refinedSamples;bool ok=true;
            for(int j=1;j<=subdivisions;++j)ok=step(lastInput+(in-lastInput)*j/subdivisions,60)&&ok;
            fs=normalFs;rebuild();if(!ok)++stats.failures;
        } else if(!step(in,40))++stats.failures;
        lastInput=in;
        double y=state[static_cast<size_t>(output)];
        return static_cast<float>(std::isfinite(y)?y:0);
    }
    const Statistics& statistics() const{return stats;}
    int nodeCount() const{return nodes;}
    double nodeVoltage(int n) const{return state[static_cast<size_t>(n)];}
    void writeSpice(std::ostream& out,double frequency,double amplitude,double duration) const {
        auto name=[](int n){return n==ground?std::string("0"):(n==supply?std::string("rail"):(n==source?std::string("input"):"n"+std::to_string(n)));};
        out<<"* Global reference for "<<revisions[revisionIndex].name<<"\nVrail rail 0 "<<rail<<"\nVinput input 0 SIN(0 "<<amplitude<<" "<<frequency<<") AC 1\n";
        for(int i=0;i<nr;++i){auto a=resistors[static_cast<size_t>(i)];out<<"R"<<i<<" "<<name(a.a)<<" "<<name(a.b)<<" "<<a.r<<"\n";}
        for(int i=0;i<nodes;++i)out<<"Rgmin"<<i<<" n"<<i<<" 0 1e12\n";
        for(int i=0;i<nc;++i){auto a=capacitors[static_cast<size_t>(i)];out<<"C"<<i<<" "<<name(a.a)<<" "<<name(a.b)<<" "<<a.c<<"\n";}
        for(int i=0;i<np;++i){auto a=pots[static_cast<size_t>(i)];double x=taper(a.control==0?sustain:(a.control==1?tone:volume),a.logarithmic);out<<"Rpt"<<i<<" "<<name(a.top)<<" "<<name(a.wiper)<<" "<<1+a.total*(1-x)<<"\nRpb"<<i<<" "<<name(a.wiper)<<" "<<name(a.bottom)<<" "<<1+a.total*x<<"\n";}
        for(int i=0;i<nq;++i){auto q=transistors[static_cast<size_t>(i)];out<<"Q"<<i<<" "<<name(q.c)<<" "<<name(q.b)<<" "<<name(q.e)<<" BJT"<<i<<"\n.model BJT"<<i<<" "<<(sign>0?"NPN":"PNP")<<"(IS="<<saturationCurrent<<" BF="<<q.beta<<" BR="<<reverseBeta<<" NF=1 NR=1 VAF="<<forwardEarlyVoltage<<" TNOM="<<temperatureC<<" CJE=0 CJC=0 TF=0 TR=0)\n";}
        const auto& s=revisions[revisionIndex];out<<".model Dnom D(IS="<<s.diodeIs<<" N="<<s.diodeN<<" RS=0 CJO=0 TT=0)\n";
        for(int i=0;i<nd;++i){auto d=diodes[static_cast<size_t>(i)];out<<"Dp"<<i<<" "<<name(d.a)<<" "<<name(d.b)<<" Dnom\nDn"<<i<<" "<<name(d.b)<<" "<<name(d.a)<<" Dnom\n";}
        out<<".temp "<<temperatureC<<"\n.options reltol=1e-6 abstol=1e-12 vntol=1e-8 method=trap\n.control\nset wr_singlescale\nset wr_vecnames\ntran "<<1/(fs*8)<<" "<<duration<<"\nwrdata spice.csv v("<<name(output)<<")\nac dec 80 20 20000\nwrdata ac.csv v("<<name(output)<<")\n.endc\n.end\n";
    }
};
}
