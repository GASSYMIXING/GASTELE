#include "Dsp.h"
#include "LegacyDsp11.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>
using namespace gastele;
static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){try {
    for(double sr:{8000.,22050.,44100.,48000.,96000.,192000.})for(int style=0;style<3;style++)for(double mix:{0.,.37,1.}) {
        Dsp disabled,enabled;gastele11::Dsp old;RealphoneEq external[2];
        for(unsigned id=0;id<5;id++){double value=defaults[id];if(id==Style)value=style*.5;if(id==Mix)value=mix;if(id==Output)value=.63;disabled.set(id,value);enabled.set(id,value);old.set(id,value);}
        enabled.set(Realphone,1.);disabled.prepare(sr);enabled.prepare(sr);old.prepare(sr);for(auto& eq:external)eq.prepare(sr);
        for(int n=0;n<8192;n++) {
            const double source[]={.2*std::sin(n*.217)+.03*std::cos(n*.021),.15*std::sin(n*.153)};
            double x[]={source[0],source[1]},a[]={source[0],source[1]},b[]={source[0],source[1]};
            old.frame(x,2);disabled.frame(a,2);enabled.frame(b,2);
            for(int c=0;c<2;c++){check(a[c]==x[c],"Realphone OFF must exactly preserve 1.1 audio");const double expected=external[c].tick(x[c]);check(std::abs(expected-b[c])<1e-12,"Realphone must be serial after legacy MIX/OUTPUT");check(std::isfinite(b[c]),"finite serial EQ output");}
        }
    }
    Dsp switched;gastele11::Dsp base;RealphoneEq eq;
    switched.prepare(48000);base.prepare(48000);eq.prepare(48000);double amount=0,target=0;
    const double alpha=1-std::exp(-1/(48000.*.012));
    for(int n=0;n<15000;n++) {
        if(n==128){target=1;switched.set(Realphone,1);}if(n==6000){target=0;switched.set(Realphone,0);}
        amount+=(target-amount)*alpha;if(std::abs(target-amount)<1e-9)amount=target;
        double a[]={.2*std::sin(n*.11),0},b[]={a[0],0};base.frame(a,1);const double filtered=eq.tick(a[0]);switched.frame(b,1);
        check(std::abs(b[0]-(a[0]+(filtered-a[0])*amount))<1e-12,"smooth serial stage on/off transition");
    }
    Dsp bypass;bypass.set(Bypass,1);bypass.set(Realphone,1);bypass.set(Output,1);bypass.prepare(48000);
    for(int n=0;n<8192;n++){double original=.3*std::sin(n*.18),x[]={original,0};bypass.frame(x,2);check(x[0]==original&&x[1]==0,"global bypass includes Realphone and output gain");}
    RealphoneEq silent;silent.prepare(48000);for(int n=0;n<8192;n++)check(silent.tick(0)==0,"EQ must not add noise");
    if(argc>1){std::ofstream file(argv[1]);file<<std::setprecision(17);RealphoneEq impulse;impulse.prepare(48000);for(int n=0;n<8192;n++)file<<impulse.tick(n==0?1.:0.)<<'\n';check(bool(file),"write impulse response");}
    std::cout<<"PASS: exact 1.1 bypass-null, full-chain serial order (all styles and MIX=0/37/100), six sample rates, smooth switching, global bypass, silence.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
