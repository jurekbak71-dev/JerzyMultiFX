#include "Plugin.h"
#include "core/DelayMidiRoutes.h"
#include "core/Presentation.h"
#include "core/Presets.h"
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
    PanelSelection view{2, 0, 0, false};
    int drag = -1, reorder = -1, startX = 0, startY = 0, scroll = 0;
    int controlPage = 0, controlPages = 1;
    bool reorderHorizontal = false;
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
    bool referenceDelay() const { return view.block == 2 && !view.advanced && !globalSettings; }
    int delayControl(int bank, int kind) const {
        static const DelayMidiRoutes routes;
        const State s = state();
        return routes.resolve(s, routes.canonical[bank][kind].first).first;
    }
    int delayFeature(int index) const {
        const int bank = std::clamp(view.section, 0, 1);
        const int type = std::clamp(int(plain(layout().delayType[bank], float(value(layout().delayType[bank])))), 0, 4);
        static constexpr const char *keys[5][8] = {
            {"filter", "level", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
            {"mx.inputTrim", "mx.outputTrim", "mx.machineCondition", "mx.slot0_pan",
             "mx.fx0_amount", "mx.fx0_rate", "mx.fx0_depth", "mx.slot0_duck"},
            {"memory.boost", "memory.squelch", "memory.depth", "memory.modRate",
             "memory.level", "memory.blend", nullptr, nullptr},
            {"reel.wow", "reel.flutter", "reel.age", "reel.motor",
             "reel.saturation", "reel.bias", "reel.width", "reel.hiss"},
            {"oil.intone", "oil.outtone", "oil.depth", "oil.speed",
             "oil.insat", "oil.outsat", "oil.width", "oil.drift"}
        };
        if (index < 0 || index >= 8 || !keys[type][index]) return -1;
        return parameterId(std::string(bank ? "delayB." : "delayA.") + keys[type][index]);
    }
    std::array<HFONT, 64> fonts{};
    int effectBypass() const {
        if (view.block != 1)
            return -1;
        return layout().processorBypass[view.section + 1];
    }
    int delayMidiTarget(int x, int y) const {
        if (referenceDelay() || globalSettings || view.block != 2 || y < 700 || y >= 748 || x < 250 || x >= 1000)
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
    static void metalPanel(HDC dc, int x, int y, int w, int h, bool raised = true) {
        fill(dc,x+2,y+3,w,h,RGB(2,8,14));
        for(int i=0;i<10;++i){
            const int top=(h*i)/10, height=(h*(i+1))/10-top;
            const int shade=raised ? 31-i*2 : 13+i;
            fill(dc,x,y+top,w,height,RGB(shade,shade+9,shade+16));
        }
        for(int i=0;i<h;i+=5)
            line(dc,x+3,y+i,x+w-4,y+i,RGB(37,48,55));
        line(dc,x,y,x+w-1,y,raised?RGB(118,134,141):RGB(53,71,82));
        line(dc,x,y,x,y+h-1,RGB(80,99,108));
        line(dc,x,y+h-1,x+w,y+h-1,RGB(3,9,15),2);
        line(dc,x+w-1,y,x+w-1,y+h,RGB(3,9,15),2);
    }
    static void screw(HDC dc,int x,int y){
        disk(dc,x+1,y+2,5,RGB(3,9,15));
        disk(dc,x,y,4,RGB(113,126,131));
        disk(dc,x,y,3,RGB(46,57,63));
        line(dc,x-2,y+1,x+2,y-1,RGB(157,169,171));
    }
    static void lamp(HDC dc,int x,int y,bool on,COLORREF color){
        disk(dc,x,y,11,RGB(2,8,13));
        disk(dc,x,y,9,RGB(69,85,91));
        disk(dc,x,y,7,on?color:RGB(80,43,47));
        if(on){disk(dc,x-2,y-2,3,RGB(224,253,248));disk(dc,x,y,2,RGB(255,255,255));}
        else disk(dc,x-2,y-2,2,RGB(133,91,93));
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

    void levelBar(HDC dc, int x, const char *name, float amplitude) {
        text(dc, x - 8, 98, 42, name, 12, RGB(236,248,250));
        const float dbfs = 20.f * std::log10(std::max(amplitude, 1.e-6f));
        const int lit = std::clamp(int((dbfs + 60) / 3), 0, 20);
        for (int i = 0; i < 20; ++i) {
            const int y = 265 - i * 7;
            fill(dc, x, y, 13, 5, i < lit ? (i > 16 ? RGB(247,119,79) : RGB(65,229,148))
                                         : RGB(27,52,65));
        }
        char reading[28];
        snprintf(reading, sizeof(reading), "%.1f", dbfs);
        text(dc, x - 10, 276, 48, reading, 10, RGB(184,211,221));
    }
    void cardArt(HDC dc, int block, int x, int y) {
        metalPanel(dc, x + 10, y + 48, 163, 85, false);
        const int cx = x + 92, cy = y + 90;
        if (block == 0) {
            metalPanel(dc, x + 20, y + 56, 142, 70);
            fill(dc,x+25,y+60,132,28,RGB(4,10,13));
            for (int row = 0; row < 14; ++row)
                line(dc,x+27,y+61+row*2,x+155,y+61+row*2,RGB(66,80,85));
            for (int col = 0; col < 26; ++col)
                line(dc,x+27+col*5,y+61,x+27+col*5,y+87,RGB(29,41,46));
            for (int i = 0; i < 3; ++i) {
                const int knob=x+51+i*40;
                disk(dc,knob+1,y+108,11,RGB(2,7,11));
                disk(dc,knob,y+106,10,RGB(135,145,149));
                disk(dc,knob,y+106,8,RGB(15,24,31));
                line(dc,knob,y+100,knob+2,y+96,RGB(229,235,235),2);
            }
            screw(dc,x+24,y+59);screw(dc,x+158,y+59);
        } else if (block == 1) {
            int lastX = x + 16, lastY = cy;
            for (int i = 1; i <= 138; ++i) {
                const int xx = x + 16 + i;
                const int yy = cy - int(24 * std::sin(i*.095) * (.65 + .35*std::sin(i*.024)));
                line(dc,lastX,lastY+2,xx,yy+2,RGB(79,49,123),4);
                line(dc,lastX,lastY,xx,yy,RGB(190,145,251),2); lastX=xx;lastY=yy;
            }
        } else if (block == 2) {
            for (int i = 0; i < 4; ++i) {
                const int xx = x + 43 + i*30;
                for (int a = 0; a < 35; ++a) {
                    const float angle = a*6.2831853f/35;
                    const int px = xx + int(std::cos(angle)*(11+i*2));
                    const int py = cy + int(std::sin(angle)*27);
                    disk(dc,px,py,1,i%2?RGB(31,123,210):RGB(47,217,238));
                }
            }
            line(dc,x+34,cy+2,x+155,cy+2,RGB(11,93,119),4);
            line(dc,x+34,cy,x+155,cy,RGB(67,211,238),2);
        } else if (block == 3) {
            for (int i = 0; i < 4; ++i) {
                int lastX=x+13,lastY=cy;
                for(int j=1;j<150;++j) {
                    const int xx=x+13+j;
                    const int yy=cy-int(9*std::sin(j*.045+i*.65)+12*std::sin(j*.013+i*.9));
                    line(dc,lastX,lastY,xx,yy,i%2?RGB(65,117,215):RGB(149,106,238));
                    lastX=xx;lastY=yy;
                }
            }
        } else {
            metalPanel(dc,x+24,y+54,136,72);
            fill(dc,x+28,y+58,128,63,RGB(8,16,23));
            for(int row=0;row<21;++row)
                line(dc,x+29,y+59+row*3,x+154,y+59+row*3,RGB(44,55,62));
            for(int col=0;col<32;++col)
                line(dc,x+29+col*4,y+59,x+29+col*4,y+119,RGB(26,38,44));
            disk(dc,cx,cy,26,RGB(4,9,14));disk(dc,cx,cy,22,RGB(62,73,80));
            disk(dc,cx,cy,19,RGB(8,16,24));disk(dc,cx,cy,9,RGB(33,47,56));
            line(dc,x+125,y+84,x+161,y+83,RGB(4,8,12),7);
            disk(dc,x+129,y+84,9,RGB(9,16,22));
        }
    }
    void chrome(HDC dc) {
        fill(dc,0,0,W,H,RGB(7,17,27));
        metalPanel(dc,0,0,W,82);
        fill(dc,0,0,W,78,RGB(8,19,29));
        line(dc,0,81,W,81,RGB(38,68,83));
        text(dc,39,11,155,"JERZY",28,RGB(224,248,252));
        text(dc,181,11,210,"MULTI FX",28,RGB(46,196,235));
        text(dc,25,47,360,"BASS GUITAR EFFECTS STUDIO",12,RGB(124,172,193));
        text(dc,395,8,100,"PRESET",12,RGB(199,221,232));
        bevel(dc,397,36,338,39,RGB(15,29,43));
        text(dc,405,43,18,"<",20,RGB(187,219,230));
        text(dc,706,43,18,">",20,RGB(187,219,230));
        const int preset=std::clamp(int(std::round(value(layout().factoryProgram)*7)),0,7);
        char presetLabel[96];
        snprintf(presetLabel,sizeof(presetLabel),"%02d   %s",preset+1,presetNames[preset]);
        text(dc,435,45,245,presetLabel,14,RGB(223,242,248));
        text(dc,678,45,22,"v",13,RGB(174,210,225));
        if(referenceDelay()){
        }
        bevel(dc,744,36,40,39,RGB(15,29,43));
        text(dc,746,43,37,"SAVE",11,RGB(194,226,237));
        text(dc,799,8,180,"MIDI MAPOWANIE",12,RGB(198,220,231));
        bevel(dc,797,36,144,39,RGB(8,30,39));
        bool mapped=false;
        for (int i=0;i<128;++i) if (value(layout().midiMap+i)>0) {mapped=true;break;}
        disk(dc,814,55,5,mapped?RGB(55,226,145):RGB(65,198,219));
        text(dc,823,44,111,mapped?"AKTYWNE":"MIDI LEARN",12,RGB(204,241,246));
        bevel(dc,962,36,155,39,RGB(14,32,47));
        text(dc,965,44,149,globalSettings||view.advanced?"WRÓĆ":"USTAWIENIA",12,RGB(204,231,240));
        bevel(dc,1126,36,127,39,RGB(14,32,47));
        text(dc,1128,44,123,"POMOC",12,RGB(204,231,240));
        metalPanel(dc,10,88,1260,210,false);
        fill(dc,13,91,1254,204,RGB(7,18,28));
        auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
        for(int i=0;i<5;++i) {
            const int b=order[i],x=24+i*222;
            const bool selected=b==view.block;
            metalPanel(dc,x,97,183,190);
            fill(dc,x+3,100,177,184,selected?RGB(22,43,55):RGB(16,30,40));
            if(selected) {
                line(dc,x-2,95,x+185,95,RGB(15,95,112),2);
                line(dc,x-2,95,x-2,289,RGB(15,95,112),2);
                line(dc,x+185,95,x+185,289,RGB(15,95,112),2);
                line(dc,x,97,x+183,97,RGB(49,224,240),3);
                line(dc,x,97,x,287,RGB(49,224,240),3);
                line(dc,x+182,97,x+182,287,RGB(49,224,240),3);
                line(dc,x+3,101,x+179,101,RGB(28,141,164));
            }
            screw(dc,x+8,274);screw(dc,x+173,274);
            char number[8];snprintf(number,sizeof(number),"%d",i+1);
            bevel(dc,x+9,108,25,27,RGB(30,48,64));
            text(dc,x+9,110,25,number,17,selected?RGB(61,227,244):RGB(226,240,245));
            const char* labels[]{"PREAMP / DRIVE","MODULACJA","DELAY A + B","REVERB","CAB / IR"};
            text(dc,x+39,112,139,labels[b],13,RGB(231,246,250));
            cardArt(dc,b,x,97);
            const bool off=value(layout().blockBypass[b])>=.5;
            metalPanel(dc,x+48,250,89,29);
            fill(dc,x+51,253,83,23,off?RGB(50,21,30):RGB(6,34,29));
            lamp(dc,x+111,264,true,off?RGB(232,57,75):RGB(43,233,135));
            text(dc,x+50,254,48,off?"OFF":"ON",15,off?RGB(244,130,141):RGB(78,244,162));
            if(i<4){line(dc,x+188,188,x+215,188,RGB(43,207,232),2);line(dc,x+207,183,x+215,188,RGB(43,207,232),2);line(dc,x+207,193,x+215,188,RGB(43,207,232),2);}
        }
        metalPanel(dc,1135,97,126,190);
        fill(dc,1138,100,120,184,RGB(8,19,28));
        levelBar(dc,1152,"IN",needleIn);
        levelBar(dc,1208,"OUT",needleOut);
    }
    void referenceKnob(HDC dc, const Control &c, const char *label, int radius, COLORREF color) {
        const int cx=c.x+c.w/2,cy=c.y+(radius==40?71:radius>=34?63:56);
        text(dc,c.x,c.y,c.w,label,radius==40?13:11,RGB(216,236,242));
        disk(dc,cx+3,cy+5,radius+7,RGB(2,8,13));
        disk(dc,cx,cy,radius+5,RGB(110,122,128));
        disk(dc,cx,cy,radius+2,RGB(25,37,46));
        for(int i=0;i<28;++i){
            const double a=i*6.283185307179586/28;
            line(dc,cx+int(std::sin(a)*(radius-1)),cy-int(std::cos(a)*(radius-1)),
                    cx+int(std::sin(a)*(radius+3)),cy-int(std::cos(a)*(radius+3)),
                    i<14?RGB(117,130,136):RGB(40,52,61));
        }
        disk(dc,cx,cy,radius-4,RGB(6,14,21));
        for(int step=0;step<8;++step){
            const int ring=radius-6-step*std::max(1,(radius-13)/8);
            const int shade=33+step*3;
            disk(dc,cx-1-step/3,cy-2-step/4,ring,RGB(shade,shade+10,shade+17));
        }
        const int face=radius-14;
        line(dc,cx-face+3,cy-face/2,cx-4,cy-face/2-3,RGB(89,105,114));
        line(dc,cx-face+5,cy-face/2+2,cx-7,cy-face/2,RGB(66,83,93));
        line(dc,cx+4,cy+face/2,cx+face-3,cy+face/2+3,RGB(29,43,53));
        for(int i=0;i<10;++i){
            const double a=(-135+i*27)*3.141592653589793/180;
            line(dc,cx+int(std::sin(a)*(radius+3)),cy-int(std::cos(a)*(radius+3)),
                    cx+int(std::sin(a)*(radius+7)),cy-int(std::cos(a)*(radius+7)),
                    i<=int(value(c.id)*9)?color:RGB(62,86,102),2);
        }
        const double a=(-135+270*value(c.id))*3.141592653589793/180;
        line(dc,cx+int(std::sin(a)*(radius-22)),cy-int(std::cos(a)*(radius-22)),
                cx+int(std::sin(a)*(radius-7)),cy-int(std::cos(a)*(radius-7)),RGB(5,12,17),5);
        line(dc,cx+int(std::sin(a)*(radius-22)),cy-int(std::cos(a)*(radius-22)),
                cx+int(std::sin(a)*(radius-7)),cy-int(std::cos(a)*(radius-7)),RGB(233,247,250),3);
        std::string reading=formatted(c.id);
        const auto &definition=layout().parameters[c.id];
        if(radius==40){
            char display[32];
            if(std::string_view(label)=="CZAS DELAYA")
                snprintf(display,sizeof(display),"%.0f ms",plain(c.id,float(value(c.id))));
            else
                snprintf(display,sizeof(display),"%.0f %%",100*plain(c.id,float(value(c.id))));
            reading=display;
        }
        if(definition.unit=="ms" && reading.find("ms")==std::string::npos)reading+=" ms";
        text(dc,c.x,c.y+(radius==40?121:radius>=34?109:87),c.w,reading.c_str(),radius>=34?13:11,
             RGB(228,247,251));
    }
    void referenceDelayPanel(HDC dc) {
        metalPanel(dc,11,305,1258,529);
        fill(dc,14,308,1252,522,RGB(11,25,36));
        screw(dc,21,315);screw(dc,1259,315);
        screw(dc,21,824);screw(dc,1259,824);
        text(dc,29,318,285,"DELAY A + B",27,RGB(230,250,252));
        text(dc,321,327,390,"DWA NIEZALEŻNE SILNIKI OPÓŹNIENIA",11,RGB(132,178,195));
        const int bank=std::clamp(view.section,0,1);
        const int mix=layout().digital[bank]+2;
        text(dc,797,326,61,"MIX",14,RGB(225,245,249));
        metalPanel(dc,850,333,344,14,false);
        fill(dc,852,337,340,6,RGB(2,9,15));
        fill(dc,852,337,int(340*value(mix)),6,RGB(43,210,235));
        disk(dc,852+int(340*value(mix))+1,343,11,RGB(2,9,15));
        disk(dc,852+int(340*value(mix)),341,9,RGB(154,171,179));
        disk(dc,852+int(340*value(mix))-2,339,5,RGB(235,244,246));
        char mixText[24];snprintf(mixText,sizeof(mixText),"%d%%",int(std::round(value(mix)*100)));
        text(dc,1196,324,59,mixText,16,RGB(232,248,250));
        metalPanel(dc,26,365,236,416,false);
        fill(dc,29,368,230,410,RGB(8,21,32));
        text(dc,36,374,212,"SILNIK DELAYA",14,RGB(227,246,249));
        const int type=int(plain(layout().delayType[bank],float(value(layout().delayType[bank]))));
        const int model=int(std::round(value(layout().mxAlgorithms[bank][0])*8));
        const char* names[]{"OIL DRUM","MX ANALOG","REEL / TAPE","DIGITAL","MEMORY","VOLANTE","OLIVERA","BRIG / BBD"};
        const int families[]{4,1,3,0,2,1,1,1};
        const int models[]{-1,-1,-1,-1,-1,0,2,4};
        for(int i=0;i<8;++i){
            const int y=402+i*44;
            const bool selected=type==families[i] && (models[i]<0 || model==models[i]) &&
                (type!=1 || i!=1 || (model!=0&&model!=2&&model!=4));
            metalPanel(dc,35,y,218,39,false);
            fill(dc,38,y+3,212,33,selected?RGB(12,78,96):RGB(10,26,39));
            if(selected)fill(dc,35,y,3,39,RGB(57,222,240));
            disk(dc,227,y+19,5,selected?RGB(64,229,238):RGB(37,57,72));
            if(i==0){
                line(dc,47,y+11,47,y+26,RGB(126,225,234),2);
                line(dc,47,y+11,58,y+11,RGB(126,225,234),2);
                line(dc,47,y+26,58,y+26,RGB(126,225,234),2);
                line(dc,58,y+11,58,y+26,RGB(126,225,234),2);
            } else {
                disk(dc,53,y+19,6,selected?RGB(78,214,231):RGB(89,121,140));
                disk(dc,53,y+19,3,RGB(10,26,39));
            }
            text(dc,64,y+10,150,names[i],13,selected?RGB(91,239,249):RGB(184,209,223));
        }
        metalPanel(dc,274,365,990,440,false);
        fill(dc,277,368,984,434,RGB(9,22,34));
        const char* heading[]{"DIGITAL","MX ANALOG","MEMORY","REEL ECHO","OIL DRUM"};
        disk(dc,307,398,18,RGB(4,14,23));
        disk(dc,307,398,15,RGB(38,203,226));
        disk(dc,307,398,11,RGB(5,25,36));
        line(dc,305,390,311,390,RGB(203,252,255),2);
        text(dc,327,379,385,heading[std::clamp(type,0,4)],27,RGB(231,249,252));
        const char* subtitles[]{"PRECYZYJNE POWTÓRZENIA","MODUŁOWE ECHA ANALOGOWE","BARWNE ECHA CYFROWE","MECHANIKA TAŚMY I GŁOWIC","CIEPŁE, ORGANICZNE ECHA"};
        text(dc,289,419,580,subtitles[std::clamp(type,0,4)],11,RGB(139,178,194));
        text(dc,735,386,316,bank?"AKTYWNY BANK B":"AKTYWNY BANK A",13,RGB(132,202,216));
        const bool bypass=value(layout().slotBypass[bank])>=.5;
        bevel(dc,928,413,124,31,bypass?RGB(86,36,48):RGB(18,76,58));
        text(dc,932,420,116,bypass?"BYPASS / OFF":"ON / AKTYWNY",11,RGB(226,247,244));
        line(dc,281,455,1061,455,RGB(31,68,88));
        text(dc,281,489,33,"A",19,bank==0?RGB(56,220,239):RGB(132,173,195));
        text(dc,611,489,33,"B",19,bank==1?RGB(177,131,247):RGB(132,173,195));
        for(size_t i=0;i<controls.size();++i){
            const auto &c=controls[i];
            if(c.y==465) referenceKnob(dc,c,(i%2)?"FEEDBACK":"CZAS DELAYA",40,i<2?RGB(53,215,236):RGB(174,128,247));
        }
        line(dc,281,648,1061,648,RGB(31,68,88));
        const char* groups[]{"FILTRACJA","MODULACJA","SATURACJA","STEREO / MECHANIKA"};
        for(int group=0;group<4;++group){
            const int gx=279+group*198;
            metalPanel(dc,gx,652,193,145,false);
            fill(dc,gx+3,655,187,139,RGB(11,25,37));
            text(dc,gx+5,658,183,groups[group],12,RGB(202,222,231));
        }
        const char* labels[5][8]={{"FILTER","LEVEL"},{"IN TRIM","OUT TRIM","MACHINE","PAN","FX AMOUNT","FX RATE","FX DEPTH","DUCK"},
          {"BOOST","SQUELCH","DEPTH","MOD RATE","LEVEL","BLEND"},
          {"WOW","FLUTTER","AGE","MOTOR","SATURATION","BIAS","WIDTH","HISS"},
          {"IN TONE","OUT TONE","DEPTH","SPEED","IN SAT","OUT SAT","WIDTH","DRIFT"}};
        for(const auto &c:controls) if(c.y==689) {
            for(int i=0;i<8;++i)if(delayFeature(i)==c.id) {
                referenceKnob(dc,c,labels[std::clamp(type,0,4)][i],26,i<2?RGB(64,212,231):RGB(173,126,236));break;
            }
        }
        metalPanel(dc,1074,459,181,186,false);
        fill(dc,1077,462,175,180,RGB(11,25,38));
        text(dc,1080,467,169,"TRYB PRACY",13,RGB(217,236,243));
        const char* routes[]{"SZEREGOWY A > B","RÓWNOLEGŁY","MIX A / B"};
        const int routing=int(plain(layout().delayMode,float(value(layout().delayMode))));
        for(int i=0;i<3;++i){
            const int y=493+i*45;
            metalPanel(dc,1082,y,165,39,false);
            fill(dc,1085,y+3,159,33,routing==i?RGB(23,124,153):RGB(15,36,52));
            lamp(dc,1096,y+19,routing==i,RGB(75,231,246));
            text(dc,1104,y+10,137,routes[i],11,RGB(223,240,246));
        }
        text(dc,286,808,676,"Prawy klik na regulatorze: MIDI Learn. Wszystkie parametry: USTAWIENIA.",10,RGB(123,171,191));
    }

    void rebuild() {
        if (!window)
            return;
        destroyCombos();
        controls.clear();
        if (referenceDelay()) {
            controlPage=0;controlPages=1;
            for (int bank = 0; bank < 2; ++bank)
                for (int kind = 0; kind < 2; ++kind) {
                    const int id = delayControl(bank, kind);
                    if (id >= 0) controls.push_back({id, 278 + bank * 330 + kind * 157, 465, 145, 150, false});
                }
            for (int i = 0; i < 8; ++i) {
                const int id = delayFeature(i);
                if (id >= 0) controls.push_back({id, 279 + i * 99, 689, 94, 106, false});
            }
        } else {
            State s = state();
            auto ids = globalSettings ? std::vector<int>{layout().masterGain, layout().outputLimiter}
                                      : panelParameters(view, s);
            std::vector<int> compact, knobs;
            for (int id : ids) {
                if (id == effectBypass() && !globalSettings) continue;
                const auto &p = controlUI()[id].definition;
                const bool common = view.block == 2 &&
                    (p.key.starts_with("md.") || p.key == "delayA.type" || p.key == "delayB.type" ||
                     p.key == "delayA.mix" || p.key == "delayB.mix" || p.key.ends_with(".bypass") ||
                     (p.key.find(".mx.") != std::string::npos && p.key.find("_algo") == std::string::npos));
                (common || p.steps > 0 || !p.choices.empty() || isToggle(id) ||
                 p.key == "atomic.stereo" ? compact : knobs).push_back(id);
            }
            const int total=int(compact.size()+knobs.size());
            controlPages=std::max(1,(total+5)/6);
            controlPage=std::clamp(controlPage,0,controlPages-1);
            for(int index=controlPage*6;index<std::min(total,(controlPage+1)*6);++index){
                const bool isCompact=index<int(compact.size());
                const int id=isCompact?compact[index]:knobs[index-int(compact.size())];
                const int local=index-controlPage*6;
                controls.push_back({id,28+(local%3)*411,375+(local/3)*131,388,isCompact?54:126,isCompact});
            }
        }
        for (auto &c : controls) {
            const auto &p = controlUI()[c.id].definition;
            if (p.choices.empty() && !(p.steps > 1 && p.steps <= 64))
                continue;
            const int yy = c.y - scroll;
            if (yy < 370 || yy + 55 > controlBottom)
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
        HWND factory = referenceDelay() ? nullptr : CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, int(430 / sx),
                                     int(46 / sy), int(300 / sx), 260, window,
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
        const int pixelsWide=std::max(1L,client.right),pixelsHigh=std::max(1L,client.bottom);
        auto bmp = CreateCompatibleBitmap(target, pixelsWide, pixelsHigh);
        auto old = SelectObject(dc, bmp);
        const int mapped=SaveDC(dc);
        SetMapMode(dc,MM_ANISOTROPIC);
        SetWindowExtEx(dc,W,H,nullptr);
        SetViewportExtEx(dc,pixelsWide,pixelsHigh,nullptr);
        if (referenceDelay()) {
            chrome(dc);
            referenceDelayPanel(dc);
            RestoreDC(dc,mapped);
            BitBlt(target,0,0,pixelsWide,pixelsHigh,dc,0,0,SRCCOPY);
            SelectObject(dc, old);
            DeleteObject(bmp);
            DeleteDC(dc);
            if (!supplied) EndPaint(window, &ps);
            return;
        }
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
        chrome(dc);
        text(dc,24,297,1190,"KLIKNIJ BLOK U GÓRY → WYBIERZ EFEKT PONIŻEJ → USTAW PARAMETRY    •    PRAWY KLIK: MIDI LEARN",13,RGB(169,211,225));
        auto sections = sectionNames(view.block);
        for (size_t i = 0; i < sections.size(); ++i) {
            int x = 28 + int(i) * 214;
            fill(dc, x, 316, 205, 36, int(i) == view.section ? RGB(32,106,125) : RGB(20,42,59));
            text(dc, x, 323, 205, sections[i].c_str(), 16,
                 int(i) == view.section ? RGB(238, 252, 255) : RGB(157, 187, 202));
        }
        if (view.block == 2 && view.advanced) {
            fill(dc, 900, 316, 160, 36, RGB(28, 59, 77));
            text(dc, 900, 323, 160, "MX A / B", 15, RGB(221,244,250));
        }
        fill(dc, 1075, 316, 175, 36, RGB(28, 59, 77));
        text(dc, 1075, 323, 175, view.advanced ? "BASIC" : "USTAWIENIA", 15, RGB(221,244,250));
        fill(dc, 20, 364, 1240, 302, RGB(16,31,47));
        line(dc,20,364,1260,364,RGB(37,85,105));
        bevel(dc, 24, 674, 1232, 112, RGB(15,33,49));
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
        for (int x : {30,1248}) for(int y : {374,778}) {
            disk(dc,x,y,5,RGB(198,199,184));
            line(dc,x-3,y,x+3,y,RGB(50,51,46));
        }
        if (view.block == 1 && !globalSettings) {
            if (view.section == 1) {

                text(dc, 250, 695, 260, "ATTACK / DECAY", 27, RGB(226, 245, 251));
                fill(dc, 525, 704, 230, 36, RGB(55, 61, 62));
                text(dc, 525, 711, 230, "TRIGGER", 16, RGB(238, 225, 198));
            } else
                text(dc, 250, 694, 340, view.section == 0 ? "CLONE COLLECTION" : "ATOMIC CLUSTER", 27,
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
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 20, 370, 1260, controlBottom);
        for (auto &c : controls) {
            int y = c.y - scroll;
            if (y + c.h < 370 || y > controlBottom)
                continue;
            const auto &p = controlUI()[c.id].definition;
            const auto label = controlTitle(c.id);
            if (c.compact) {
                fill(dc,c.x+2,y+1,c.w-4,19,RGB(31,64,78));
                text(dc, c.x+3, y+2, c.w-6, label.c_str(), 13, RGB(227,243,248));
                if (!p.choices.empty() || (p.steps > 1 && p.steps <= 64)) {
                    bevel(dc,c.x,y+23,c.w,29,RGB(20,44,59));
                    text(dc,c.x+10,y+28,c.w-40,formatted(c.id).c_str(),14,RGB(229,247,250));
                    text(dc,c.x+c.w-32,y+27,22,"▼",13,accent());
                    continue;
                }
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
                fill(dc,c.x+2,y+1,c.w-4,20,RGB(31,64,78));
                referenceKnob(dc,c,label.c_str(),34,accent());
            }
        }
        RestoreDC(dc, saved);
        bevel(dc,1031,638,48,22,RGB(21,51,67));
        text(dc,1031,639,48,"<",14,controlPage>0?RGB(229,246,252):RGB(87,117,131));
        bevel(dc,1083,638,100,22,RGB(15,36,50));
        char pageLabel[32];snprintf(pageLabel,sizeof(pageLabel),"STRONA %d/%d",controlPage+1,controlPages);
        text(dc,1083,642,100,pageLabel,11,RGB(215,235,244));
        bevel(dc,1187,638,48,22,RGB(21,51,67));
        text(dc,1187,639,48,">",14,controlPage+1<controlPages?RGB(229,246,252):RGB(87,117,131));
        const char *buttons[]{"LOAD IR WAV", "LOAD PRESET", "SAVE PRESET", "MASTER BYPASS"};
        buttons[3]=value(layout().bypass)>=.5 ? "MASTER BYPASSED" : "MASTER ACTIVE";
        for (int i = 0; i < 4; ++i) {
            fill(dc, 28 + i * 250, 805, 238, 32, RGB(26,53,72));
            text(dc, 28 + i * 250, 811, 238, buttons[i], 14, RGB(226,248,252));
        }
        text(dc, 1028, 807, 225, "Right click: MIDI Learn", 13, RGB(160, 171, 175));
        RestoreDC(dc,mapped);
        BitBlt(target,0,0,pixelsWide,pixelsHigh,dc,0,0,SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
        if (!supplied) EndPaint(window, &ps);
    }
    int hit(int x, int y) const {
        if (referenceDelay()) {
            for (const auto &c:controls)
                if (x>=c.x && x<c.x+c.w && y>=c.y && y<c.y+c.h) return c.id;
            return -1;
        }
        for (auto &c : controls)
            if (y >= 370 && y < controlBottom && x >= c.x && x < c.x + c.w && y + scroll >= c.y && y + scroll < c.y + c.h)
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
                    scroll = 0;controlPage=0;
                    rebuild();
                }
            }
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            if (referenceDelay()) {
                const int bank=std::clamp(view.section,0,1);
                if(y>=36&&y<76){
                    if(x>=397&&x<430){change(layout().factoryProgram,std::max(0.0,value(layout().factoryProgram)-1./7));return 0;}
                    if(x>=704&&x<735){change(layout().factoryProgram,std::min(1.0,value(layout().factoryProgram)+1./7));return 0;}
                    if(x>=430&&x<704){
                        HMENU menu=CreatePopupMenu();
                        for(int i=0;i<8;++i)AppendMenuA(menu,MF_STRING,20000+i,presetNames[i]);
                        POINT point{int(430/sx),int(76/sy)};ClientToScreen(window,&point);
                        const int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,
                                                       point.x,point.y,0,window,nullptr);
                        DestroyMenu(menu);
                        if(choice>=20000&&choice<20008)change(layout().factoryProgram,double(choice-20000)/7);
                        return 0;
                    }
                    if(x>=744&&x<784){file("savePreset",true);return 0;}
                    if(x>=962&&x<1118){view.advanced=true;rebuild();return 0;}
                    if(x>=1126&&x<1253){MessageBoxA(window,"Choose a block, then adjust its controls. Right-click any control for MIDI Learn. Use SETTINGS for all parameters.","Jerzy MultiFX",MB_OK|MB_ICONINFORMATION);return 0;}
                }
                if(y>=97&&y<287&&x>=24&&x<1135){
                    const int at=std::clamp((x-24)/222,0,4);
                    const int local=x-(24+at*222);
                    if(local<183){
                        auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
                        if(y>=250){const int id=layout().blockBypass[order[at]];change(id,value(id)>=.5?0:1);}
                        else {reorder=at;reorderHorizontal=true;view.block=order[at];view.section=0;scroll=0;controlPage=0;rebuild();SetCapture(window);}
                        return 0;
                    }
                }
                if(y>=313&&y<360&&x>=852&&x<1193){
                    const int id=layout().digital[bank]+2;
                    change(id,double(x-852)/340);
                    drag=id;startX=x;startY=y;startValue=value(id);SetCapture(window);return 0;
                }
                if(y>=402&&y<754&&x>=35&&x<253){
                    const int choice=(y-402)/44;
                    if(choice>=0&&choice<8){
                        const int families[]{4,1,3,0,2,1,1,1};
                        const int models[]{-1,-1,-1,-1,-1,0,2,4};
                        change(layout().delayType[bank],double(families[choice])/4);
                        if(models[choice]>=0)change(layout().mxAlgorithms[bank][0],double(models[choice])/8);
                        if(value(layout().digital[bank]+2)<.01)change(layout().digital[bank]+2,.5);
                        rebuild();return 0;
                    }
                }
                if(y>=493&&y<622&&x>=1082&&x<1247){
                    const int route=(y-493)/45;
                    if(route<3)change(layout().delayMode,double(route)/2);
                    return 0;
                }
                if(y>=413&&y<444&&x>=928&&x<1052){
                    const int id=layout().slotBypass[bank];change(id,value(id)>=.5?0:1);return 0;
                }
                if(y>=377&&y<418&&x>=735&&x<1050){view.section=1-bank;rebuild();return 0;}
                const int id=hit(x,y);
                if(id>=0){drag=id;startX=x;startY=y;startValue=value(id);SetCapture(window);return 0;}
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
            if (y >= 36 && y < 76 && x >= 397 && x < 430) {
                change(layout().factoryProgram,std::max(0.0,value(layout().factoryProgram)-1./7));return 0;
            }
            if (y >= 36 && y < 76 && x >= 704 && x < 735) {
                change(layout().factoryProgram,std::min(1.0,value(layout().factoryProgram)+1./7));return 0;
            }
            if (y >= 36 && y < 76 && x >= 744 && x < 784) {file("savePreset",true);return 0;}
            if (y >= 36 && y < 76 && x >= 962 && x < 1118) {
                if(view.block==2)view.advanced=!view.advanced;
                else globalSettings=!globalSettings;
                scroll = 0;controlPage=0;
                rebuild();
                return 0;
            }
            if (y >= 97 && y < 287 && x >= 24 && x < 1135) {
                int at = std::clamp((x - 24) / 222, 0, 4);
                auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
                if (x - (24 + at * 222) >= 183) return 0;
                if (y >= 250)
                    change(layout().blockBypass[order[at]],
                           value(layout().blockBypass[order[at]]) >= .5 ? 0 : 1);
                else {
                    reorder = at;
                    reorderHorizontal = true;
                    view.block = order[at];
                    view.section = 0;
                    globalSettings = false;
                    scroll = 0;controlPage=0;
                    rebuild();
                    SetCapture(window);
                }
                return 0;
            }
            if (y >= 316 && y < 352) {
                auto sections = sectionNames(view.block);
                if (x >= 1075)
                    view.advanced = !view.advanced;
                else if (view.block == 2 && x >= 900)
                    view.mxSlot = 1 - view.mxSlot;
                else if (x >= 28 && size_t((x - 28) / 214) < sections.size())
                    view.section = (x - 28) / 214;
                scroll = 0;controlPage=0;
                rebuild();
                return 0;
            }
            if(y>=638&&y<661&&x>=1031&&x<1235){
                if(x<1079&&controlPage>0)--controlPage;
                if(x>=1187&&controlPage+1<controlPages)++controlPage;
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
            if (y < 370 || y > controlBottom)
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
                const bool delaySlider = (referenceDelay() &&
                    (key=="delayA.mix"||key=="delayB.mix")) || view.block == 2 &&
                    (key == "delayA.mix" || key == "delayB.mix" ||
                     key == "delayA.time" || key == "delayB.time" ||
                     key == "delayA.feedback" || key == "delayB.feedback");
                change(drag, startValue + (delaySlider ? (x - startX) / (referenceDelay()?340.0:170.0) :
                    (startY - y) * ((w & MK_SHIFT) ? .0004 : .004)));
            }
            return 0;
        case WM_LBUTTONUP:
            if (reorder >= 0) {
                int dest = reorderHorizontal ? std::clamp((x - 24) / 222, 0, 4)
                                             : std::clamp((y - 90) / 113, 0, 4);
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
                reorderHorizontal = false;
            }
            drag = -1;
            ReleaseCapture();
            return 0;
        case WM_RBUTTONUP: {
            if(referenceDelay()){
                int id=-1;
                const int bank=std::clamp(view.section,0,1);
                if(y>=97&&y<287&&x>=24&&x<1135){
                    const int at=std::clamp((x-24)/222,0,4);
                    if(x-(24+at*222)<183&&y>=250){auto order=permutation(int(plain(layout().order,float(value(layout().order)))));id=layout().blockBypass[order[at]];}
                }
                if(y>=313&&y<360&&x>=852&&x<1193)id=layout().digital[bank]+2;
                if(y>=413&&y<444&&x>=928&&x<1052)id=layout().slotBypass[bank];
                if(id<0)id=hit(x,y);
                for(int b=0;b<2;++b)for(int kind=0;kind<2;++kind)
                    if(id==delayControl(b,kind))id=parameterId(std::string(b?"delayB.":"delayA.")+(kind?"feedback":"time"));
                if(id>=0)change(layout().midiLearn,double(id+1)/layout().controlCount);
                return 0;
            }
            int id = delayMidiTarget(x, y);
            if (id < 0) id = hit(x, y);
            if (view.block == 1 && y >= 695 && y <= 785) {
                const int center = view.section == 1 ? 1080 : W / 2;
                if (std::abs(x - center) < 60)
                    id = effectBypass();
            }
            if (y >= 250 && y < 287 && x >= 24 && x < 1135) {
                auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
                const int at=std::clamp((x-24)/222,0,4);
                if(x-(24+at*222)<183)id=layout().blockBypass[order[at]];
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
            } else if(!referenceDelay()) {
                controlPage=std::clamp(controlPage+(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1),0,controlPages-1);
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
