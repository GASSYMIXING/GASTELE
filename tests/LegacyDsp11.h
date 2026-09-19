#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace gastele11 {
constexpr double pi = 3.14159265358979323846;
enum Param : unsigned { Bypass, Character, Style, Mix, Output, Count, Meter=100 };
inline constexpr std::array<double,Count> defaults {0., .65, 0., 1., .5};
inline double normalized(double v) { return std::isfinite(v) ? std::clamp(v,0.,1.) : 0.; }
struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void configure(double sr,double freq,bool high) {
        const double w=2*pi*std::min(freq,sr*.42)/sr,c=std::cos(w),s=std::sin(w),alpha=s/std::sqrt(2.),a0=1+alpha;
        b0=(high ? 1+c:1-c)*.5/a0;
        b1=(high ? -(1+c):1-c)/a0;b2=b0;a1=-2*c/a0;a2=(1-alpha)/a0;
        z1=z2=0;
    }
    double tick(double x) {
        double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;
        if(std::abs(z1)<1e-25) z1=0;if(std::abs(z2)<1e-25) z2=0;
        return y;
    }
};
struct Voice {
    Biquad hp,lp,post; double phase=1,held=0,envelope=0; uint32_t random=0x125789ab;
    void prepare(double sr,int type) {
        constexpr double highs[]={320,450,230},lows[]={3400,2800,4300};
        hp.configure(sr,highs[type],true);lp.configure(sr,lows[type],false);post.configure(sr,lows[type],false);
        phase=1;held=envelope=0;random=0x125789ab;
    }
    double tick(double x,double amount,double sr,int type) {
        const double band=lp.tick(hp.tick(x));
        const double drive=1+amount*(type==0?5.:type==1?8.:11.);
        double y=std::tanh(band*drive)/(1+amount*1.25);
        const double rates[]={16000,12000,18000};
        phase+=std::min(1.,(rates[type]-(type==1?6000:6500)*amount)/sr);
        if(phase>=1) { phase-=std::floor(phase);held=y; }
        const double levels=std::pow(2.,(type==1?12.:14.)-amount*(type==1?7.:6.));
        const double crushed=std::round(held*levels)/levels;
        y=y*(1-amount)+crushed*amount;
        envelope=std::max(std::abs(band),envelope*.9995);
        random^=random<<13;random^=random>>17;random^=random<<5;
        const double noise=(double(random)/4294967295.*2-1)*amount*amount*(type==2?.008:.0015)*std::min(1.,envelope*30);
        return post.tick(y+noise);
    }
};
class Dsp {
public:
    std::array<double,Count> target=defaults;
    void prepare(double sampleRate) {
        sr=std::max(8000.,sampleRate); smooth=1-std::exp(-1/(sr*.012));
        current=target;weights={0,0,0};weights[styleIndex()]=1;
        for(int c=0;c<2;c++) for(int t=0;t<3;t++) voices[c][t].prepare(sr,t);
        peak=0;
    }
    void set(unsigned id,double value) { if(id<Count) target[id]=normalized(value); }
    int styleIndex()const {return std::clamp(int(std::round(target[Style]*2)),0,2);}
    void frame(double* values,int channels) {
        for(unsigned i=0;i<Count;i++) {
            current[i]+=(target[i]-current[i])*smooth;
            if(std::abs(target[i]-current[i])<1e-9) current[i]=target[i];
        }
        const int selected=styleIndex();
        for(int t=0;t<3;t++) weights[t]+=((t==selected?1.:0.)-weights[t])*smooth;
        const double wet=current[Mix]*(1-current[Bypass]);
        const double gain=std::pow(10.,(-24+48*current[Output])/20.);
        // The global bypass crossfades the whole effect, including output gain.
        const double finalGain=gain+(1-gain)*current[Bypass];
        peak*=.9995;
        for(int c=0;c<channels;c++) {
            const double dry=std::isfinite(values[c]) ? values[c] : 0.;
            double processed=0;
            for(int t=0;t<3;t++) processed+=voices[c][t].tick(std::clamp(dry,-16.,16.),current[Character],sr,t)*weights[t];
            values[c]=(dry+(processed-dry)*wet)*finalGain;
            if(!std::isfinite(values[c])) values[c]=0;
            peak=std::max(peak,std::abs(values[c]));
        }
    }
    double level()const {return std::clamp(peak,0.,1.);}
private:
    double sr=44100,smooth=.002,peak=0;
    std::array<double,Count> current=defaults;
    std::array<double,3> weights{1,0,0};
    Voice voices[2][3];
};
}
