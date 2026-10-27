#include "Plugin.h"
#include "core/Presentation.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "public.sdk/source/common/pluginview.h"
#include <cstdio>
#include <sstream>
#include <windows.h>
#include <commdlg.h>
#include <windowsx.h>
namespace multifx {
class Editor final : public Steinberg::CPluginView {
    Controller *controller;
    HWND window = nullptr;
    HINSTANCE instance = nullptr;
    PanelSelection view;
    int drag = -1, reorder = -1, startY = 0, scroll = 0;
    double startValue = 0;
    float needleIn = 0, needleOut = 0;
    static constexpr int controlBottom = 660;
    float sx = 1, sy = 1;
    static constexpr int W = 1280, H = 850;
    struct Control {
        int id, x, y, w, h;
        bool compact;
    };
    std::vector<Control> controls;
    std::vector<HWND> combos;
    std::string signature;
    bool globalSettings = false;
    std::array<HFONT, 64> fonts{};
    int effectBypass() const {
        if (view.block != 1)
            return -1;
        return layout().processorBypass[view.section + 1];
    }
    bool isToggle(int id) const {
        const auto &p=layout().parameters[id];
        return p.steps==1 || p.key=="atomic.smooth" || p.key=="atomic.flat";
    }
    double value(int id) const {
        return id >= 0 ? controller->getParamNormalized(id) : 0;
    }
    State state() const {
        State s;
        for (size_t i = 0; i < layout().parameters.size(); ++i)
            s.values[i] = float(value(int(i)));
        return s;
    }
    void change(int id, double v) {
        if (id < 0)
            return;
        controller->beginEdit(id);
        controller->setParamNormalized(id, clean(float(v)));
        controller->performEdit(id, clean(float(v)));
        controller->endEdit(id);
        if (window)
            InvalidateRect(window, nullptr, FALSE);
    }
    void destroyCombos() {
        for (auto h : combos)
            DestroyWindow(h);
        combos.clear();
    }
    static void fill(HDC dc, int x, int y, int w, int h, COLORREF color) {
        HBRUSH b = CreateSolidBrush(color);
        RECT r{x, y, x + w, y + h};
        FillRect(dc, &r, b);
        DeleteObject(b);
    }
    void text(HDC dc, int x, int y, int w, const char *s, int size, COLORREF color) {
        size = std::clamp(size, 8, 63);
        if (!fonts[size])
            fonts[size] = CreateFontA(size, 0, 0, 0, FW_MEDIUM, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                      CLEARTYPE_QUALITY, 0, "Segoe UI");
        HFONT f = fonts[size] ? fonts[size] : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        auto old = SelectObject(dc, f);
        SetTextColor(dc, color);
        SetBkMode(dc, TRANSPARENT);
        RECT r{x, y, x + w, y + size + 8};
        int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
        std::vector<wchar_t> ws(n);
        MultiByteToWideChar(CP_UTF8, 0, s, -1, ws.data(), n);
        DrawTextW(dc, ws.data(), -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_CENTER);
        SelectObject(dc, old);
    }
    static void line(HDC dc, int x, int y, int xx, int yy, COLORREF color, int width = 1) {
        auto pen = CreatePen(PS_SOLID, width, color);
        auto old = SelectObject(dc, pen);
        MoveToEx(dc, x, y, nullptr); LineTo(dc, xx, yy);
        SelectObject(dc, old); DeleteObject(pen);
    }
    static void disk(HDC dc, int x, int y, int r, COLORREF color) {
        auto brush = CreateSolidBrush(color);
        auto old = SelectObject(dc, brush);
        auto pen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, x-r, y-r, x+r, y+r);
        SelectObject(dc, pen); SelectObject(dc, old); DeleteObject(brush);
    }
    static void bevel(HDC dc, int x, int y, int w, int h, COLORREF color) {
        fill(dc, x, y, w, h, color);
        line(dc, x, y, x+w-1, y, RGB(206,209,205));
        line(dc, x, y, x, y+h-1, RGB(186,189,185));
        line(dc, x, y+h-1, x+w, y+h-1, RGB(29,32,32));
        line(dc, x+w-1, y, x+w-1, y+h, RGB(29,32,32));
    }
    void vu(HDC dc, int x, const char *label, float amplitude) {
        bevel(dc, x, 9, 192, 65, RGB(25,27,26));
        fill(dc, x+5, 14, 182, 55, RGB(226,209,164));
        // Relative peak scale: 0 VU = -18 dBFS; the scale is not an RMS calibration.
        const int cx=x+96, cy=68;
        for (int i=0;i<=10;++i) {
            double angle=(-60.+i*12.)*3.141592653589793/180.;
            line(dc,cx+int(std::sin(angle)*39),cy-int(std::cos(angle)*39),
                    cx+int(std::sin(angle)*46),cy-int(std::cos(angle)*46),
                    i>=8?RGB(154,44,27):RGB(48,43,32));
        }
        text(dc,x+8,16,34,"-36",9,RGB(67,56,39));
        text(dc,x+100,15,32,"0",9,RGB(67,56,39));
        text(dc,x+146,16,36,"+18",9,RGB(154,44,27));
        const double db=20*std::log10(std::max(amplitude,1.e-6f));
        const double angle=(-60+120*std::clamp((db+54)/54,0.,1.))*3.141592653589793/180.;
        line(dc,cx,cy,cx+int(std::sin(angle)*43),cy-int(std::cos(angle)*43),RGB(102,30,20),2);
        disk(dc,cx,cy,4,RGB(44,38,30));
        text(dc,x+7,46,58,label,12,RGB(42,38,30));
        text(dc,x+126,47,59,"PEAK VU",9,RGB(67,56,39));
    }
    COLORREF accent() const { return RGB(111,58,24); }
    std::string formatted(int id) const {
        return parameterText(id, float(value(id)), value(parameterId("attack.poly")) >= .5);
    }

