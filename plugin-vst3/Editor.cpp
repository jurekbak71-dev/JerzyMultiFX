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
    COLORREF accent() const {
        return view.block == 1 ? (view.section == 1   ? RGB(255, 139, 52)
                                  : view.section == 2 ? RGB(220, 197, 91)
                                                      : RGB(190, 129, 240))
                               : RGB(90, 199, 207);
    }
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
            (common || p.steps > 0 || p.key == "atomic.stereo" ? compact : knobs).push_back(id);
        }
        int y = 236;
        for (size_t i = 0; i < compact.size(); ++i) {
            const int x = 28 + int(i % 6) * 205, cy = y + int(i / 6) * 62;
            controls.push_back({compact[i], x, cy, 194, 54, true});
        }
        y += int((compact.size() + 5) / 6) * 62 + 15;
        const int columns = view.block == 2                        ? 6
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
            if (yy < 232 || yy + 55 > 790)
                continue;
            HWND h = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                                   int(c.x / sx), int((yy + 23) / sy), int(c.w / sx), 240, window,
                                   reinterpret_cast<HMENU>(INT_PTR(10000 + c.id)), instance, nullptr);
            if (!h)
                continue;
            SendMessage(h, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
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
        fill(dc, 0, 0, W, H, RGB(15, 20, 24));
        text(dc, 24, 18, 400, "JERZY MULTIFX / PREVIEW 2", 25, RGB(236, 236, 226));
        char meter[150];
        snprintf(meter, sizeof(meter), "IN %.1f dB  |  OUT %.1f dB  |  GR %.1f dB  |  LATENCY %d samples",
                 20 * std::log10(std::max(controller->meterIn.load(), 1.e-6f)),
                 20 * std::log10(std::max(controller->meterOut.load(), 1.e-6f)), controller->meterGR.load(),
                 controller->meterLatency.load());
        text(dc, 440, 25, 810, meter, 15, accent());
        fill(dc, 1040, 49, 210, 25, RGB(35, 60, 65));
        text(dc, 1040, 52, 210, globalSettings ? "BACK TO EFFECT" : "SETTINGS / OUTPUT", 14, accent());
        auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
        for (int i = 0; i < 5; ++i) {
            int b = order[i], x = 20 + i * 248;
            fill(dc, x, 80, 238, 76, b == view.block ? RGB(43, 79, 86) : RGB(33, 43, 50));
            text(dc, x + 4, 92, 230, blockName(b), 15, RGB(238, 238, 225));
            const bool bypass = value(layout().blockBypass[b]) >= .5;
            fill(dc, x + 25, 122, 188, 25, bypass ? RGB(82, 48, 43) : RGB(35, 105, 84));
            text(dc, x + 25, 124, 188, bypass ? "BYPASS / OFF" : "ON", 14, RGB(242, 240, 220));
        }
        auto sections = sectionNames(view.block);
        for (size_t i = 0; i < sections.size(); ++i) {
            int x = 28 + int(i) * 230;
            fill(dc, x, 177, 218, 36, int(i) == view.section ? accent() : RGB(43, 47, 54));
            text(dc, x, 184, 218, sections[i].c_str(), 16,
                 int(i) == view.section ? RGB(15, 20, 24) : RGB(225, 225, 216));
        }
        if (view.block == 2 && view.advanced) {
            text(dc, 540, 218, 350, view.mxSlot == 0 ? "MX ENGINE A" : "MX ENGINE B", 15, accent());
            fill(dc, 900, 177, 160, 36, RGB(42, 57, 66));
            text(dc, 900, 184, 160, "MX A / B", 15, accent());
        }
        fill(dc, 1075, 177, 175, 36, RGB(42, 57, 66));
        text(dc, 1075, 184, 175, view.advanced ? "BASIC" : "SETTINGS", 15, accent());
        COLORREF panel = view.block == 1 ? (view.section == 1   ? RGB(221, 93, 9)
                                            : view.section == 2 ? RGB(36, 23, 66)
                                                                : RGB(30, 26, 39))
                                         : RGB(22, 33, 39);
        fill(dc, 20, 229, 1240, 562, panel);
        if (view.block == 1 && !globalSettings && !view.advanced) {
            if (view.section == 1) {
                fill(dc, 28, 620, 1224, 170, RGB(22, 25, 25));
                text(dc, 320, 630, 640, "ATTACK / DECAY", 44, RGB(232, 231, 220));
                fill(dc, 525, 704, 230, 36, RGB(55, 61, 62));
                text(dc, 525, 711, 230, "TRIGGER", 16, RGB(238, 225, 198));
            } else
                text(dc, 340, 658, 600, view.section == 0 ? "CLONE COLLECTION" : "ATOMIC CLUSTER", 29,
                     accent());
        }
        if (effectBypass() >= 0 && !globalSettings) {
            const int cx = view.section == 1 ? 1080 : W / 2, cy = 735;
            HBRUSH b = CreateSolidBrush(RGB(173, 181, 176));
            auto oldBrush = SelectObject(dc, b);
            Ellipse(dc, cx - 27, cy - 27, cx + 27, cy + 27);
            SelectObject(dc, oldBrush);
            DeleteObject(b);
            text(dc, cx - 110, 771, 220, value(effectBypass()) >= .5 ? "BYPASS / OFF" : "EFFECT ON", 13,
                 accent());
            HBRUSH led = CreateSolidBrush(value(effectBypass()) >= .5 ? RGB(94, 48, 40) : accent());
            oldBrush = SelectObject(dc, led);
            Ellipse(dc, cx - 5, cy - 42, cx + 5, cy - 32);
            SelectObject(dc, oldBrush);
            DeleteObject(led);
        }
        if (view.block == 0 && view.section == 2 && value(layout().fuzzModel) * 19 >= 5.5)
            text(dc, 28, 755, 1220,
                 "Circuit models use the original nonlinear solver and require much more CPU. Legacy models "
                 "are lighter for live use.",
                 14, accent());
        if (globalSettings)
            text(dc, 28, 735, 1220,
                 "The audio device and buffer are configured in your DAW. Effect latency is reported to the "
                 "host.",
                 15, accent());
        if (view.block == 4 && !controller->fileStatus.empty())
            text(dc, 28, 755, 1220, controller->fileStatus.c_str(), 15, accent());
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 20, 232, 1260, 790);
        for (auto &c : controls) {
            int y = c.y - scroll;
            if (y + c.h < 232 || y > 790)
                continue;
            const auto &p = layout().parameters[c.id];
            const auto label = controlTitle(c.id);
            if (c.compact) {
                text(dc, c.x, y, c.w, label.c_str(), 13, RGB(218, 219, 210));
                if (!p.choices.empty())
                    continue;
                fill(dc, c.x, y + 23, c.w, 29,
                     value(c.id) >= .5 && p.steps == 1 ? RGB(37, 109, 87) : RGB(46, 57, 64));
                text(dc, c.x, y + 28, c.w,
                     p.steps == 1 ? (value(c.id) >= .5 ? "ON" : "OFF") : formatted(c.id).c_str(), 14,
                     accent());
                if (p.steps == 0) {
                    fill(dc, c.x, y + 51, c.w, 3, RGB(16, 27, 32));
                    fill(dc, c.x, y + 51, int(c.w * value(c.id)), 3, accent());
                }
            } else {
                const int cx = c.x + c.w / 2, cy = y + 53, r = 39;
                HBRUSH brush = CreateSolidBrush(RGB(40, 45, 49));
                HPEN pen = CreatePen(PS_SOLID, 3, RGB(101, 108, 108));
                auto ob = SelectObject(dc, brush);
                auto op = SelectObject(dc, pen);
                Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
                SelectObject(dc, op);
                SelectObject(dc, ob);
                DeleteObject(brush);
                DeleteObject(pen);
                double angle = (-135 + 270 * value(c.id)) * 3.141592653589793 / 180;
                pen = CreatePen(PS_SOLID, 4, accent());
                op = SelectObject(dc, pen);
                MoveToEx(dc, cx + int(std::sin(angle) * 19), cy - int(std::cos(angle) * 19), nullptr);
                LineTo(dc, cx + int(std::sin(angle) * 32), cy - int(std::cos(angle) * 32));
                SelectObject(dc, op);
                DeleteObject(pen);
                text(dc, c.x, y + 96, c.w, label.c_str(), 14, RGB(240, 238, 222));
                text(dc, c.x, y + 114, c.w, formatted(c.id).c_str(), 13, accent());
            }
        }
        RestoreDC(dc, saved);
        const char *buttons[]{"LOAD IR WAV", "LOAD PRESET", "SAVE PRESET", "MASTER BYPASS"};
        for (int i = 0; i < 4; ++i) {
            fill(dc, 28 + i * 250, 805, 238, 32, RGB(39, 57, 65));
            text(dc, 28 + i * 250, 811, 238, buttons[i], 14, accent());
        }
        text(dc, 1028, 807, 225, "Scroll / Shift: fine", 13, RGB(160, 171, 175));
        SetStretchBltMode(target, HALFTONE);
        StretchBlt(target, 0, 0, client.right, client.bottom, dc, 0, 0, W, H, SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(window, &ps);
    }
    int hit(int x, int y) const {
        for (auto &c : controls)
            if (x >= c.x && x < c.x + c.w && y + scroll >= c.y && y + scroll < c.y + c.h)
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
        case WM_TIMER:
            if (w == 2) {
                KillTimer(window, 2);
                change(layout().attackTrigger, 0);
                return 0;
            }
            if (drag < 0 && selectionSignature() != signature)
                rebuild();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
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
            if (y >= 49 && y < 76 && x >= 1040) {
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
            if (y < 232 || y > 790)
                return 0;
            int id = hit(x, y);
            if (id >= 0) {
                if (layout().parameters[id].steps == 1) {
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
                                    std::max(0, bottom - 780));
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
        SetTimer(window, 1, 100, nullptr);
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
