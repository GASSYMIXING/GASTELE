#include "Panel.h"
#include <iostream>
#include <vector>
int wmain(int argc,wchar_t** argv) {
    if(argc<3)return 1; // Render the same production Panel used by the VST3 editor.
    gastele::Panel panel;if(!panel.load(argv[1]))return 2;
    Gdiplus::Bitmap image(1499,1049,PixelFormat32bppARGB);Gdiplus::Graphics g(&image);
    auto values=gastele::defaults;
    const bool realphone=argc>3&&wcscmp(argv[3],L"--realphone")==0;
    const bool sweep=argc>4&&wcscmp(argv[3],L"--knobs")==0;
    if(sweep){double value=wcstod(argv[4],nullptr);values[gastele::Character]=values[gastele::Mix]=values[gastele::Output]=value;}
    else if(realphone)values[gastele::Realphone]=1;
    else {
        if(argc>3){values[gastele::Character]=.9;values[gastele::Style]=.5;values[gastele::Mix]=.5;values[gastele::Output]=.4;}
        if(argc>4)values[gastele::Bypass]=1;
    }
    panel.draw(g,1499,1049,values,argc>4?0:.018);
    UINT count=0,bytes=0;Gdiplus::GetImageEncodersSize(&count,&bytes);
    std::vector<BYTE> buffer(bytes);auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());Gdiplus::GetImageEncoders(count,bytes,encoders);
    for(UINT i=0;i<count;i++)if(wcscmp(encoders[i].MimeType,L"image/png")==0)return image.Save(argv[2],&encoders[i].Clsid,nullptr)==Gdiplus::Ok?0:3;
    return 4;
}
