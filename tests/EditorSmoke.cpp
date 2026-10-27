#include "plugin-vst3/Plugin.h"
#include "core/Presentation.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <cstdio>
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
    SendMessage(h, WM_PRINT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT | PRF_CHILDREN);
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
static unsigned long long meterPixels(HWND h, int x) {
    RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
    HDC screen=GetDC(h),dc=CreateCompatibleDC(screen);
    auto bitmap=CreateCompatibleBitmap(screen,1280,850);auto old=SelectObject(dc,bitmap);
    SendMessage(h,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
    unsigned long long hash=1469598103934665603ULL;
    for(int y=28;y<66;++y)for(int xx=x+10;xx<x+180;++xx) hash=(hash^GetPixel(dc,xx,y))*1099511628211ULL;
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
        ViewRect testSize(0, 0, 1280, 850);
        check(view->onSize(&testSize) == kResultOk, "set deterministic GUI test size");
        pump();
        HWND child = FindWindowExA(parent, nullptr, "JerzyMultiFXEditor010", nullptr);
        check(child, "editor child window");
        // Exercise the real DLL's meter notification and independent needle rendering.
        IConnectionPoint *meters=nullptr;
        check(c->queryInterface(IConnectionPoint::iid,reinterpret_cast<void **>(&meters))==kResultOk,"meter connection");
        const auto inSilent=meterPixels(child,850),outSilent=meterPixels(child,1054);
        auto *message=new HostMessage;message->setMessageID("meters");
        message->getAttributes()->setFloat("in",.5);message->getAttributes()->setFloat("out",0);
        check(meters->notify(message)==kResultOk,"input meter notification");
        for(int i=0;i<8;++i)SendMessage(child,WM_TIMER,1,0);
        check(meterPixels(child,850)!=inSilent,"IN needle follows input");
        check(meterPixels(child,1054)==outSilent,"OUT remains at silence independently");
        message->getAttributes()->setFloat("out",.2);meters->notify(message);
        for(int i=0;i<8;++i)SendMessage(child,WM_TIMER,1,0);
        check(meterPixels(child,1054)!=outSilent,"OUT needle follows output");
        message->release();meters->release();
        SendMessage(child,WM_RBUTTONUP,0,MAKELPARAM(100,135));
        check(std::abs(c->getParamNormalized(layout().midiLearn)-double(layout().blockBypass[0]+1)/layout().controlCount)<1e-6,"block bypass MIDI Learn");
        SendMessage(child,WM_RBUTTONUP,0,MAKELPARAM(900,820));
        check(std::abs(c->getParamNormalized(layout().midiLearn)-double(layout().bypass+1)/layout().controlCount)<1e-6,"master bypass MIDI Learn");
        int gain = layout().preamp + 1;
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(210, 366));
        SendMessage(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(210, 316));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(210, 316));
        pump();
        check(c->getParamNormalized(gain) > .1, "preamp gain control changes host parameter");
        click(child, 900, 820);
        check(c->getParamNormalized(layout().bypass) == 1, "master bypass");
        click(child, 900, 820);
        check(c->getParamNormalized(layout().bypass) == 0, "master enable");
        SendMessage(child, WM_RBUTTONUP, 0, MAKELPARAM(210, 366));
        check(c->getParamNormalized(layout().midiLearn) > 0, "MIDI learn control");
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(50, 95));
        SendMessage(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(550, 95));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(550, 95));
        pump();
        check(c->getParamNormalized(layout().order) > 0, "drag and drop block order");
        ViewRect size(0, 0, 880, 592);
        check(view->onSize(&size) == kResultOk, "GUI scale 80 percent");
        ViewRect normal(0, 0, 1280, 850);
        check(view->onSize(&normal) == kResultOk, "GUI restore scale");
        click(child, 300, 95); // Three separate effect panels in block two.
        pump();
        int model = -1;
        for (size_t i = 0; i < layout().parameters.size(); ++i)
            if (layout().parameters[i].key == "clone.model")
                model = int(i);
        HWND modelList = GetDlgItem(child, 10000 + model);
        check(modelList && SendMessage(modelList, CB_GETCOUNT, 0, 0) == 14, "original Chorus model dropdown");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-clone.bmp").c_str());
        click(child, 290, 195);
        pump(); // Attack page.
        check(GetDlgItem(child, 10000 + model) == nullptr, "Chorus controls hidden on Attack page");
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-attack.bmp").c_str());
        const int attack=parameterId("attack.bypass");
        click(child,1080,735);
        check(c->getParamNormalized(attack)==0,"dedicated Attack footswitch");
        SendMessage(child,WM_RBUTTONUP,0,MAKELPARAM(1080,735));
        check(std::abs(c->getParamNormalized(layout().midiLearn)-double(attack+1)/layout().controlCount)<1e-6,"Attack switch MIDI Learn");
        click(child, 520, 195);
        pump(); // Atomic page.
        if(argc>=3)screenshot(child,(std::string(argv[2])+"-atomic.bmp").c_str());
        click(child, 90, 95);
        pump(); // Reordered MD block is now first.
        HWND family = GetDlgItem(child, 10000 + layout().delayType[0]);
        check(family && SendMessage(family, CB_GETCOUNT, 0, 0) == 13, "MD family dropdown");
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
        check(divisions && SendMessage(divisions, CB_GETCURSEL, 0, 0) == 3,
              "Oil original quarter-note enum displayed");
        RECT divisionRect{};GetWindowRect(divisions,&divisionRect);
        POINT divisionTop{divisionRect.left,divisionRect.top};ScreenToClient(child,&divisionTop);
        check(divisionTop.y<300,"continuous-tagged Oil enums occupy the compact selector row, never a knob");
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
