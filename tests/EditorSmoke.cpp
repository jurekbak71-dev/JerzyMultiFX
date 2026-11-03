#include "plugin-vst3/Plugin.h"
#include "core/Presentation.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <windows.h>
#include <windowsx.h>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace multifx;
static void check(bool c, const char *m) {
    if (!c)
        throw std::runtime_error(m);
}
static void pump() {
    MSG m{};
    while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
}
static void click(HWND h, int x, int y) {
    SendMessage(h, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    SendMessage(h, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    pump();
}
static void screenshot(HWND h, const char *path) {
    RECT size{};
    GetClientRect(h, &size);
    const int w = size.right, height = size.bottom;
    HDC dc = GetDC(h), memory = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, w, height);
    auto previous = SelectObject(memory, bmp);
    RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    SendMessage(h, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT);
    SelectObject(memory, previous);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<unsigned char> pixels(w * height * 4);
    check(GetDIBits(dc, bmp, 0, height, pixels.data(), &info, DIB_RGB_COLORS) != 0, "read editor pixels");
    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
    FILE *file = std::fopen(path, "wb");
    check(file != nullptr, "write screenshot");
    std::fwrite(&header, sizeof(header), 1, file);
    std::fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, file);
    std::fwrite(pixels.data(), 1, pixels.size(), file);
    std::fclose(file);
    DeleteObject(bmp);
    DeleteDC(memory);
    ReleaseDC(h, dc);
}
static unsigned long long meterPixels(HWND h, int x, int top) {
    RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
    HDC screen=GetDC(h),dc=CreateCompatibleDC(screen);
    auto bitmap=CreateCompatibleBitmap(screen,1280,720);auto old=SelectObject(dc,bitmap);
    SendMessage(h,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
    unsigned long long hash=1469598103934665603ULL;
    for(int y=top;y<top+14;++y)for(int xx=x;xx<x+60;++xx) hash=(hash^GetPixel(dc,xx,y))*1099511628211ULL;
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(h,screen);return hash;
}
int main(int argc, char **argv) try {
    check(argc >= 2, "usage: editor_smoke plugin.vst3 [screenshot.bmp]");
    for (int cycle = 0; cycle < 3; ++cycle) {
        HMODULE module = LoadLibraryA(argv[1]);
        check(module != nullptr, "load Windows VST3 DLL");
        auto init = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InitDll"));
        auto exit = reinterpret_cast<bool (*)()>(GetProcAddress(module, "ExitDll"));
        auto factoryFunction =
            reinterpret_cast<IPluginFactory *(*)()>(GetProcAddress(module, "GetPluginFactory"));
        check(init && exit && factoryFunction && init(), "module entry points");
        auto *factory = factoryFunction();
        IEditController *c = nullptr;
        check(factory->createInstance(controllerId, IEditController::iid, reinterpret_cast<void **>(&c)) ==
                      kResultOk &&
                  c,
              "instantiate real DLL controller");
        check(c->initialize(nullptr) == kResultOk, "initialize GUI controller");
        auto *view = c->createView(ViewType::kEditor);
        check(view, "custom pedal editor exists");
        HWND parent = CreateWindowExA(0, "STATIC", "Jerzy MultiFX GUI test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      0, 0, 1320, 920, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        check(parent, "create test host window");
        check(view->attached(parent, kPlatformTypeHWND) == kResultOk, "attach custom GUI");
        ViewRect testSize(0, 0, 1280, 720);
        check(view->onSize(&testSize) == kResultOk, "set deterministic GUI test size");
        pump();
        HWND child = FindWindowExA(parent, nullptr, "JerzyMultiFXEditor010", nullptr);
        check(child, "editor child window");
#ifdef MULTIFX_JUCE_GUI
        HWND vectorSurface=nullptr;
        for(HWND candidate=GetWindow(child,GW_CHILD);candidate;candidate=GetWindow(candidate,GW_HWNDNEXT))
            if(IsWindowVisible(candidate)){vectorSurface=candidate;break;}
        check(vectorSurface,"JUCE vector surface attached to the VST3 editor");
#endif
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-initial.bmp").c_str());
        if(std::getenv("MULTIFX_VISUAL_ONLY")) {
            check(argc>=3,"visual capture needs output path");
            screenshot(child,argv[2]);
            return 0;
        }
        // Exercise the real DLL's meter notification and independent needle rendering.
        IConnectionPoint *meters=nullptr;
        check(c->queryInterface(IConnectionPoint::iid,reinterpret_cast<void **>(&meters))==kResultOk,"meter connection");
        const auto inSilent=meterPixels(child,1145,101),outSilent=meterPixels(child,1145,123);
        auto *message=new HostMessage;message->setMessageID("meters");
        message->getAttributes()->setFloat("in",.5);message->getAttributes()->setFloat("out",0);
        check(meters->notify(message)==kResultOk,"input meter notification");
        for(int i=0;i<8;++i)SendMessage(child,WM_TIMER,1,0);
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-meter.bmp").c_str());
        check(meterPixels(child,1145,101)!=inSilent,"IN meter follows input");
        check(meterPixels(child,1145,123)==outSilent,"OUT remains at silence independently");
        message->getAttributes()->setFloat("out",.2);meters->notify(message);
        for(int i=0;i<8;++i)SendMessage(child,WM_TIMER,1,0);
        check(meterPixels(child,1145,123)!=outSilent,"OUT meter follows output");
        message->release();meters->release();
        SendMessage(child,WM_RBUTTONUP,0,MAKELPARAM(187,132));
        check(std::abs(c->getParamNormalized(layout().midiLearn)-double(layout().blockBypass[0]+1)/layout().controlCount)<1e-6,"block bypass MIDI Learn");
        click(child,100,220); click(child,100,250); // Select Oil Drum for DELAY A from the engine selector.
        click(child,600,220); // Select bank B.
        click(child,100,220); click(child,100,250); // Select Oil Drum for DELAY B.
        click(child,470,220); // Return to bank A.
        check(c->getParamNormalized(layout().delayType[0]) == 1 &&
              c->getParamNormalized(layout().delayType[1]) == 1,
              "visual engine list selects real Oil Drum engines in both banks");
        click(child,1000,190);
        check(c->getParamNormalized(layout().digital[0]+2)>.3,
              "reference top MIX slider controls the selected delay bank");
        const int oilTime=parameterId("delayA.oil.time");
        const double oldOilTime=c->getParamNormalized(oilTime);
        SendMessage(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(120,330));
        SendMessage(child,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(120,290));
        SendMessage(child,WM_LBUTTONUP,0,MAKELPARAM(120,290));
        check(c->getParamNormalized(oilTime)>oldOilTime,
              "reference TIME knob controls the Oil Drum engine");
        click(child,1100,340);
        check(std::abs(c->getParamNormalized(layout().delayMode)-.5)<1e-6,
              "reference routing button selects real parallel DSP mode");
        click(child,1100,220);
        check(c->getParamNormalized(layout().slotBypass[0])==1,"reference bank bypass switches off");
        click(child,1100,220);
        check(c->getParamNormalized(layout().slotBypass[0])==0,"reference bank bypass switches on");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-delay.bmp").c_str());
        click(child,100,110); // PREAMP / DRIVE in the horizontal chain.
        SendMessage(child,WM_RBUTTONUP,0,MAKELPARAM(900,650));
        check(std::abs(c->getParamNormalized(layout().midiLearn)-double(layout().bypass+1)/layout().controlCount)<1e-6,"master bypass MIDI Learn");
        int gain = layout().preamp + 1;
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(427, 443));
        SendMessage(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(427, 403));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(427, 403));
        pump();
        check(c->getParamNormalized(gain) > .1, "preamp gain control changes host parameter");
        click(child, 900, 650);
        check(c->getParamNormalized(layout().bypass) == 1, "master bypass");
        click(child, 900, 650);
        check(c->getParamNormalized(layout().bypass) == 0, "master enable");
        SendMessage(child, WM_RBUTTONUP, 0, MAKELPARAM(427, 443));
        check(c->getParamNormalized(layout().midiLearn) > 0, "MIDI learn control");
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(100, 110));
        SendMessage(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(545, 110));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(545, 110));
        pump();
        check(c->getParamNormalized(layout().order) > 0, "drag and drop block order");
        ViewRect size(0, 0, 1024, 576);
        check(view->onSize(&size) == kResultOk, "compact GUI resize");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-small.bmp").c_str());
        ViewRect dpi125(0,0,1600,900);
        check(view->onSize(&dpi125)==kResultOk,"GUI scale 125 percent");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-125.bmp").c_str());
        ViewRect large(0,0,1920,1080);
        check(view->onSize(&large)==kResultOk,"GUI scale 150 percent");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-large.bmp").c_str());
        ViewRect normal(0, 0, 1280, 720);
        check(view->onSize(&normal) == kResultOk, "GUI restore scale");
        click(child, 330, 110); // Three separate effect panels in block two.
        pump();
        int model = -1;
        for (size_t i = 0; i < layout().parameters.size(); ++i)
            if (layout().parameters[i].key == "clone.model")
                model = int(i);
        HWND modelList = GetDlgItem(child, 10000 + model);
        check(modelList && SendMessage(modelList, CB_GETCOUNT, 0, 0) == 14, "original Chorus model dropdown");
        check(!IsWindowVisible(modelList), "Chorus model data stays behind vector drawn choice field");
        const double previousModel=c->getParamNormalized(model);
        SendMessage(modelList,CB_SETCURSEL,1,0);
        SendMessage(child,WM_COMMAND,MAKEWPARAM(10000+model,CBN_SELCHANGE),reinterpret_cast<LPARAM>(modelList));
        check(c->getParamNormalized(model)!=previousModel,"Chorus choice updates host parameter");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-clone.bmp").c_str());
        click(child,1210,543);
        check(GetDlgItem(child,10000+model)==nullptr,"next page replaces visible controls");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-clone-page2.bmp").c_str());
        click(child,1055,543);
        check(GetDlgItem(child,10000+model)!=nullptr,"previous page restores its controls");
        click(child, 340, 195);
        pump(); // Attack page.
        check(GetDlgItem(child, 10000 + model) == nullptr, "Chorus controls hidden on Attack page");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-attack.bmp").c_str());
        const int attack=parameterId("attack.bypass");
        click(child,1080,570);
        check(c->getParamNormalized(attack)==0,"dedicated Attack footswitch");
        SendMessage(child,WM_RBUTTONUP,0,MAKELPARAM(1080,570));
        check(std::abs(c->getParamNormalized(layout().midiLearn)-double(attack+1)/layout().controlCount)<1e-6,"Attack switch MIDI Learn");
        click(child, 555, 195);
        pump(); // Atomic page.
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-atomic.bmp").c_str());
        click(child, 100, 110);
        pump(); // Reordered MD block is now first.
        SendMessage(child,WM_RBUTTONUP,0,MAKELPARAM(120,330));
        check(std::abs(c->getParamNormalized(layout().midiLearn) -
                       double(parameterId("delayA.time") + 1) / layout().controlCount) < 1e-6,
              "reference DELAY TIME knob learns the common MIDI macro");
        click(child,1000,45); // Full parameter list.
        HWND family = GetDlgItem(child, 10000 + layout().delayType[0]);
        check(family && SendMessage(family, CB_GETCOUNT, 0, 0) == 13, "MD family dropdown");
        click(child, 320, 560);
        check(std::abs(c->getParamNormalized(layout().midiLearn) -
                       double(parameterId("delayA.time") + 1) / layout().controlCount) < 1e-6,
              "DELAY TIME MIDI Learn is available independently of the chosen engine");
        SendMessage(family, CB_SETCURSEL, 5, 0);
        SendMessage(child, WM_COMMAND, MAKEWPARAM(10000 + layout().delayType[0], CBN_SELCHANGE),
                    reinterpret_cast<LPARAM>(family));
        pump();
        int algorithm = -1;
        for (size_t i = 0; i < layout().parameters.size(); ++i)
            if (layout().parameters[i].key == "delayA.mx.slot0_algorithm")
                algorithm = int(i);
        // Choosing Brig from the single engine menu sets its original model and hides unrelated models.
        check(std::abs(c->getParamNormalized(algorithm) - .5) < 1e-6,
              "selected MX Brig model reaches controller");
        // Original Oil enums expose choices even when the source marked them continuous.
        family = GetDlgItem(child, 10000 + layout().delayType[0]);
        SendMessage(family, CB_SETCURSEL, 12, 0);
        SendMessage(child, WM_COMMAND, MAKEWPARAM(10000 + layout().delayType[0], CBN_SELCHANGE),
                    reinterpret_cast<LPARAM>(family));
        pump();
        const int division = parameterId("delayA.oil.division");
        HWND divisions = GetDlgItem(child, 10000 + division);
        for(int page=0;page<20&&!divisions;++page){
            click(child,1210,543); // Explicit parameter pages expose the remaining Oil controls.
            divisions=GetDlgItem(child,10000+division);
        }
        check(divisions && SendMessage(divisions, CB_GETCURSEL, 0, 0) == 3,
              "Oil original quarter-note enum displayed");
        RECT divisionRect{};GetWindowRect(divisions,&divisionRect);
        POINT divisionTop{divisionRect.left,divisionRect.top};ScreenToClient(child,&divisionTop);
        check(divisionTop.x>=20 && divisionTop.y>=218 && divisionTop.y<520,
              "continuous-tagged Oil enums occupy the compact selector row, never a knob");
        SendMessage(divisions, CB_SETCURSEL, 2, 0);
        SendMessage(child, WM_COMMAND, MAKEWPARAM(10000 + division, CBN_SELCHANGE),
                    reinterpret_cast<LPARAM>(divisions));
        check(std::abs(c->getParamNormalized(division) - 2. / 7) < 1e-6,
              "Oil enum normalized by real choice count");
        if (argc >= 3)
            screenshot(child, argv[2]);
        check(view->removed() == kResultOk, "detach GUI");
        view->release();
        DestroyWindow(parent);
        c->terminate();
        c->release();
        factory->release();
        check(exit(), "unload module");
        FreeLibrary(module);
    }
    std::cout << "Windows real VST3 GUI: attach, parameter edit, bypass, MIDI learn, reorder, resize, "
                 "screenshot, close passed\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
}
