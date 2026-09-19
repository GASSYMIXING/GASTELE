#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace gastele {
// Approximation fitted to the user-supplied Pro-Q screenshot at 48 kHz.
// The high-cut's 3568 Hz / Q .740 / 24 dB/oct is explicitly visible.
// Other values are estimates of the combined curve, not recovered preset data.
class RealphoneEq {
public:
    struct Bell {double hz,gainDb,q;};
    inline static constexpr double lowCutHz=523.,lowCutQ=.729;
    inline static constexpr double highCutHz=3568.,highCutQ=.740;
    inline static constexpr std::array<Bell,3> bells {{{948.,4.45,2.923},{1610.,6.20,2.354},{2654.,5.85,4.842}}};
    void prepare(double sampleRate) {
        const double sr=std::max(8000.,sampleRate);
        sections[0].cut(sr,lowCutHz,lowCutQ,true);sections[1].cut(sr,lowCutHz,lowCutQ,true);
        for(int i=0;i<3;i++)sections[i+2].bell(sr,bells[i]);
        sections[5].cut(sr,highCutHz,highCutQ,false);sections[6].cut(sr,highCutHz,highCutQ,false);
    }
    double tick(double input) {
        double x=std::isfinite(input)?input:0.;
        for(auto& section:sections)x=section.tick(x);
        if(!std::isfinite(x)){for(auto& section:sections)section.z1=section.z2=0.;return 0.;}
        return x;
    }
private:
    struct Section {
        double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        void cut(double sr,double hz,double q,bool high) {
            const double w=2*3.14159265358979323846*std::min(hz,sr*.42)/sr;
            const double c=std::cos(w),alpha=std::sin(w)/(2*q),a0=1+alpha;
            b0=(high?1+c:1-c)*.5/a0;b1=(high?-(1+c):1-c)/a0;b2=b0;a1=-2*c/a0;a2=(1-alpha)/a0;z1=z2=0;
        }
        void bell(double sr,const Bell& band) {
            const double w=2*3.14159265358979323846*std::min(band.hz,sr*.42)/sr;
            const double c=std::cos(w),alpha=std::sin(w)/(2*band.q),A=std::pow(10.,band.gainDb/40),a0=1+alpha/A;
            b0=(1+alpha*A)/a0;b1=-2*c/a0;b2=(1-alpha*A)/a0;a1=b1;a2=(1-alpha/A)/a0;z1=z2=0;
        }
        double tick(double x) {
            const double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;
            if(std::abs(z1)<1e-25)z1=0;if(std::abs(z2)<1e-25)z2=0;return y;
        }
    };
    std::array<Section,7> sections;
};
}
