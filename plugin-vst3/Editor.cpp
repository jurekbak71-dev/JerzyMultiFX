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
    int drag = -1, reorder = -1, startX = 0, startY = 0, scroll = 0;
    double startValue = 0;
    float needleIn = 0, needleOut = 0;
    static constexpr int controlBottom = 660;
    float sx = 1, sy = 1;
    static constexpr int W = 1280, H = 850, sidebar = 220;
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
    int delayMidiTarget(int x, int y) const {
        if (globalSettings || view.block != 2 || y < 700 || y >= 748 || x < 250 || x >= 1000)
            return -1;
        const int index = (x - 250) / 250;
        const std::string bank = view.section ? "delayB." : "delayA.";
        return parameterId(bank + std::array<const char *, 3>{"time", "feedback", "mix"}[index]);
    }
    bool isToggle(int id) const {
        return std::string_view(controlUI()[id].control) == "switch";
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
        const int steps = controlUI()[id].definition.steps;
        v = clean(float(v));
        if (steps > 0)
            v = std::round(v * steps) / steps;
        controller->beginEdit(id);
        controller->setParamNormalized(id, v);
        controller->performEdit(id, v);
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
        line(dc, x, y, x+w-1, y, RGB(69,88,103));
        line(dc, x, y, x, y+h-1, RGB(51,69,84));
        line(dc, x, y+h-1, x+w, y+h-1, RGB(10,19,30));
        line(dc, x+w-1, y, x+w-1, y+h, RGB(10,19,30));
    }
    void vu(HDC dc, int x, const char *label, float amplitude) {
        bevel(dc, x, 9, 192, 65, RGB(16,30,45));
        fill(dc, x+5, 14, 182, 55, RGB(24,43,59));
        // Relative peak scale: 0 VU = -18 dBFS; the scale is not an RMS calibration.
        const int cx=x+96, cy=68;
        for (int i=0;i<=10;++i) {
            double angle=(-60.+i*12.)*3.141592653589793/180.;
            line(dc,cx+int(std::sin(angle)*39),cy-int(std::cos(angle)*39),
                    cx+int(std::sin(angle)*46),cy-int(std::cos(angle)*46),
                    i>=8?RGB(255,122,94):RGB(76,157,169));
        }
        text(dc,x+8,16,34,"-36",9,RGB(160,191,206));
        text(dc,x+100,15,32,"0",9,RGB(160,191,206));
        text(dc,x+146,16,36,"+18",9,RGB(255,122,94));
        const double db=20*std::log10(std::max(amplitude,1.e-6f));
        const double angle=(-60+120*std::clamp((db+54)/54,0.,1.))*3.141592653589793/180.;
        line(dc,cx,cy,cx+int(std::sin(angle)*43),cy-int(std::cos(angle)*43),RGB(69,226,239),2);
        disk(dc,cx,cy,4,RGB(69,226,239));
        text(dc,x+7,46,58,label,12,RGB(238,248,250));
        text(dc,x+126,47,59,"PEAK VU",9,RGB(160,191,206));
    }
    COLORREF accent() const { return view.block == 2 && view.section ? RGB(176,124,255) : RGB(60,213,231); }
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
            const auto &p = controlUI()[id].definition;
            const bool common =
                view.block == 2 &&
                (p.key.starts_with("md.") || p.key == "delayA.type" || p.key == "delayB.type" ||
                 p.key == "delayA.mix" || p.key == "delayB.mix" ||
                 p.key == "delayA.time" || p.key == "delayB.time" ||
                 p.key == "delayA.feedback" || p.key == "delayB.feedback" ||
                 p.key.ends_with(".bypass") ||
                 (p.key.find(".mx.") != std::string::npos && p.key.find("_algo") == std::string::npos));
            (common || p.steps > 0 || !p.choices.empty() || isToggle(id) || p.key == "atomic.stereo" ? compact : knobs).push_back(id);
        }
        int y = 236;
        for (size_t i = 0; i < compact.size(); ++i) {
            const int x = 248 + int(i % 5) * 200, cy = y + int(i / 5) * 62;
            controls.push_back({compact[i], x, cy, 188, 54, true});
        }
        y += int((compact.size() + 4) / 5) * 62 + 15;
        const int columns = view.block == 1 && view.section == 2 ? 2
                            : view.block == 2                        ? 5
                            : view.block == 1 && view.section == 1 ? 4
                                                                   : 3,
                  cell = (W - sidebar - 28) / columns,
                  rowHeight = view.block == 2                        ? 130
                              : view.block == 1 && view.section == 1 ? 160
                                                                     : 145;
        for (size_t i = 0; i < knobs.size(); ++i)
            controls.push_back({knobs[i], 240 + int(i % columns) * cell + 12, y + int(i / columns) * rowHeight,
                                cell - 24, rowHeight - 8, false});
        for (auto &c : controls) {
            const auto &p = controlUI()[c.id].definition;
            if (p.choices.empty() && !(p.steps > 1 && p.steps <= 64))
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
            if (p.choices.empty()) {
                for (int option = 0; option <= p.steps; ++option) {
                    char label[32];
                    snprintf(label, sizeof(label), "%d", int(std::round(p.minimum +
                             (p.maximum - p.minimum) * option / p.steps)));
                    SendMessageA(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
                }
            } else {
                std::istringstream choices(family ? mdEngineChoices : p.choices);
                std::string label;
                while (std::getline(choices, label, '|'))
                    SendMessageA(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            }
            int choice = int(
                std::round(value(c.id) * (family ? 4 : p.choices.empty() ? p.steps :
                            std::count(p.choices.begin(), p.choices.end(), '|'))));
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
    void paint(HDC supplied = nullptr) {
        PAINTSTRUCT ps{};
        auto target = supplied ? supplied : BeginPaint(window, &ps);
        RECT client;
        GetClientRect(window, &client);
        sx = float(W) / std::max(1L, client.right);
        sy = float(H) / std::max(1L, client.bottom);
        auto dc = CreateCompatibleDC(target);
        auto bmp = CreateCompatibleBitmap(target, W, H);
        auto old = SelectObject(dc, bmp);
        fill(dc, 0, 0, W, H, RGB(9,19,31));
        fill(dc, 0, 78, W, 2, RGB(32,96,116));
        text(dc, 24, 16, 400, "JERZY MULTIFX", 27, RGB(229,249,252));
        text(dc, 24, 51, 400, "BASS EFFECTS / LIVE STUDIO", 13, RGB(104,177,198));
        vu(dc, 850, "IN", needleIn); vu(dc, 1054, "OUT", needleOut);
        bevel(dc, 430, 13, 400, 28, RGB(22,42,58));
        text(dc, 430, 17, 400, globalSettings ? "BACK TO EFFECT" : "SETTINGS / OUTPUT", 14, RGB(223,244,248));
        auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
        for (int i = 0; i < 5; ++i) {
            int b = order[i], x = 20, y = 90 + i * 113;
            bevel(dc, x, y, 202, 104, b == view.block ? RGB(21,54,72) : RGB(15,31,47));
            if (b == view.block) fill(dc, x, y, 4, 104, accent());
            text(dc, x + 7, y + 12, 188, blockName(b), 15, RGB(231,247,250));
            const bool bypass = value(layout().blockBypass[b]) >= .5;
            fill(dc, x + 12, y + 62, 178, 29, bypass ? RGB(95, 36, 48) : RGB(22, 95, 70));
            disk(dc, x + 28, y + 76, 5, bypass ? RGB(241,81,99) : RGB(65,236,143));
            text(dc, x + 28, y + 67, 162, bypass ? "BYPASS / OFF" : "ON / AKTYWNY", 14, RGB(242, 251, 251));
        }
        if (view.block == 2 && !globalSettings) {
            for (int bank = 0; bank < 2; ++bank) {
                const int x = 248 + bank * 506;
                const int type = int(plain(layout().delayType[bank], float(value(layout().delayType[bank]))));
                const int mixId = layout().digital[bank] + 2;
                bevel(dc, x, 90, 494, 70, bank == view.section ? RGB(27,65,83) : RGB(18,39,56));
                text(dc, x + 10, 96, 150, bank ? "DELAY B" : "DELAY A", 19,
                     bank ? RGB(191,150,255) : RGB(83,224,239));
                const char *engines[]{"DIGITAL", "MX COLLECTION", "MEMORY", "REEL", "OIL DRUM"};
                text(dc, x + 165, 98, 315, engines[std::clamp(type,0,4)], 14, RGB(220,241,247));
                fill(dc, x + 18, 137, 455, 5, RGB(8,22,34));
                fill(dc, x + 18, 137, int(455 * value(mixId)), 5,
                     bank ? RGB(176,124,255) : RGB(60,213,231));
                char mixLabel[32];
                snprintf(mixLabel,sizeof(mixLabel),"MIX %d%%",int(std::round(value(mixId)*100)));
                text(dc, x + 337, 112, 140, mixLabel, 12, RGB(183,215,225));
            }
        }
        auto sections = sectionNames(view.block);
        for (size_t i = 0; i < sections.size(); ++i) {
            int x = 248 + int(i) * 200;
            fill(dc, x, 177, 192, 36, int(i) == view.section ? RGB(32,106,125) : RGB(20,42,59));
            text(dc, x, 184, 192, sections[i].c_str(), 16,
                 int(i) == view.section ? RGB(238, 252, 255) : RGB(157, 187, 202));
        }
        if (view.block == 2 && view.advanced) {
            fill(dc, 900, 177, 160, 36, RGB(28, 59, 77));
            text(dc, 900, 184, 160, "MX A / B", 15, RGB(221,244,250));
        }
        fill(dc, 1075, 177, 175, 36, RGB(28, 59, 77));
        text(dc, 1075, 184, 175, view.advanced ? "BASIC" : "SETTINGS", 15, RGB(221,244,250));
        fill(dc, 232, 229, 1028, 437, RGB(16,31,47));
        line(dc,232,229,1260,229,RGB(37,85,105));
        bevel(dc, 232, 674, 1024, 112, RGB(15,33,49));
        if (view.block == 2 && !globalSettings) {
            const char *labels[]{"MIDI LEARN / TIME", "MIDI LEARN / FEEDBACK", "MIDI LEARN / MIX"};
            for (int i = 0; i < 3; ++i) {
                bevel(dc, 250 + i * 250, 700, 230, 48, RGB(24, 58, 72));
                text(dc, 255 + i * 250, 713, 220, labels[i], 14, RGB(219, 244, 250));
            }
            text(dc, 250, 758, 740, "Same CC follows the selected DELAY engine", 12, RGB(158, 197, 211));
        }
        if (view.block == 2 && view.advanced && !globalSettings)
            text(dc,250,700,400,view.mxSlot == 0 ? "MX ENGINE A" : "MX ENGINE B",24,RGB(226,245,251));
        for (int x : {30,1248}) for(int y : {238,778}) {
            disk(dc,x,y,5,RGB(198,199,184));
            line(dc,x-3,y,x+3,y,RGB(50,51,46));
        }
        if (view.block == 1 && !globalSettings) {
            if (view.section == 1) {

                text(dc, 250, 695, 290, "ATTACK / DECAY", 27, RGB(226, 245, 251));
                fill(dc, 525, 704, 230, 36, RGB(55, 61, 62));
                text(dc, 525, 711, 230, "TRIGGER", 16, RGB(238, 225, 198));
            } else
                text(dc, 250, 694, 450, view.section == 0 ? "CLONE COLLECTION" : "ATOMIC CLUSTER", 27,
                     RGB(226,245,251));
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
            text(dc, 248, 755, 990,
                 "Circuit models use the original nonlinear solver and require much more CPU. Legacy models "
                 "are lighter for live use.",
                 14, RGB(232,221,190));
        if (globalSettings)
            text(dc, 248, 735, 990,
                 "The audio device and buffer are configured in your DAW. Effect latency is reported to the "
                 "host.",
                 15, RGB(232,221,190));
        if (view.block == 4 && !controller->fileStatus.empty())
            text(dc, 248, 755, 990, controller->fileStatus.c_str(), 15, RGB(222,245,250));
        char status[96];
        snprintf(status,sizeof(status),"GR %.1f dB / PDC %d smp",controller->meterGR.load(),controller->meterLatency.load());
        text(dc,1007,682,235,status,12,RGB(166,208,222));
        int contentEnd=controlBottom;
        for(const auto &c:controls) contentEnd=std::max(contentEnd,c.y+c.h+10);
        if(contentEnd>controlBottom) {
            fill(dc,1256,244,3,402,RGB(91,97,90));
            const int thumb=std::max(24,402*(controlBottom-232)/(contentEnd-232));
            fill(dc,1256,244+(402-thumb)*scroll/std::max(1,contentEnd-controlBottom),3,thumb,RGB(227,215,177));
        }
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 232, 232, 1260, controlBottom);
        for (auto &c : controls) {
            int y = c.y - scroll;
            if (y + c.h < 232 || y > controlBottom)
                continue;
            const auto &p = controlUI()[c.id].definition;
            const auto label = controlTitle(c.id);
            if (c.compact) {
                text(dc, c.x, y, c.w, label.c_str(), 13, RGB(174,205,218));
                if (!p.choices.empty() || (p.steps > 1 && p.steps <= 64))
                    continue;
                bevel(dc, c.x, y + 23, c.w, 29, RGB(26,51,68));
                if (view.block == 2 && p.steps == 0 && !isToggle(c.id)) {
                    fill(dc,c.x+9,y+39,c.w-18,4,RGB(8,23,36));
                    fill(dc,c.x+9,y+39,int((c.w-18)*value(c.id)),4,accent());
                    disk(dc,c.x+9+int((c.w-18)*value(c.id)),y+41,6,RGB(227,249,252));
                    text(dc,c.x+7,y+25,c.w-14,formatted(c.id).c_str(),13,RGB(229,247,250));
                    continue;
                }
                if (isToggle(c.id)) {
                    const bool on=p.key.ends_with(".bypass") ? value(c.id)<.5 : value(c.id)>=.5;
                    disk(dc,c.x+16,y+38,7,on?RGB(65,236,143):RGB(241,81,99));
                    bevel(dc,c.x+35,y+27,12,21,RGB(12,27,42));
                    bevel(dc,c.x+33,y+(on?26:37),16,10,RGB(163,202,216));
                }
                text(dc, c.x+50, y + 28, c.w-54,
                     p.key.ends_with(".bypass") ? (value(c.id)>=.5 ? "BYPASS / OFF" : "EFFECT ON") : isToggle(c.id) ? (value(c.id) >= .5 ? "ON" : "OFF") : formatted(c.id).c_str(), 14,
                     RGB(232,247,250));
                if (p.steps == 0 && !isToggle(c.id)) {
                    fill(dc, c.x, y + 51, c.w, 3, RGB(9, 23, 36));
                    fill(dc, c.x, y + 51, int(c.w * value(c.id)), 3, accent());
                }
            } else {
                const int cx = c.x + c.w / 2, cy = y + 53, r = 39;
                // Restrained layered knob with a clear cyan value indicator.
                disk(dc,cx+3,cy+5,r+3,RGB(8,16,27));
                disk(dc,cx,cy,r+2,RGB(34,76,93));
                for(int rr=r;rr>=r-7;--rr) {
                    int shade=69-(r-rr)*4;
                    disk(dc,cx-1,cy-1,rr,RGB(shade,shade+15,shade+25));
                }
                disk(dc,cx,cy,r-9,RGB(12,24,39));
                disk(dc,cx-2,cy-3,r-13,RGB(27,49,67));
                for(int i=0;i<32;++i) {
                    double a=i*6.283185307179586/32;
                    line(dc,cx+int(std::sin(a)*(r-11)),cy-int(std::cos(a)*(r-11)),
                            cx+int(std::sin(a)*(r-7)),cy-int(std::cos(a)*(r-7)),RGB(86,126,147));
                }
                for(int i=0;i<=10;++i) {
                    double a=(-135+i*27)*3.141592653589793/180;
                    line(dc,cx+int(std::sin(a)*(r+8)),cy-int(std::cos(a)*(r+8)),
                            cx+int(std::sin(a)*(r+12)),cy-int(std::cos(a)*(r+12)),RGB(61,113,133));
                }
                double angle = (-135 + 270 * value(c.id)) * 3.141592653589793 / 180;
                auto pen = CreatePen(PS_SOLID, 3, accent());
                auto op = SelectObject(dc, pen);
                MoveToEx(dc, cx + int(std::sin(angle) * 19), cy - int(std::cos(angle) * 19), nullptr);
                LineTo(dc, cx + int(std::sin(angle) * 32), cy - int(std::cos(angle) * 32));
                SelectObject(dc, op);
                DeleteObject(pen);
                text(dc, c.x, y + 96, c.w, label.c_str(), 14, RGB(216,238,245));
                text(dc, c.x, y + 114, c.w, formatted(c.id).c_str(), 13, accent());
            }
        }
        RestoreDC(dc, saved);
        const char *buttons[]{"LOAD IR WAV", "LOAD PRESET", "SAVE PRESET", "MASTER BYPASS"};
        buttons[3]=value(layout().bypass)>=.5 ? "MASTER BYPASSED" : "MASTER ACTIVE";
        for (int i = 0; i < 4; ++i) {
            fill(dc, 28 + i * 250, 805, 238, 32, RGB(26,53,72));
            text(dc, 28 + i * 250, 811, 238, buttons[i], 14, RGB(226,248,252));
        }
        text(dc, 1028, 807, 225, "Right click: MIDI Learn", 13, RGB(160, 171, 175));
        SetStretchBltMode(target, HALFTONE);
        StretchBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, W, H, SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
        if (!supplied) EndPaint(window, &ps);
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
        case WM_PRINTCLIENT:
            paint(reinterpret_cast<HDC>(w));
            return 0;
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
                                   std::max(1, controlUI()[id].definition.choices.empty() ?
                                       controlUI()[id].definition.steps :
                                       int(std::count(controlUI()[id].definition.choices.begin(),
                                                      controlUI()[id].definition.choices.end(), '|'))));
                    scroll = 0;
                    rebuild();
                }
            }
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            if (view.block == 2 && !globalSettings && y >= 90 && y < 160 && x >= 248 && x < 1248) {
                view.section = x >= 754 ? 1 : 0;
                scroll = 0;
                rebuild();
                return 0;
            }
            if (const int id = delayMidiTarget(x, y); id >= 0) {
                change(layout().midiLearn, double(id + 1) / layout().controlCount);
                return 0;
            }
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
            if (y >= 90 && y < 90 + 5 * 113 && x >= 20 && x < 222) {
                int at = std::clamp((y - 90) / 113, 0, 4);
                auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
                if ((y - 90) % 113 >= 60)
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
                else if (x >= 248 && size_t((x - 248) / 200) < sections.size())
                    view.section = (x - 248) / 200;
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
                if (controlUI()[id].definition.steps > 1 && controlUI()[id].definition.steps <= 64)
                    return 0;
                drag = id;
                startX = x;
                startY = y;
                startValue = value(id);
                SetCapture(window);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (drag >= 0) {
                const auto &key = layout().parameters[drag].key;
                const bool delaySlider = view.block == 2 &&
                    (key == "delayA.mix" || key == "delayB.mix" ||
                     key == "delayA.time" || key == "delayB.time" ||
                     key == "delayA.feedback" || key == "delayB.feedback");
                change(drag, startValue + (delaySlider ? (x - startX) / 170.0 :
                    (startY - y) * ((w & MK_SHIFT) ? .0004 : .004)));
            }
            return 0;
        case WM_LBUTTONUP:
            if (reorder >= 0) {
                int dest = std::clamp((y - 90) / 113, 0, 4);
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
            int id = delayMidiTarget(x, y);
            if (id < 0) id = hit(x, y);
            if (view.block == 1 && y >= 695 && y <= 785) {
                const int center = view.section == 1 ? 1080 : W / 2;
                if (std::abs(x - center) < 60)
                    id = effectBypass();
            }
            if (y >= 90 && y < 90 + 5 * 113 && x >= 20 && x < 222 && (y - 90) % 113 >= 60) {
                auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
                id=layout().blockBypass[order[std::clamp((y-90)/113,0,4)]];
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
                double step = controlUI()[id].definition.steps ? 1. / controlUI()[id].definition.steps : .001;
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
