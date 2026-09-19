#include "Plugin.h"
#include "Editor.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "base/source/fstreamer.h"
#include <iostream>
#include <stdexcept>
using namespace Steinberg;using namespace Steinberg::Vst;using namespace gastele;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static void rewind(MemoryStream& s){s.seek(0,IBStream::kIBSeekSet,nullptr);}
int main(){try {
    auto* p=new Processor;auto* c=new Controller;
    require(p->initialize(nullptr)==kResultOk&&c->initialize(nullptr)==kResultOk,"initialization");
    ParameterChanges changes(Count);int32 index=0;
    const std::array<double,Count> chosen{0,.87,1,.42,.39,1.};
    for(unsigned id=0;id<Count;id++)changes.addParameterData(id,index)->addPoint(0,chosen[id],index);
    ProcessData flush;flush.inputParameterChanges=&changes;flush.numSamples=0;
    require(p->process(flush)==kResultOk,"zero-sample parameter flush");
    MemoryStream saved;require(p->getState(&saved)==kResultOk,"save state");rewind(saved);
    require(c->setComponentState(&saved)==kResultOk,"restore controller state");
    for(unsigned id=0;id<Count;id++)require(std::abs(c->getParamNormalized(id)-chosen[id])<1e-12,"all parameters restored");
    auto* restored=new Processor;restored->initialize(nullptr);rewind(saved);require(restored->setState(&saved)==kResultOk,"restore processor state");
    MemoryStream duplicate;restored->getState(&duplicate);require(duplicate.getSize()==saved.getSize()&&memcmp(duplicate.getData(),saved.getData(),saved.getSize())==0,"exact state roundtrip");
    MemoryStream truncated(saved.getData(),7);require(restored->setState(&truncated)!=kResultOk,"reject truncated state");
    MemoryStream afterInvalid;restored->getState(&afterInvalid);require(memcmp(afterInvalid.getData(),saved.getData(),saved.getSize())==0,"failed restore is transactional");
    MemoryStream legacy;IBStreamer legacyWriter(&legacy,kLittleEndian);legacyWriter.writeInt32u(1);
    for(unsigned id=0;id<5;id++)legacyWriter.writeDouble(chosen[id]);rewind(legacy);
    require(c->setComponentState(&legacy)==kResultOk,"load legacy 1.1 controller state");
    require(c->getParamNormalized(Realphone)==0,"legacy sessions default Realphone OFF");
    for(unsigned id=0;id<5;id++)require(c->getParamNormalized(id)==chosen[id],"preserve legacy parameters");
    rewind(legacy);require(restored->setState(&legacy)==kResultOk,"load legacy 1.1 processor state");
    // Compare real process() output with the same DSP driven at the specified sample offsets.
    auto* timed=new Processor;timed->initialize(nullptr);ProcessSetup setup{kRealtime,kSample32,256,48000};
    timed->setupProcessing(setup);timed->setActive(true);timed->setProcessing(true);
    ParameterChanges automation(Count);automation.addParameterData(Character,index)->addPoint(64,.1,index);automation.getParameterData(0)->addPoint(128,.95,index);
    float in[2][256],out[2][256];float* ins[]={in[0],in[1]};float* outs[]={out[0],out[1]};
    for(int n=0;n<256;n++){in[0][n]=float(.3*std::sin(n*.14));in[1][n]=float(.2*std::cos(n*.11));}
    AudioBusBuffers input,output;input.numChannels=output.numChannels=2;input.channelBuffers32=ins;output.channelBuffers32=outs;input.silenceFlags=0;
    ParameterChanges meter(1);ProcessData data;data.numSamples=256;data.numInputs=data.numOutputs=1;data.inputs=&input;data.outputs=&output;data.inputParameterChanges=&automation;data.outputParameterChanges=&meter;
    timed->process(data);Dsp reference;reference.prepare(48000);
    for(int n=0;n<256;n++){if(n==64)reference.set(Character,.1);if(n==128)reference.set(Character,.95);double x[]={in[0][n],in[1][n]};reference.frame(x,2);for(int ch=0;ch<2;ch++)require(std::abs(out[ch][n]-float(x[ch]))<1e-7,"sample-position automation");}
    require(meter.getParameterCount()==1,"meter output parameter");
    // Exercise the compiled editor in a hidden test window; no user application is controlled.
    HWND parent=CreateWindowExW(0,L"STATIC",L"GASTELE test",WS_OVERLAPPEDWINDOW,0,0,1000,700,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    auto* view=c->createView(ViewType::kEditor);require(view!=nullptr,"editor factory");
    require(view->attached(parent,kPlatformTypeHWND)==kResultOk,"editor attach");
    HWND child=GetWindow(parent,GW_CHILD);require(child!=nullptr,"native editor window");
    SendMessageW(child,WM_LBUTTONDOWN,0,MAKELPARAM(255,210));SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(255,210));require(c->getParamNormalized(Bypass)==1,"power button binding");
    SendMessageW(child,WM_LBUTTONDOWN,0,MAKELPARAM(260,507));SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(260,507));require(c->getParamNormalized(Style)==.5,"style button binding");
    SendMessageW(child,WM_LBUTTONDOWN,0,MAKELPARAM(255,355));SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(255,331));SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(255,331));require(c->getParamNormalized(Character)>.96,"rotary drag binding");
    SendMessageW(child,WM_LBUTTONDBLCLK,0,MAKELPARAM(255,355));require(std::abs(c->getParamNormalized(Character)-.65)<1e-12,"double-click reset");
    SendMessageW(child,WM_LBUTTONDOWN,0,MAKELPARAM(380,555));SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(380,555));require(c->getParamNormalized(Realphone)==1,"Realphone button ON");
    SendMessageW(child,WM_KEYDOWN,VK_SPACE,0);require(c->getParamNormalized(Realphone)==0,"Realphone keyboard toggle");
    ViewRect resize{0,0,1200,900};view->checkSizeConstraint(&resize);require(resize.getHeight()==840,"aspect ratio constraint");require(view->onSize(&resize)==kResultOk,"resize");
    for(int n=0;n<4;n++){view->removed();require(view->attached(parent,kPlatformTypeHWND)==kResultOk,"editor reopen");}
    view->removed();view->release();DestroyWindow(parent);
    timed->setProcessing(false);timed->setActive(false);timed->terminate();timed->release();restored->terminate();restored->release();p->terminate();p->release();c->terminate();c->release();
    std::cout<<"PASS: v2 state roundtrip with Realphone, legacy v1 session migration, corrupt-state safety, zero-sample flush, sample-position automation, meter, native UI lifecycle, power/style/Realphone/knob/reset bindings and resize.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