    void rebuild() {
        if (!window)
            return;
        destroyCombos();
        controls.clear();
        State s = state();
        auto ids = globalSettings ? std::vector<int>{layout().masterGain, layout().outputLimiter}
                                  : panelParameters(view, s);
        std::vector<int> compact, knobs;
        for (int id : ids) {
            if (id == effectBypass() && !globalSettings)
                continue;
            const auto &p = layout().parameters[id];
            const bool common =
                view.block == 2 &&
                (p.key.starts_with("md.") || p.key == "delayA.type" || p.key == "delayB.type" ||
                 p.key.ends_with(".bypass") ||
                 (p.key.find(".mx.") != std::string::npos && p.key.find("_algo") == std::string::npos));
            (common || p.steps > 0 || !p.choices.empty() || isToggle(id) || p.key == "atomic.stereo" ? compact : knobs).push_back(id);
        }
        int y = 236;
        for (size_t i = 0; i < compact.size(); ++i) {
            const int x = 28 + int(i % 6) * 205, cy = y + int(i / 6) * 62;
            controls.push_back({compact[i], x, cy, 194, 54, true});
        }
        y += int((compact.size() + 5) / 6) * 62 + 15;
        const int columns = view.block == 1 && view.section == 2 ? 2
                            : view.block == 2                        ? 6
                            : view.block == 1 && view.section == 1 ? 4
                                                                   : 3,
                  cell = W / columns,
                  rowHeight = view.block == 2                        ? 130
                              : view.block == 1 && view.section == 1 ? 160
                                                                     : 145;
        for (size_t i = 0; i < knobs.size(); ++i)
            controls.push_back({knobs[i], int(i % columns) * cell + 20, y + int(i / columns) * rowHeight,
                                cell - 40, rowHeight - 8, false});
        for (auto &c : controls) {
            auto &p = layout().parameters[c.id];
            if (p.choices.empty())
                continue;
            const int yy = c.y - scroll;
            if (yy < 232 || yy + 55 > controlBottom)
                continue;
            HWND h = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                                   int(c.x / sx), int((yy + 23) / sy), int(c.w / sx), 240, window,
                                   reinterpret_cast<HMENU>(INT_PTR(10000 + c.id)), instance, nullptr);
            if (!h)
                continue;
            const int fontSize = std::clamp(int(15 / sy), 8, 63);
            if (!fonts[fontSize]) fonts[fontSize] = CreateFontA(fontSize,0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
            SendMessage(h, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[fontSize]), TRUE);
            const bool family = p.key == "delayA.type" || p.key == "delayB.type";
            std::istringstream choices(family ? mdEngineChoices : p.choices);
            std::string label;
            while (std::getline(choices, label, '|'))
                SendMessageA(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            int choice = int(
                std::round(value(c.id) * (family ? 4 : std::count(p.choices.begin(), p.choices.end(), '|'))));
            if (family) {
                const int bank = p.key == "delayA.type" ? 0 : 1;
                choice = choice == 0   ? 0
                         : choice == 1 ? 1 + int(std::round(value(layout().mxAlgorithms[bank][0]) * 8))
                                       : choice + 8;
            }
            SendMessage(h, CB_SETCURSEL, choice, 0);
            combos.push_back(h);
        }
        HWND factory = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, int(430 / sx),
                                     int(50 / sy), int(400 / sx), 260, window,
                                     reinterpret_cast<HMENU>(INT_PTR(10000 + layout().factoryProgram)),
                                     instance, nullptr);
        if (factory) {
            std::istringstream choices(layout().parameters[layout().factoryProgram].choices);
            std::string label;
            while (std::getline(choices, label, '|'))
                SendMessageA(factory, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            SendMessage(factory, CB_SETCURSEL, int(std::round(value(layout().factoryProgram) * 7)), 0);
            combos.push_back(factory);
        }
        signature = selectionSignature();
        InvalidateRect(window, nullptr, FALSE);
    }
    std::string selectionSignature() const {
        auto s = state();
        std::string key = std::to_string(view.block) + ":" + std::to_string(view.section) + ":" +
                          std::to_string(view.mxSlot) + ":" + std::to_string(view.advanced) + ":" +
                          std::to_string(globalSettings);
        for (auto &p : layout().parameters)
            if (p.key.ends_with(".type") || p.key.ends_with("_algorithm") || p.key.ends_with(".sync") ||
                p.key.ends_with("_sync") || p.key.ends_with(".tempoSync")) {
                int id = parameterId(p.key);
                key += ":" + std::to_string(value(id));
            }
        return key;
    }
    void paint() {
        PAINTSTRUCT ps;
        auto target = BeginPaint(window, &ps);
        RECT client;
        GetClientRect(window, &client);
        sx = float(W) / std::max(1L, client.right);
        sy = float(H) / std::max(1L, client.bottom);
        auto dc = CreateCompatibleDC(target);
        auto bmp = CreateCompatibleBitmap(target, W, H);
        auto old = SelectObject(dc, bmp);
        fill(dc, 0, 0, W, H, RGB(44,47,46));
        // Solid walnut cheeks, steel chassis and a brushed aluminium faceplate.
        for(int y=0;y<H;++y) {
            int grain=int(5*std::sin(y*.11)+3*std::sin(y*.47));
            fill(dc,0,y,16,1,RGB(80+grain,45+grain,25+grain));
            fill(dc,1264,y,16,1,RGB(80+grain,45+grain,25+grain));
        }
        text(dc, 24, 16, 400, "JERZY MULTIFX", 27, RGB(236,226,201));
        text(dc, 24, 51, 400, "STUDIO EFFECTS / PREVIEW 3", 13, RGB(185,181,161));
        vu(dc, 850, "IN", needleIn); vu(dc, 1054, "OUT", needleOut);
        bevel(dc, 430, 13, 400, 28, RGB(74,78,75));
        text(dc, 430, 17, 400, globalSettings ? "BACK TO EFFECT" : "SETTINGS / OUTPUT", 14, RGB(238,226,200));
        auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
        for (int i = 0; i < 5; ++i) {
            int b = order[i], x = 20 + i * 248;
            fill(dc, x, 80, 238, 76, b == view.block ? RGB(108,112,104) : RGB(62,66,63));
            text(dc, x + 4, 92, 230, blockName(b), 15, RGB(238, 238, 225));
            const bool bypass = value(layout().blockBypass[b]) >= .5;
            fill(dc, x + 25, 122, 188, 25, bypass ? RGB(82, 48, 43) : RGB(35, 105, 84));
            text(dc, x + 25, 124, 188, bypass ? "BYPASS / OFF" : "ON", 14, RGB(242, 240, 220));
        }
        auto sections = sectionNames(view.block);
        for (size_t i = 0; i < sections.size(); ++i) {
            int x = 28 + int(i) * 230;
            fill(dc, x, 177, 218, 36, int(i) == view.section ? RGB(190,184,163) : RGB(68,72,68));
            text(dc, x, 184, 218, sections[i].c_str(), 16,
                 int(i) == view.section ? RGB(15, 20, 24) : RGB(225, 225, 216));
        }
        if (view.block == 2 && view.advanced) {
            text(dc, 540, 218, 350, view.mxSlot == 0 ? "MX ENGINE A" : "MX ENGINE B", 15, RGB(232,221,190));
            fill(dc, 900, 177, 160, 36, RGB(42, 57, 66));
            text(dc, 900, 184, 160, "MX A / B", 15, RGB(232,221,190));
        }
        fill(dc, 1075, 177, 175, 36, RGB(42, 57, 66));
        text(dc, 1075, 184, 175, view.advanced ? "BASIC" : "SETTINGS", 15, RGB(232,221,190));
        COLORREF panel = RGB(155,160,155);
        fill(dc, 20, 229, 1240, 562, panel);
        for(int y=230;y<791;y+=3)
            line(dc,21,y,1259,y,RGB(151,156,151));
        bevel(dc, 24, 674, 1232, 112, RGB(53,57,54));
        for (int x : {30,1248}) for(int y : {238,778}) {
            disk(dc,x,y,5,RGB(198,199,184));
            line(dc,x-3,y,x+3,y,RGB(50,51,46));
        }
        if (view.block == 1 && !globalSettings) {
            if (view.section == 1) {

                text(dc, 40, 695, 460, "ATTACK / DECAY", 32, RGB(232, 231, 220));
                fill(dc, 525, 704, 230, 36, RGB(55, 61, 62));
                text(dc, 525, 711, 230, "TRIGGER", 16, RGB(238, 225, 198));
            } else
                text(dc, 35, 694, 450, view.section == 0 ? "CLONE COLLECTION" : "ATOMIC CLUSTER", 27,
                     RGB(232,221,190));
        }
        if (effectBypass() >= 0 && !globalSettings) {
            const int cx = view.section == 1 ? 1080 : W / 2, cy = 735;
            HBRUSH b = CreateSolidBrush(RGB(173, 181, 176));
            auto oldBrush = SelectObject(dc, b);
            Ellipse(dc, cx - 27, cy - 27, cx + 27, cy + 27);
            SelectObject(dc, oldBrush);
            DeleteObject(b);
            text(dc, cx - 110, 765, 220, value(effectBypass()) >= .5 ? "BYPASS / OFF" : "EFFECT ON", 13,
                 RGB(232,221,190));
            HBRUSH led = CreateSolidBrush(value(effectBypass()) >= .5 ? RGB(94, 48, 40) : RGB(115,225,105));
            oldBrush = SelectObject(dc, led);
            Ellipse(dc, cx - 5, cy - 42, cx + 5, cy - 32);
            SelectObject(dc, oldBrush);
            DeleteObject(led);
        }
        if (!globalSettings && view.block == 0 && view.section == 2 && value(layout().fuzzModel) * 19 >= 5.5)
            text(dc, 28, 755, 1220,
                 "Circuit models use the original nonlinear solver and require much more CPU. Legacy models "
                 "are lighter for live use.",
                 14, RGB(232,221,190));
        if (globalSettings)
            text(dc, 28, 735, 1220,
                 "The audio device and buffer are configured in your DAW. Effect latency is reported to the "
                 "host.",
                 15, RGB(232,221,190));
        if (view.block == 4 && !controller->fileStatus.empty())
            text(dc, 28, 755, 1220, controller->fileStatus.c_str(), 15, RGB(232,221,190));
        char status[96];
        snprintf(status,sizeof(status),"GR %.1f dB / PDC %d smp",controller->meterGR.load(),controller->meterLatency.load());
        text(dc,790,682,230,status,12,RGB(220,213,187));
        int contentEnd=controlBottom;
        for(const auto &c:controls) contentEnd=std::max(contentEnd,c.y+c.h+10);
        if(contentEnd>controlBottom) {
            fill(dc,1256,244,3,402,RGB(91,97,90));
            const int thumb=std::max(24,402*(controlBottom-232)/(contentEnd-232));
            fill(dc,1256,244+(402-thumb)*scroll/std::max(1,contentEnd-controlBottom),3,thumb,RGB(227,215,177));
        }
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 20, 232, 1260, controlBottom);
        for (auto &c : controls) {
            int y = c.y - scroll;
            if (y + c.h < 232 || y > controlBottom)
                continue;
            const auto &p = layout().parameters[c.id];
            const auto label = controlTitle(c.id);
            if (c.compact) {
                text(dc, c.x, y, c.w, label.c_str(), 13, RGB(30,34,31));
                if (!p.choices.empty())
                    continue;
                bevel(dc, c.x, y + 23, c.w, 29, RGB(67,71,67));
                if (isToggle(c.id)) {
                    const bool on=p.key.ends_with(".bypass") ? value(c.id)<.5 : value(c.id)>=.5;
                    disk(dc,c.x+16,y+38,7,on?RGB(105,220,96):RGB(48,50,43));
                    bevel(dc,c.x+35,y+27,12,21,RGB(28,29,26));
                    bevel(dc,c.x+33,y+(on?26:37),16,10,RGB(208,209,195));
                }
                text(dc, c.x+50, y + 28, c.w-54,
                     p.key.ends_with(".bypass") ? (value(c.id)>=.5 ? "BYPASS / OFF" : "EFFECT ON") : isToggle(c.id) ? (value(c.id) >= .5 ? "ON" : "OFF") : formatted(c.id).c_str(), 14,
                     RGB(238,225,192));
                if (p.steps == 0 && !isToggle(c.id)) {
                    fill(dc, c.x, y + 51, c.w, 3, RGB(16, 27, 32));
                    fill(dc, c.x, y + 51, int(c.w * value(c.id)), 3, accent());
                }
            } else {
                const int cx = c.x + c.w / 2, cy = y + 53, r = 39;
                // Layered machined aluminium skirt and ribbed ebonite grip.
                disk(dc,cx+3,cy+5,r+3,RGB(70,74,69));
                disk(dc,cx,cy,r+2,RGB(56,59,55));
                for(int rr=r;rr>=r-7;--rr) {
                    int shade=174+(r-rr)*8;
                    disk(dc,cx-1,cy-1,rr,RGB(shade,shade,shade-6));
                }
                disk(dc,cx,cy,r-9,RGB(26,28,25));
                disk(dc,cx-2,cy-3,r-13,RGB(48,50,45));
                for(int i=0;i<32;++i) {
                    double a=i*6.283185307179586/32;
                    line(dc,cx+int(std::sin(a)*(r-11)),cy-int(std::cos(a)*(r-11)),
                            cx+int(std::sin(a)*(r-7)),cy-int(std::cos(a)*(r-7)),RGB(90,93,83));
                }
                for(int i=0;i<=10;++i) {
                    double a=(-135+i*27)*3.141592653589793/180;
                    line(dc,cx+int(std::sin(a)*(r+8)),cy-int(std::cos(a)*(r+8)),
                            cx+int(std::sin(a)*(r+12)),cy-int(std::cos(a)*(r+12)),RGB(55,59,53));
                }
                double angle = (-135 + 270 * value(c.id)) * 3.141592653589793 / 180;
                auto pen = CreatePen(PS_SOLID, 3, RGB(244,230,189));
                auto op = SelectObject(dc, pen);
                MoveToEx(dc, cx + int(std::sin(angle) * 19), cy - int(std::cos(angle) * 19), nullptr);
                LineTo(dc, cx + int(std::sin(angle) * 32), cy - int(std::cos(angle) * 32));
                SelectObject(dc, op);
                DeleteObject(pen);
                text(dc, c.x, y + 96, c.w, label.c_str(), 14, RGB(25,29,26));
                text(dc, c.x, y + 114, c.w, formatted(c.id).c_str(), 13, accent());
            }
        }
        RestoreDC(dc, saved);
        const char *buttons[]{"LOAD IR WAV", "LOAD PRESET", "SAVE PRESET", "MASTER BYPASS"};
        buttons[3]=value(layout().bypass)>=.5 ? "MASTER BYPASSED" : "MASTER ACTIVE";
        for (int i = 0; i < 4; ++i) {
            fill(dc, 28 + i * 250, 805, 238, 32, RGB(93,98,90));
            text(dc, 28 + i * 250, 811, 238, buttons[i], 14, RGB(239,227,195));
        }
        text(dc, 1028, 807, 225, "Right click: MIDI Learn", 13, RGB(160, 171, 175));
        SetStretchBltMode(target, HALFTONE);
        StretchBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, W, H, SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(window, &ps);
    }
    int hit(int x, int y) const {
        for (auto &c : controls)
            if (y >= 232 && y < controlBottom && x >= c.x && x < c.x + c.w && y + scroll >= c.y && y + scroll < c.y + c.h)
                return c.id;
        return -1;
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
        of.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
        of.lpstrDefExt = save ? "jmfx" : nullptr;
        if (save ? GetSaveFileNameA(&of) : GetOpenFileNameA(&of))
            controller->loadFile(path, kind);
    }
    LRESULT message(UINT msg, WPARAM w, LPARAM lp) {
        int x = int(GET_X_LPARAM(lp) * sx), y = int(GET_Y_LPARAM(lp) * sy);
        switch (msg) {
        case WM_PAINT:
            paint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (window)
                PostMessage(window, WM_APP + 1, 0, 0);
            return 0;
        case WM_APP + 1:
            rebuild();
            return 0;
        case WM_TIMER: {
            if (w == 2) {
                KillTimer(window, 2);
                change(layout().attackTrigger, 0);
                return 0;
            }
            auto follow = [](float current, float target) {
                if (!std::isfinite(target)) target=0;
                return current + (std::max(0.f,target)-current) * (target>current ? .65f : .18f);
            };
            needleIn=follow(needleIn,controller->meterIn.load());
            needleOut=follow(needleOut,controller->meterOut.load());
            if (drag < 0 && selectionSignature() != signature)
                rebuild();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_COMMAND:
            if (HIWORD(w) == CBN_SELCHANGE) {
                int id = LOWORD(w) - 10000;
                if (id >= 0 && id < int(layout().parameters.size())) {
                    const int choice = int(SendMessage(reinterpret_cast<HWND>(lp), CB_GETCURSEL, 0, 0));
                    const auto &key = layout().parameters[id].key;
                    if (key == "delayA.type" || key == "delayB.type") {
                        const int bank = key == "delayA.type" ? 0 : 1;
                        const int family = choice == 0 ? 0 : choice <= 9 ? 1 : choice - 8;
                        change(id, double(family) / 4);
                        if (choice > 0)
                            change(layout().digital[bank] + 2, 1);
                        if (family == 1) {
                            change(layout().mxAlgorithms[bank][0], double(choice - 1) / 8);
                            int enabled =
                                parameterId(std::string(bank ? "delayB" : "delayA") + ".mx.slot1_enable");
                            change(enabled, 0);
                        }
                    } else
                        change(id,
                               double(choice) /
                                   std::max(1, int(std::count(layout().parameters[id].choices.begin(),
                                                              layout().parameters[id].choices.end(), '|'))));
                    scroll = 0;
                    rebuild();
                }
            }
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            if (view.block == 1 && !globalSettings && y >= 700 && y <= 768) {
                const int center = view.section == 1 ? 1080 : W / 2;
                if (x >= center - 40 && x <= center + 40) {
                    const int id = effectBypass();
                    change(id, value(id) >= .5 ? 0 : 1);
                    return 0;
                }
                if (view.section == 1 && x >= 525 && x <= 755) {
                    change(layout().attackTrigger, 1);
                    SetTimer(window, 2, 120, nullptr);
                    return 0;
                }
            }
            if (y >= 13 && y < 41 && x >= 430 && x < 830) {
                globalSettings = !globalSettings;
                scroll = 0;
                rebuild();
                return 0;
            }
            if (y >= 80 && y < 156 && x >= 20 && x < 1260) {
                int at = std::clamp((x - 20) / 248, 0, 4);
                auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
                if (y >= 122)
                    change(layout().blockBypass[order[at]],
                           value(layout().blockBypass[order[at]]) >= .5 ? 0 : 1);
                else {
                    reorder = at;
                    view.block = order[at];
                    view.section = 0;
                    globalSettings = false;
                    scroll = 0;
                    rebuild();
                    SetCapture(window);
                }
                return 0;
            }
            if (y >= 177 && y < 213) {
                auto sections = sectionNames(view.block);
                if (x >= 1075)
                    view.advanced = !view.advanced;
                else if (view.block == 2 && x >= 900)
                    view.mxSlot = 1 - view.mxSlot;
                else if (x >= 28 && size_t((x - 28) / 230) < sections.size())
                    view.section = (x - 28) / 230;
                scroll = 0;
                rebuild();
                return 0;
            }
            if (y >= 805 && x >= 28 && x < 1028) {
                int b = (x - 28) / 250;
                if (b == 0)
                    file("loadIR");
                if (b == 1)
                    file("loadPreset");
                if (b == 2)
                    file("savePreset", true);
                if (b == 3)
                    change(layout().bypass, value(layout().bypass) >= .5 ? 0 : 1);
                return 0;
            }
            if (y < 232 || y > controlBottom)
                return 0;
            int id = hit(x, y);
            if (id >= 0) {
                if (isToggle(id)) {
                    change(id, value(id) >= .5 ? 0 : 1);
                    return 0;
                }
                if (layout().parameters[id].steps > 1)
                    return 0;
                drag = id;
                startY = y;
                startValue = value(id);
                SetCapture(window);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (drag >= 0)
                change(drag, startValue + (startY - y) * ((w & MK_SHIFT) ? .0004 : .004));
            return 0;
        case WM_LBUTTONUP:
            if (reorder >= 0) {
                int dest = std::clamp((x - 20) / 248, 0, 4);
                auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
                std::swap(order[reorder], order[dest]);
                auto a = std::array<int, 5>{0, 1, 2, 3, 4};
                int rank = 0;
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
        case WM_RBUTTONUP: {
            int id = hit(x, y);
            if (view.block == 1 && y >= 695 && y <= 785) {
                const int center = view.section == 1 ? 1080 : W / 2;
                if (std::abs(x - center) < 60)
                    id = effectBypass();
            }
            if (y >= 122 && y < 156 && x >= 20 && x < 1260) {
                auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
                id=layout().blockBypass[order[std::clamp((x-20)/248,0,4)]];
            }
            if (y >= 805 && y < 837 && x >= 778 && x < 1016) id=layout().bypass;
            if (id >= 0)
                change(layout().midiLearn, double(id + 1) / layout().controlCount);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(window, &pt);
            int xx = int(pt.x * sx), yy = int(pt.y * sy), id = hit(xx, yy);
            if (id >= 0 && (GET_KEYSTATE_WPARAM(w) & MK_CONTROL)) {
                double step = layout().parameters[id].steps ? 1. / layout().parameters[id].steps : .001;
                change(id, value(id) + (GET_WHEEL_DELTA_WPARAM(w) > 0 ? step : -step));
            } else {
                int bottom = 0;
                for (auto &c : controls)
                    bottom = std::max(bottom, c.y + c.h);
                scroll = std::clamp(scroll + (GET_WHEEL_DELTA_WPARAM(w) > 0 ? -100 : 100), 0,
                                    std::max(0, bottom - (controlBottom - 10)));
                rebuild();
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
        const double fit = std::clamp(std::min(double(GetSystemMetrics(SM_CXSCREEN) - 80) / W,
                                               double(GetSystemMetrics(SM_CYSCREEN) - 120) / H),
                                      .6, 1.);
        Steinberg::ViewRect r(0, 0, int(W * fit), int(H * fit));
        setRect(r);
        sx = float(W) / r.getWidth();
        sy = float(H) / r.getHeight();
    }
    ~Editor() override {
        removed();
        for (auto f : fonts)
            if (f)
                DeleteObject(f);
        controller->release();
    }
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override {
        return type && !strcmp(type, Steinberg::kPlatformTypeHWND) ? Steinberg::kResultTrue
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
        window = CreateWindowExA(0, wc.lpszClassName, "Jerzy MultiFX",
                                 WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, getRect().getWidth(),
                                 getRect().getHeight(), static_cast<HWND>(parent), nullptr, instance, this);
        if (!window)
            return Steinberg::kResultFalse;
        systemWindow = parent;
        SetTimer(window, 1, 33, nullptr);
        rebuild();
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API removed() override {
        if (window) {
            if (value(layout().attackTrigger) >= .5)
                change(layout().attackTrigger, 0);
            destroyCombos();
            KillTimer(window, 1);
            KillTimer(window, 2);
            DestroyWindow(window);
            window = nullptr;
            UnregisterClassA("JerzyMultiFXEditor010", instance);
        }
        systemWindow = nullptr;
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API canResize() override {
        return Steinberg::kResultTrue;
    }
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect *r) override {
        if (!r || r->getWidth() < 640 || r->getHeight() < 425)
            return Steinberg::kResultFalse;
        setRect(*r);
        if (window) {
            sx = float(W) / r->getWidth();
            sy = float(H) / r->getHeight();
            MoveWindow(window, 0, 0, r->getWidth(), r->getHeight(), TRUE);
            rebuild();
        }
        return Steinberg::kResultOk;
    }
};
Steinberg::IPlugView *createEditor(Controller *controller) {
    return new Editor(controller);
}
} // namespace multifx
#else
namespace multifx {
Steinberg::IPlugView *createEditor(Controller *) {
    return nullptr;
}
} // namespace multifx
#endif
