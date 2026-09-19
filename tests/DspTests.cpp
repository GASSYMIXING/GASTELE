#include "Dsp.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <chrono>
using namespace gastele;
void require(bool ok,const char* description){if(!ok)throw std::runtime_error(description);}
double rms(double freq,int style) {
    Dsp d;d.target[Character]=0;d.target[Style]=style*.5;d.prepare(48000);double sum=0;
    for(int n=0;n<48000;n++){double x[2]={.05*std::sin(2*pi*freq*n/48000),0};d.frame(x,1);if(n>=24000)sum+=x[0]*x[0];}
    return std::sqrt(sum/24000);
}
int main(){try{
    for(double sr:{8000.,22050.,44100.,48000.,96000.,192000.})for(int style=0;style<3;style++) {
        Dsp d;d.target[Style]=style*.5;d.prepare(sr);
        for(int n=0;n<50000;n++) {
            if(n%997==0){d.set(Character,(n/997)%2);d.set(Bypass,(n/997)%2);d.set(Output,((n/997)%3)*.5);d.set(Style,((n/997)%3)*.5);}
            double x[2]={std::sin(n*.157)*.9,n==200?std::numeric_limits<double>::quiet_NaN():.4*std::sin(n*.113)};d.frame(x,2);
            require(std::isfinite(x[0])&&std::isfinite(x[1]),"finite output under automation");require(std::abs(x[0])<30&&std::abs(x[1])<30,"bounded output");
        }
    }
    for(int type=0;type<3;type++) {
        double low=rms(70,type),mid=rms(1000,type),high=rms(10000,type);
        require(mid>low*8,"telephone low frequency rejection");require(mid>high*8,"telephone high frequency rejection");
        std::cout<<"Style "<<type<<": 70 Hz="<<low<<", 1 kHz="<<mid<<", 10 kHz="<<high<<"\n";
    }
    for(unsigned mode:{Bypass,Mix}){
        Dsp d;d.target[mode]=mode==Bypass?1:0;d.prepare(48000);
        for(int n=0;n<2000;n++){double ref=.75*std::sin(n*.37),x[2]={ref,-ref};d.frame(x,2);require(std::abs(x[0]-ref)<1e-14&&std::abs(x[1]+ref)<1e-14,"transparent dry/bypass");}
    }
    Dsp silence;silence.prepare(48000);for(int n=0;n<10000;n++){double x[2]{};silence.frame(x,2);require(x[0]==0&&x[1]==0,"no noise without signal");}
    Dsp stereo;stereo.prepare(48000);for(int n=0;n<10000;n++){double x[2]={std::sin(n*.2),0};stereo.frame(x,2);require(x[1]==0,"stereo channel independence");}
    double fingerprints[3]{};
    for(int t=0;t<3;t++){Dsp d;d.target[Style]=t*.5;d.prepare(48000);for(int n=0;n<20000;n++){double x[2]={.3*std::sin(n*.2),0};d.frame(x,1);fingerprints[t]+=x[0]*x[0];}}
    require(std::abs(fingerprints[0]-fingerprints[1])>1&&std::abs(fingerprints[1]-fingerprints[2])>1,"distinct styles");
    auto start=std::chrono::steady_clock::now();Dsp performance;performance.prepare(48000);double x[2];
    for(int n=0;n<480000;n++){x[0]=x[1]=.2;performance.frame(x,2);}
    std::cout<<"10 seconds stereo processed in "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" seconds\n";
    std::cout<<"PASS: six sample rates, automation, telephone response, dry/bypass null, silence, stereo separation, style distinction.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
