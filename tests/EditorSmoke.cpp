#include "plugin-vst3/Plugin.h"
#include "pluginterfaces/base/ipluginbase.h"
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
    constexpr int w = 1100, height = 740;
    HDC dc = GetDC(h), memory = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, w, height);
    auto previous = SelectObject(memory, bmp);
    RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    check(BitBlt(memory, 0, 0, w, height, dc, 0, 0, SRCCOPY) != 0, "capture editor");
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
                                      0, 0, 1140, 800, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        check(parent, "create test host window");
        check(view->attached(parent, kPlatformTypeHWND) == kResultOk, "attach custom GUI");
        pump();
        HWND child = FindWindowExA(parent, nullptr, "JerzyMultiFXEditor010", nullptr);
        check(child, "editor child window");
        int gain = layout().preamp + 1;
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(100, 291));
        SendMessage(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(100, 241));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(100, 241));
        pump();
        check(c->getParamNormalized(gain) > .1, "preamp gain control changes host parameter");
        click(child, 980, 690);
        check(c->getParamNormalized(layout().bypass) == 1, "master bypass");
        click(child, 980, 690);
        check(c->getParamNormalized(layout().bypass) == 0, "master enable");
        SendMessage(child, WM_RBUTTONUP, 0, MAKELPARAM(100, 291));
        check(c->getParamNormalized(layout().midiLearn) > 0, "MIDI learn control");
        SendMessage(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(50, 125));
        SendMessage(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(480, 125));
        SendMessage(child, WM_LBUTTONUP, 0, MAKELPARAM(480, 125));
        pump();
        check(c->getParamNormalized(layout().order) > 0, "drag and drop block order");
        ViewRect size(0, 0, 880, 592);
        check(view->onSize(&size) == kResultOk, "GUI scale 80 percent");
        ViewRect normal(0, 0, 1100, 740);
        check(view->onSize(&normal) == kResultOk, "GUI restore scale");
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
