#include "Plugin.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "public.sdk/source/common/pluginview.h"
#include <commdlg.h>
#include <cstdio>
#include <windows.h>
#include <windowsx.h>
namespace multifx {
class Editor final : public Steinberg::CPluginView {
    Controller *controller;
    HWND window = nullptr;
    HINSTANCE instance = nullptr;
    int block = 0, page = 0, drag = -1, reorder = -1, startY = 0;
    double startValue = 0;
    std::vector<int> shown;
    static constexpr int W = 1100, H = 740;
    float sx = 1, sy = 1;
    void change(int p, double v) {
        controller->beginEdit(p);
        controller->setParamNormalized(p, clean(float(v)));
        controller->performEdit(p, clean(float(v)));
        controller->endEdit(p);
        if (window)
            InvalidateRect(window, nullptr, FALSE);
    }
    double value(int p) {
        return controller->getParamNormalized(p);
    }
    void select() {
        shown.clear();
        auto &lay = layout();
        for (int i = 0; i < int(lay.parameters.size()); ++i) {
            bool belongs = block == 0   ? (i >= lay.preamp && i < lay.processors[1].start)
                           : block == 1 ? (i >= lay.processors[1].start && i < lay.delayMode)
                           : block == 2 ? (i >= lay.delayMode && i < lay.reverb)
                           : block == 3 ? (i >= lay.reverb && i < lay.ir)
                                        : (i >= lay.ir && i < lay.midiLearn);
            if (belongs)
                shown.push_back(i);
        }
        page = std::clamp(page, 0, int(shown.size() / 16));
    }
    static void fill(HDC dc, int x, int y, int w, int h, COLORREF color) {
        auto brush = CreateSolidBrush(color);
        RECT r{x, y, x + w, y + h};
        FillRect(dc, &r, brush);
        DeleteObject(brush);
    }
    static void text(HDC dc, int x, int y, int w, const char *label, int size, COLORREF color) {
        auto font = CreateFontA(size, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                                0, "Segoe UI");
        auto old = SelectObject(dc, font);
        SetTextColor(dc, color);
        SetBkMode(dc, TRANSPARENT);
        RECT r{x, y, x + w, y + size + 8};
        int count = MultiByteToWideChar(CP_UTF8, 0, label, -1, nullptr, 0);
        std::vector<wchar_t> wide(count);
        MultiByteToWideChar(CP_UTF8, 0, label, -1, wide.data(), count);
        DrawTextW(dc, wide.data(), -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dc, old);
        DeleteObject(font);
    }
    void paint() {
        PAINTSTRUCT ps;
        HDC target = BeginPaint(window, &ps), dc = CreateCompatibleDC(target);
        HBITMAP bmp = CreateCompatibleBitmap(target, W, H);
        auto old = SelectObject(dc, bmp);
        fill(dc, 0, 0, W, H, RGB(18, 24, 29));
        text(dc, 26, 20, 850, "JERZY MULTIFX   /   5 BLOCK AUDIO ENGINE", 28, RGB(232, 242, 246));
        text(dc, 28, 58, 950,
             "Drag blocks to reorder  |  Double click block: bypass  |  Right click a control: MIDI Learn",
             16, RGB(140, 163, 175));
        char meters[128];
        snprintf(meters, sizeof(meters), "IN %.1f dB  OUT %.1f dB  GR %.1f dB",
                 20 * std::log10(std::max(controller->meterIn.load(), 1.e-6f)),
                 20 * std::log10(std::max(controller->meterOut.load(), 1.e-6f)), controller->meterGR.load());
        text(dc, 700, 24, 380, meters, 14, RGB(110, 226, 188));
        const char *names[]{"ANALOG PROCESSOR", "CHORUS / ATTACK / ATOMIC", "MD MULTIDELAY", "REVERB",
                            "IR LOADER"};
        auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
        for (int i = 0; i < 5; ++i) {
            int b = order[i];
            bool bypass = value(layout().blockBypass[b]) >= .5;
            fill(dc, 24 + i * 212, 100, 204, 70, b == block ? RGB(24, 103, 112) : RGB(35, 46, 55));
            text(dc, 34 + i * 212, 114, 185, names[b], 15, RGB(238, 244, 247));
            text(dc, 34 + i * 212, 141, 180, bypass ? "BYPASS" : "ACTIVE", 13,
                 bypass ? RGB(240, 156, 98) : RGB(110, 226, 188));
        }
        char title[160];
        snprintf(title, sizeof(title), "%s    /    PARAMETER PAGE %d of %d", names[block], page + 1,
                 int(shown.size() / 16) + 1);
        text(dc, 30, 194, 1000, title, 20, RGB(223, 237, 241));
        for (int row = 0; row < 16; ++row) {
            int at = page * 16 + row;
            if (at >= int(shown.size()))
                break;
            int id = shown[at], col = row / 8, line = row % 8, x = 30 + col * 540, y = 240 + line * 51;
            const auto &p = layout().parameters[id];
            text(dc, x, y, 490, p.name.c_str(), 15, RGB(208, 223, 230));
            fill(dc, x, y + 26, 330, 8, RGB(57, 72, 83));
            fill(dc, x, y + 26, int(330 * value(id)), 8, RGB(70, 202, 189));
            Steinberg::Vst::String128 label{};
            controller->getParamStringByValue(id, value(id), label);
            char ascii[256]{};
            wchar_t wide[128]{};
            for (int k = 0; k < 127 && label[k]; ++k)
                wide[k] = wchar_t(label[k]);
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, ascii, sizeof(ascii), nullptr, nullptr);
            if (p.choices.empty())
                snprintf(ascii, sizeof(ascii), "%.3g %s", plain(id, float(value(id))), p.unit.c_str());
            text(dc, x + 346, y + 17, 165, ascii, 14, RGB(114, 219, 206));
        }
        const char *buttons[]{"< PAGE",      "PAGE >",      "LOAD IR WAV",
                              "LOAD PRESET", "SAVE PRESET", "MASTER BYPASS"};
        for (int i = 0; i < 6; ++i) {
            fill(dc, 26 + i * 178, 674, 168, 38, RGB(40, 57, 68));
            text(dc, 34 + i * 178, 683, 155, buttons[i], 15, RGB(221, 237, 241));
        }
        text(dc, 28, 720, 1000,
             "Software GDI renderer | CTRL + mouse wheel: fine adjustment | IR is embedded in project/preset",
             12, RGB(141, 164, 175));
        RECT rect;
        GetClientRect(window, &rect);
        SetStretchBltMode(target, HALFTONE);
        StretchBlt(target, 0, 0, rect.right, rect.bottom, dc, 0, 0, W, H, SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(window, &ps);
    }
    int hit(int x, int y) {
        if (y < 236 || y >= 648)
            return -1;
        int col = x >= 560 ? 1 : 0, row = (y - 240) / 51;
        if (row < 0 || row > 7)
            return -1;
        int at = page * 16 + col * 8 + row;
        return at < int(shown.size()) ? shown[at] : -1;
    }
    void file(const char *kind, bool save = false) {
        char path[4096]{};
        OPENFILENAMEA of{};
        of.lStructSize = sizeof(of);
        of.hwndOwner = window;
        of.lpstrFile = path;
        of.nMaxFile = sizeof(path);
        of.lpstrFilter =
            !strcmp(kind, "loadIR") ? "WAV impulse\0*.wav\0\0" : "Jerzy MultiFX preset\0*.jmfx\0\0";
        of.lpstrDefExt = save ? "jmfx" : nullptr;
        of.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
        if (save ? GetSaveFileNameA(&of) : GetOpenFileNameA(&of))
            controller->loadFile(path, kind);
    }
    LRESULT message(UINT msg, WPARAM w, LPARAM lp) {
        RECT rect;
        if (window) {
            GetClientRect(window, &rect);
            sx = float(W) / std::max(1L, rect.right);
            sy = float(H) / std::max(1L, rect.bottom);
        }
        int x = int(GET_X_LPARAM(lp) * sx), y = int(GET_Y_LPARAM(lp) * sy);
        switch (msg) {
        case WM_PAINT:
            paint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER:
            controller->requestMeters();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            if (y >= 100 && y < 170 && x >= 24 && x < 1084) {
                reorder = std::clamp((x - 24) / 212, 0, 4);
                auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
                block = order[reorder];
                page = 0;
                select();
                SetCapture(window);
            } else if (y >= 674 && y < 712) {
                int b = (x - 26) / 178;
                if (b == 0)
                    page = std::max(0, page - 1);
                if (b == 1)
                    page = std::min(int(shown.size() / 16), page + 1);
                if (b == 2)
                    file("loadIR");
                if (b == 3)
                    file("loadPreset");
                if (b == 4)
                    file("savePreset", true);
                if (b == 5)
                    change(layout().bypass, 1 - value(layout().bypass));
            } else {
                drag = hit(x, y);
                if (drag >= 0) {
                    startY = y;
                    startValue = value(drag);
                    SetCapture(window);
                }
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (drag >= 0) {
                double step = (w & MK_SHIFT) ? .0004 : .004;
                change(drag, startValue + (startY - y) * step);
            }
            return 0;
        case WM_LBUTTONUP:
            if (reorder >= 0) {
                int dest = std::clamp((x - 24) / 212, 0, 4);
                auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
                std::swap(order[reorder], order[dest]);
                int rank = 0;
                auto a = std::array<int, 5>{0, 1, 2, 3, 4};
                while (a != order && rank < 119) {
                    std::next_permutation(a.begin(), a.end());
                    ++rank;
                }
                change(layout().order, double(rank) / 119);
                reorder = -1;
            }
            drag = -1;
            ReleaseCapture();
            return 0;
        case WM_LBUTTONDBLCLK:
            if (y >= 100 && y < 170) {
                int i = std::clamp((x - 24) / 212, 0, 4);
                auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
                int id = layout().blockBypass[order[i]];
                change(id, 1 - value(id));
            }
            return 0;
        case WM_RBUTTONUP: {
            int id = hit(x, y);
            if (id >= 0)
                change(layout().midiLearn, double(id + 1) / layout().controlCount);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(window, &pt);
            int id = hit(int(pt.x * sx), int(pt.y * sy));
            if (id >= 0) {
                auto &p = layout().parameters[id];
                double step = p.steps ? 1. / p.steps : ((GET_KEYSTATE_WPARAM(w) & MK_CONTROL) ? .001 : .01);
                change(id, value(id) + (GET_WHEEL_DELTA_WPARAM(w) > 0 ? step : -step));
            }
            return 0;
        }
        }
        return DefWindowProcA(window, msg, w, lp);
    }
    static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        auto *self = reinterpret_cast<Editor *>(GetWindowLongPtrA(h, GWLP_USERDATA));
        if (m == WM_NCCREATE) {
            self = static_cast<Editor *>(reinterpret_cast<CREATESTRUCTA *>(l)->lpCreateParams);
            self->window = h;
            SetWindowLongPtrA(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->message(m, w, l) : DefWindowProcA(h, m, w, l);
    }

  public:
    explicit Editor(Controller *c) : controller(c) {
        c->addRef();
        Steinberg::ViewRect r(0, 0, W, H);
        setRect(r);
        select();
    }
    ~Editor() override {
        removed();
        controller->release();
    }
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString t) override {
        return t && !strcmp(t, Steinberg::kPlatformTypeHWND) ? Steinberg::kResultTrue
                                                             : Steinberg::kResultFalse;
    }
    Steinberg::tresult PLUGIN_API attached(void *parent, Steinberg::FIDString type) override {
        if (!parent || window || isPlatformTypeSupported(type) != Steinberg::kResultTrue)
            return Steinberg::kResultFalse;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&proc), &instance);
        WNDCLASSA wc{};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = proc;
        wc.hInstance = instance;
        wc.lpszClassName = "JerzyMultiFXEditor010";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return Steinberg::kResultFalse;
        window =
            CreateWindowExA(0, wc.lpszClassName, "Jerzy MultiFX", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0,
                            0, W, H, static_cast<HWND>(parent), nullptr, instance, this);
        if (!window)
            return Steinberg::kResultFalse;
        SetTimer(window, 1, 100, nullptr);
        systemWindow = parent;
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API removed() override {
        if (window) {
            KillTimer(window, 1);
            DestroyWindow(window);
            window = nullptr;
        }
        systemWindow = nullptr;
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API canResize() override {
        return Steinberg::kResultTrue;
    }
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect *r) override {
        if (!r)
            return Steinberg::kResultFalse;
        setRect(*r);
        if (window)
            MoveWindow(window, 0, 0, r->getWidth(), r->getHeight(), TRUE);
        return Steinberg::kResultOk;
    }
};
Steinberg::IPlugView *createEditor(Controller *c) {
    return new Editor(c);
}
} // namespace multifx
#else
namespace multifx {
Steinberg::IPlugView *createEditor(Controller *) {
    return nullptr;
}
} // namespace multifx
#endif
