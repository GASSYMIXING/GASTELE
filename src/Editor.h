#pragma once
#include "public.sdk/source/common/pluginview.h"
#include "Plugin.h"
#include "Panel.h"
#include <windowsx.h>
namespace gastele {
class Editor final : public Steinberg::CPluginView {
public:
    explicit Editor(Controller*);
    ~Editor() override;
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API attached(void*,Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API removed() override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect*) override;
    Steinberg::tresult PLUGIN_API canResize() override{return Steinberg::kResultTrue;}
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect*) override;
    static LRESULT CALLBACK windowProc(HWND,UINT,WPARAM,LPARAM);
private:
    Controller* controller;HWND window=nullptr;Panel panel;
    int dragging=-1,startY=0,focused=-1;double startValue=0;
    std::array<double,Count> lastValues=defaults;double displayMeter=0;
    int hit(int x,int y);void change(unsigned,double,bool gesture=true);void endDrag();void paint(HDC);
};
}
