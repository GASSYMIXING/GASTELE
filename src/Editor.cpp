#include "Editor.h"
#include <cstring>
namespace gastele {
using namespace Steinberg;
static HMODULE thisModule() {
    HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&thisModule),&module);return module;
}
Editor::Editor(Controller* c):controller(c) {controller->addRef();rect={0,0,1000,700};panel.load(thisModule());}
Editor::~Editor(){removed();controller->release();}
tresult PLUGIN_API Editor::isPlatformTypeSupported(FIDString type){return type&&std::strcmp(type,kPlatformTypeHWND)==0?kResultTrue:kResultFalse;}
tresult PLUGIN_API Editor::attached(void* parent,FIDString type) {
    if(isPlatformTypeSupported(type)!=kResultTrue||!parent||window)return kResultFalse;
    WNDCLASSEXW wc{sizeof(wc)};wc.style=CS_DBLCLKS;wc.lpfnWndProc=windowProc;wc.hInstance=thisModule();wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=L"Gassymixing.GASTELE.1";
    RegisterClassExW(&wc);
    window=CreateWindowExW(0,wc.lpszClassName,L"GASTELE by Gassymixing",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,rect.getWidth(),rect.getHeight(),static_cast<HWND>(parent),nullptr,wc.hInstance,this);
    if(!window)return kResultFalse;SetTimer(window,1,33,nullptr);return CPluginView::attached(parent,type);
}
void Editor::endDrag(){if(dragging>=0){controller->endEdit(dragging);dragging=-1;}if(window&&GetCapture()==window)ReleaseCapture();}
tresult PLUGIN_API Editor::removed(){endDrag();if(window){KillTimer(window,1);DestroyWindow(window);window=nullptr;}return CPluginView::removed();}
tresult PLUGIN_API Editor::onSize(ViewRect* size) {
    if(!size)return kInvalidArgument;rect=*size;if(window)SetWindowPos(window,nullptr,0,0,rect.getWidth(),rect.getHeight(),SWP_NOMOVE|SWP_NOZORDER);return kResultTrue;
}
tresult PLUGIN_API Editor::checkSizeConstraint(ViewRect* size) {
    if(!size)return kInvalidArgument;int w=std::clamp(size->getWidth(),750,1499);size->right=size->left+w;size->bottom=size->top+int(w*Panel::height/Panel::width+.5f);return kResultTrue;
}
int Editor::hit(int x,int y) {
    RECT r;GetClientRect(window,&r);const double px=x*Panel::width/std::max(1L,r.right),py=y*Panel::height/std::max(1L,r.bottom);
    if(px>=171&&px<=610&&py>=260&&py<=375)return Bypass;
    if(px>=253&&px<=518&&py>=412&&py<=680)return Character;
    if(px>=108&&px<=676&&py>=721&&py<=802)return Style;
    if(px>=473&&px<=677&&py>=811&&py<=854)return Realphone;
    if(px>=75&&px<=367&&py>=881&&py<=1019)return Mix;
    if(px>=475&&px<=776&&py>=881&&py<=1019)return Output;
    return -1;
}
void Editor::change(unsigned id,double value,bool gesture) {
    if(id>=Count)return;value=normalized(value);if(gesture)controller->beginEdit(id);
    controller->setParamNormalized(id,value);controller->performEdit(id,value);
    if(gesture)controller->endEdit(id);if(window)InvalidateRect(window,nullptr,FALSE);
}
void Editor::paint(HDC dc) {
    RECT r;GetClientRect(window,&r);if(r.right<1||r.bottom<1)return;
    Gdiplus::Bitmap buffer(r.right,r.bottom,PixelFormat32bppPARGB);Gdiplus::Graphics g(&buffer);
    std::array<double,Count> values;for(unsigned i=0;i<Count;i++)values[i]=controller->getParamNormalized(i);
    panel.draw(g,r.right,r.bottom,values,displayMeter,GetFocus()==window?focused:-1);
    Gdiplus::Graphics output(dc);output.DrawImage(&buffer,0,0);
}
LRESULT CALLBACK Editor::windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* self=reinterpret_cast<Editor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window=hwnd;}
    if(!self)return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg) {
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT p;HDC dc=BeginPaint(hwnd,&p);self->paint(dc);EndPaint(hwnd,&p);return 0;}
    case WM_TIMER:{
        bool dirty=false;for(unsigned i=0;i<Count;i++){double v=self->controller->getParamNormalized(i);if(v!=self->lastValues[i]){self->lastValues[i]=v;dirty=true;}}
        const double v=self->controller->getParamNormalized(Meter);const double next=std::max(v,self->displayMeter*.85);
        if(std::abs(next-self->displayMeter)>.0001){self->displayMeter=next;dirty=true;}if(dirty)InvalidateRect(hwnd,nullptr,FALSE);return 0;}
    case WM_LBUTTONDOWN:{
        SetFocus(hwnd);int id=self->hit(GET_X_LPARAM(lp),GET_Y_LPARAM(lp));self->focused=id;
        if(id==Bypass||id==Realphone)self->change(id,self->controller->getParamNormalized(id)<.5?1:0);
        else if(id==Style){RECT r;GetClientRect(hwnd,&r);double x=GET_X_LPARAM(lp)*Panel::width/r.right;self->change(id,std::clamp(int((x-108)/189),0,2)*.5);}
        else if(id>=0){self->dragging=id;self->startY=GET_Y_LPARAM(lp);self->startValue=self->controller->getParamNormalized(id);self->controller->beginEdit(id);SetCapture(hwnd);}
        InvalidateRect(hwnd,nullptr,FALSE);return 0;}
    case WM_MOUSEMOVE:if(self->dragging>=0){const double range=(wp&MK_SHIFT)?1600.:240.;self->change(self->dragging,self->startValue+(self->startY-GET_Y_LPARAM(lp))/range,false);}return 0;
    case WM_LBUTTONUP:case WM_CAPTURECHANGED:case WM_CANCELMODE:self->endDrag();return 0;
    case WM_LBUTTONDBLCLK:{int id=self->hit(GET_X_LPARAM(lp),GET_Y_LPARAM(lp));if(id>=0)self->change(id,defaults[id]);return 0;}
    case WM_MOUSEWHEEL:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(hwnd,&p);int id=self->hit(p.x,p.y);if(id>=0){double step=id==Style?.5:(id==Bypass||id==Realphone)?1.:((GET_KEYSTATE_WPARAM(wp)&MK_SHIFT)?.001:.01);self->change(id,self->controller->getParamNormalized(id)+GET_WHEEL_DELTA_WPARAM(wp)/120.*step);}return 0;}
    case WM_GETDLGCODE:return DLGC_WANTARROWS|DLGC_WANTTAB|DLGC_WANTCHARS;
    case WM_KEYDOWN:{
        if(wp==VK_TAB){self->focused=(self->focused+((GetKeyState(VK_SHIFT)&0x8000)?Count-1:1)+Count)%Count;InvalidateRect(hwnd,nullptr,FALSE);return 0;}
        const int id=self->focused;if(id<0)return 0;
        if(wp==VK_HOME)self->change(id,defaults[id]);
        else if(wp==VK_SPACE&&(id==Bypass||id==Realphone))self->change(id,self->controller->getParamNormalized(id)<.5?1:0);
        else if(wp==VK_LEFT||wp==VK_DOWN||wp==VK_RIGHT||wp==VK_UP){double step=id==Style?.5:(id==Bypass||id==Realphone)?1.:.01;self->change(id,self->controller->getParamNormalized(id)+((wp==VK_LEFT||wp==VK_DOWN)?-step:step));}
        return 0;}
    case WM_KILLFOCUS:self->endDrag();InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_SETCURSOR:{POINT p;GetCursorPos(&p);ScreenToClient(hwnd,&p);SetCursor(LoadCursor(nullptr,self->hit(p.x,p.y)>=0?IDC_HAND:IDC_ARROW));return TRUE;}
    default:break;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}
