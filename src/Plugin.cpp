#include "Plugin.h"
#include "Editor.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cstring>
#include <type_traits>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace gastele {
static bool readState(IBStream* stream,std::array<double,Count>& values) {
    if(!stream)return false;
    IBStreamer reader(stream,kLittleEndian); uint32 version=0;
    if(!reader.readInt32u(version)||(version!=1&&version!=2))return false;
    values=defaults;
    const unsigned storedCount=version==1?5:Count;
    for(unsigned i=0;i<storedCount;i++) {auto& value=values[i];if(!reader.readDouble(value)||!std::isfinite(value)||value<0||value>1)return false;}
    return true;
}
Processor::Processor(){setControllerClass(controllerID);}
tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    auto result=AudioEffect::initialize(context);if(result!=kResultOk)return result;
    addAudioInput(STR16("Stereo In"),SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"),SpeakerArr::kStereo);
    return kResultOk;
}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* ins,int32 ni,SpeakerArrangement* outs,int32 no) {
    if(ni!=1||no!=1||!ins||!outs||ins[0]!=outs[0]||(ins[0]!=SpeakerArr::kMono&&ins[0]!=SpeakerArr::kStereo))return kResultFalse;
    return AudioEffect::setBusArrangements(ins,ni,outs,no);
}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 s){return s==kSample32||s==kSample64?kResultTrue:kResultFalse;}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
    if(setup.sampleRate<8000||!std::isfinite(setup.sampleRate))return kInvalidArgument;
    const auto result=AudioEffect::setupProcessing(setup);
    if(result==kResultOk)dsp.prepare(setup.sampleRate);return result;
}
tresult PLUGIN_API Processor::setActive(TBool active){if(active)dsp.prepare(processSetup.sampleRate);return AudioEffect::setActive(active);}
template<typename Sample> void Processor::audio(ProcessData& data) {
    struct Event {IParamValueQueue* queue=nullptr; int32 index=0,offset=0; double value=0;bool ready=false;};
    std::array<Event,Count> events{};
    if(data.inputParameterChanges)for(int32 q=0;q<data.inputParameterChanges->getParameterCount();q++) {
        auto* queue=data.inputParameterChanges->getParameterData(q);
        if(queue&&queue->getParameterId()<Count) {
            auto& e=events[queue->getParameterId()];e.queue=queue;
            e.ready=queue->getPoint(0,e.offset,e.value)==kResultTrue;
        }
    }
    const bool buffers=data.numInputs>0&&data.numOutputs>0&&data.inputs&&data.outputs;
    const int channels=buffers?std::min({2,data.inputs[0].numChannels,data.outputs[0].numChannels}):0;
    Sample** input=nullptr;Sample** output=nullptr;
    if(buffers) {
        if constexpr(std::is_same_v<Sample,float>){input=data.inputs[0].channelBuffers32;output=data.outputs[0].channelBuffers32;}
        else {input=data.inputs[0].channelBuffers64;output=data.outputs[0].channelBuffers64;}
        data.outputs[0].silenceFlags=0;
    }
    for(int32 n=0;n<std::max(1,data.numSamples);n++) {
        for(unsigned id=0;id<Count;id++) {
            auto& e=events[id];
            while(e.ready&&(e.offset<=n||data.numSamples==0)) {
                dsp.set(id,e.value);e.ready=e.queue->getPoint(++e.index,e.offset,e.value)==kResultTrue;
            }
        }
        if(n>=data.numSamples||!input||!output)continue;
        double values[2]{};
        for(int c=0;c<channels;c++)if(input[c]&&!(data.inputs[0].silenceFlags&(uint64(1)<<c)))values[c]=input[c][n];
        dsp.frame(values,channels);
        for(int c=0;c<channels;c++)if(output[c])output[c][n]=static_cast<Sample>(values[c]);
        for(int c=channels;c<data.outputs[0].numChannels;c++)if(output[c])output[c][n]=0;
    }
    if(data.outputParameterChanges&&data.numSamples>0) {
        int32 index=0;
        if(auto* q=data.outputParameterChanges->addParameterData(Meter,index))q->addPoint(data.numSamples-1,dsp.level(),index);
    }
}
tresult PLUGIN_API Processor::process(ProcessData& data) {
    if(data.symbolicSampleSize==kSample32)audio<float>(data);
    else if(data.symbolicSampleSize==kSample64)audio<double>(data);
    else return kResultFalse;
    return kResultOk;
}
tresult PLUGIN_API Processor::getState(IBStream* stream) {
    if(!stream)return kInvalidArgument;IBStreamer writer(stream,kLittleEndian);
    if(!writer.writeInt32u(2))return kResultFalse;
    for(auto value:dsp.target)if(!writer.writeDouble(value))return kResultFalse;return kResultOk;
}
tresult PLUGIN_API Processor::setState(IBStream* stream) {
    std::array<double,Count> values{};if(!readState(stream,values))return kResultFalse;
    dsp.target=values;return kResultOk;
}
tresult PLUGIN_API Controller::initialize(FUnknown* context) {
    auto result=EditController::initialize(context);if(result!=kResultOk)return result;
    parameters.addParameter(STR16("Bypass"),nullptr,1,0,ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass,Bypass);
    auto* character=new RangeParameter(STR16("Character"),Character,STR16("%"),0,100,65);character->setPrecision(0);parameters.addParameter(character);
    auto* style=new StringListParameter(STR16("Style"),Style);
    style->appendString(STR16("Landline"));style->appendString(STR16("Mobile"));style->appendString(STR16("Radio"));parameters.addParameter(style);
    auto* mix=new RangeParameter(STR16("Mix"),Mix,STR16("%"),0,100,100);mix->setPrecision(0);parameters.addParameter(mix);
    auto* output=new RangeParameter(STR16("Output"),Output,STR16("dB"),-24,24,0);output->setPrecision(1);parameters.addParameter(output);
    parameters.addParameter(STR16("Realphone"),nullptr,1,0,ParameterInfo::kCanAutomate,Realphone);
    parameters.addParameter(STR16("Output Level"),nullptr,0,0,ParameterInfo::kIsReadOnly,Meter);
    return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream) {
    std::array<double,Count> values{};if(!readState(stream,values))return kResultFalse;
    for(unsigned i=0;i<Count;i++)setParamNormalized(i,values[i]);return kResultOk;
}
IPlugView* PLUGIN_API Controller::createView(FIDString name) {
    return name&&std::strcmp(name,ViewType::kEditor)==0?new Editor(this):nullptr;
}
}
bool InitModule(){return true;}
bool DeinitModule(){
    HMODULE module=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&DeinitModule),&module);
    UnregisterClassW(L"Gassymixing.GASTELE.1",module);return true;
}
BEGIN_FACTORY_DEF("Gassymixing", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(gastele::processorID),PClassInfo::kManyInstances,kVstAudioEffectClass,"GASTELE",Vst::kDistributable,"Fx|Distortion", "1.2",kVstVersionString,gastele::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(gastele::controllerID),PClassInfo::kManyInstances,kVstComponentControllerClass,"GASTELE Controller",0,"", "1.2",kVstVersionString,gastele::Controller::create)
END_FACTORY
