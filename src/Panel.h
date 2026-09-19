#pragma once
#include "Dsp.h"
#include <windows.h>
#include <objidl.h>
#include <ole2.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <memory>
#include <string>

namespace gastele {
class Panel {
public:
    static constexpr float width=1499,height=1049;
    Panel() {Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}
    ~Panel(){image.reset();if(stream)stream->Release();Gdiplus::GdiplusShutdown(token);}
    bool load(HMODULE module) {
        auto resource=FindResourceW(module,MAKEINTRESOURCEW(101),RT_RCDATA);
        if(!resource)return false;auto data=LoadResource(module,resource);
        stream=SHCreateMemStream(static_cast<const BYTE*>(LockResource(data)),SizeofResource(module,resource));
        if(!stream)return false;image.reset(Gdiplus::Bitmap::FromStream(stream));return flatten();
    }
    bool load(const wchar_t* path){image.reset(Gdiplus::Bitmap::FromFile(path));return flatten();}
    static void rounded(Gdiplus::Graphics& g,Gdiplus::Color color,Gdiplus::RectF r,float radius) {
        Gdiplus::GraphicsPath p;const float d=radius*2;
        p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
        p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);p.AddArc(r.X,r.GetBottom()-d,d,d,90,90);p.CloseFigure();
        Gdiplus::SolidBrush b(color);g.FillPath(&b,&p);
    }
    static void text(Gdiplus::Graphics& g,const wchar_t* value,Gdiplus::RectF r,float size,Gdiplus::Color color,int style=Gdiplus::FontStyleBold) {
        Gdiplus::FontFamily family(L"Arial");Gdiplus::Font font(&family,size,style,Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(color);Gdiplus::StringFormat format;format.SetAlignment(Gdiplus::StringAlignmentCenter);format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(value,-1,&font,r,&format,&brush);
    }
    void draw(Gdiplus::Graphics& g,int w,int h,const std::array<double,Count>& values,double meter,int focus=-1) {
        using namespace Gdiplus;
        auto saved=g.Save();g.ScaleTransform(w/width,h/height);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        if(!image){g.Clear(Color(255,30,30,30));text(g,L"GASTELE",RectF(0,0,width,height),80,Color::White);g.Restore(saved);return;}
        g.DrawImage(image.get(),RectF(0,0,width,height));
        const Color pink(255,255,42,136),ink(255,23,24,25);
        knob(g,385.f,536.f,110.f,values[Character],true);
        knob(g,150.f,952.f,55.f,values[Mix],false);
        knob(g,554.f,952.f,55.f,values[Output],false);
        wchar_t label[40];
        if(std::abs(values[Character]-.65)>.0001){swprintf_s(label,L"%.0f%%",values[Character]*100);readout(g,label,{324,638,126,35},pink);}
        if(std::abs(values[Mix]-1)>.0001){swprintf_s(label,L"%.0f%%",values[Mix]*100);readout(g,label,{246,950,111,36},pink);}
        if(std::abs(values[Output]-.5)>.0001){swprintf_s(label,L"%.1f dB",-24+values[Output]*48);readout(g,label,{652,950,116,36},pink);}
        const int selected=std::clamp(int(std::round(values[Style]*2)),0,2);
        if(selected!=0) {
            // Clip the replacement segments inside the original capsule's border.
            GraphicsPath clip;clip.AddArc(111,729,62,62,90,180);clip.AddArc(608,729,62,62,270,180);clip.CloseFigure();
            auto s=g.Save();g.SetClip(&clip);
            const wchar_t* labels[]={L"LANDLINE",L"MOBILE",L"RADIO"};
            for(int i=0;i<3;i++) {
                SolidBrush fill(i==selected?pink:Color(255,238,233,224));g.FillRectangle(&fill,110.f+i*187.f,728.f,187.f,70.f);
                const Color fg=i==selected?Color::White:ink;
                SolidBrush dot(fg);g.FillEllipse(&dot,193.f+i*188.f,740.f,15.f,15.f);
                text(g,labels[i],RectF(110.f+i*188.f,762.f,184.f,30.f),21.f,fg);
            }
            g.Restore(s);Pen separator(ink,3);g.DrawLine(&separator,297.f,726.f,297.f,797.f);g.DrawLine(&separator,486.f,726.f,486.f,797.f);
        }
        const bool realphoneOn=values[Realphone]>=.5;
        rounded(g,ink,RectF(473,811,204,43),21);
        rounded(g,realphoneOn?pink:Color(255,40,38,41),RectF(475,813,200,39),19);
        SolidBrush realphoneLamp(realphoneOn?Color::White:Color(255,113,89,102));
        g.FillEllipse(&realphoneLamp,490.f,825.f,12.f,12.f);
        text(g,L"Realphone",RectF(507,814,151,36),22,realphoneOn?Color::White:Color(255,218,207,212));
        if(values[Bypass]>=.5) {
            rounded(g,Color(225,28,28,32),RectF(177,268,425,94),45);
            rounded(g,Color(255,55,30,43),RectF(301,280,209,69),4);
            text(g,L"LINE OFF",RectF(274,278,236,75),39,Color(255,171,165,170),FontStyleBold|FontStyleItalic);
            SolidBrush dot(Color(255,89,80,86));g.FillEllipse(&dot,533.f,302.f,27.f,27.f);
            rounded(g,Color(255,30,29,31),RectF(1265,896,180,45),8);
            text(g,L"BYPASSED",RectF(1267,903,137,32),21,Color(255,155,151,155));
            SolidBrush lamp(Color(255,79,52,66));g.FillEllipse(&lamp,1412.f,909.f,17.f,17.f);
        }
        // Clear the baked-in demo meter; illuminate bars from the actual output signal.
        rounded(g,Color(255,30,29,31),RectF(868,907,341,99),12);
        const double db=meter>1e-8?20*std::log10(meter):-80;
        const int lit=std::clamp(int(std::ceil((db+54)/54*12)),0,12);
        for(int i=0;i<12;i++) {
            const float x=891.f+i*24.7f,y=931.f-i*.7f;
            if(i<lit) {
                rounded(g,Color(30,255,40,136),RectF(x-6,y-6,23,76),11);
                rounded(g,Color(55,255,40,136),RectF(x-3,y-3,17,70),8);
            }
            rounded(g,i<lit?pink:Color(255,70,42,54),RectF(x,y,11,64),5);
        }
        if(focus>=0&&focus<Count) {
            const RectF boxes[]={RectF(170,260,438,110),RectF(256,416,255,267),RectF(105,721,573,82),RectF(78,883,289,133),RectF(477,883,296,133),RectF(473,811,204,43)};
            Pen p(Color(200,255,130,182),2);p.SetDashStyle(DashStyleDot);g.DrawRectangle(&p,boxes[focus]);
        }
        g.Restore(saved);
    }
private:
    ULONG_PTR token=0;IStream* stream=nullptr;std::unique_ptr<Gdiplus::Bitmap> image;
    bool flatten() {
        if(!image||image->GetLastStatus()!=Gdiplus::Ok)return false;
        // The approved PNG has partial alpha. Composite it on white once so
        // moving controls never accumulate translucent copies of old markers.
        auto opaque=std::make_unique<Gdiplus::Bitmap>(image->GetWidth(),image->GetHeight(),PixelFormat32bppARGB);
        {Gdiplus::Graphics g(opaque.get());g.Clear(Gdiplus::Color::White);g.DrawImage(image.get(),0,0);}
        image=std::move(opaque);return true;
    }
    void readout(Gdiplus::Graphics& g,const wchar_t* label,Gdiplus::RectF r,Gdiplus::Color color) {
        rounded(g,Gdiplus::Color(255,20,22,23),r,6);text(g,label,r,26,color);
    }
    void knob(Gdiplus::Graphics& g,float x,float y,float radius,double value,bool main) {
        using namespace Gdiplus;
        // The illustration is not a concentric rotary sprite. Keep its bezel,
        // shadows and readout stationary; draw a circular face and radial pointer.
        // Every angle uses this same geometry, including the default values.
        const auto saved=g.Save();
        GraphicsPath face;face.AddEllipse(x-radius,y-radius,2*radius,2*radius);
        g.SetClip(&face,CombineModeIntersect);
        LinearGradientBrush surface(PointF(x-radius,y-radius),PointF(x+radius,y+radius),
                                    Color(255,49,50,52),Color(255,27,28,30));
        g.FillEllipse(&surface,x-radius,y-radius,2*radius,2*radius);
        // Static concentric shading maintains the original dark metal finish.
        Pen edge(Color(255,79,81,82),1.4f);
        g.DrawEllipse(&edge,x-radius+.8f,y-radius+.8f,2*radius-1.6f,2*radius-1.6f);
        Pen inner(Color(255,24,25,27),1.1f);
        g.DrawEllipse(&inner,x-radius+3.f,y-radius+3.f,2*radius-6.f,2*radius-6.f);
        const float angle=static_cast<float>((-135.+270.*normalized(value))*pi/180.);
        const float dx=std::sin(angle),dy=-std::cos(angle);
        const float innerRadius=radius*(main?.43f:.44f),outerRadius=radius*(main?.85f:.87f);
        const PointF a(x+dx*innerRadius,y+dy*innerRadius),b(x+dx*outerRadius,y+dy*outerRadius);
        const float stroke=main?12.f:6.3f;
        Pen shadow(Color(150,0,0,0),stroke+4.f);shadow.SetStartCap(LineCapRound);shadow.SetEndCap(LineCapRound);
        g.DrawLine(&shadow,PointF(a.X,a.Y+1.8f),PointF(b.X,b.Y+1.8f));
        Pen pointer(Color(255,255,57,139),stroke);pointer.SetStartCap(LineCapRound);pointer.SetEndCap(LineCapRound);
        g.DrawLine(&pointer,a,b);
        Pen glint(Color(130,255,161,194),1.2f);glint.SetStartCap(LineCapRound);glint.SetEndCap(LineCapRound);
        g.DrawLine(&glint,PointF(a.X-1,a.Y-1),PointF(b.X-1,b.Y-1));
        g.Restore(saved);
        // This fixed foreground layer covers the physical overlap at the bottom.
        if(main)g.DrawImage(image.get(),RectF(316,631,141,47),316,631,141,47,UnitPixel);
    }
};
}
