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
#include <d2d1.h>
#include <map>
#include <memory>
#ifdef MULTIFX_JUCE_GUI
#include "JerzyVSTGuiKit.h"
#endif
namespace multifx {
#ifdef MULTIFX_JUCE_GUI
class MonoAnalogLook final : public JerzyAudioUI::HardwareLookAndFeel {
public:
    MonoAnalogLook():HardwareLookAndFeel(JerzyAudioUI::steel()){}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float p,float a0,float a1,juce::Slider& slider) override {
        juce::Graphics::ScopedSaveState saved(g);g.setOpacity(slider.isEnabled()?1.0f:.45f);
        auto b=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(5.0f);
        const float d=juce::jmin(b.getWidth(),b.getHeight());
        auto r=juce::Rectangle<float>(d,d).withCentre(b.getCentre());const auto cc=r.getCentre();
        g.setColour(juce::Colour(0xff050505).withAlpha(.8f));g.fillEllipse(r.translated(0.0f,d*.045f));
        juce::ColourGradient rim(juce::Colour(0xffddd7c7),cc.x,r.getY(),juce::Colour(0xff292b2c),cc.x,r.getBottom(),false);
        rim.addColour(.48,juce::Colour(0xff777a79));g.setGradientFill(rim);g.fillEllipse(r);
        g.setColour(juce::Colour(0xff0e0f0f));g.drawEllipse(r,juce::jmax(1.0f,d*.035f));
        auto face=r.reduced(d*.105f);
        juce::ColourGradient body(juce::Colour(0xff4b8193),face.getX(),face.getY(),juce::Colour(0xff192c34),face.getRight(),face.getBottom(),false);
        body.addColour(.38,juce::Colour(0xff496a76));body.addColour(.72,juce::Colour(0xff293b43));
        g.setGradientFill(body);g.fillEllipse(face);
        g.setColour(juce::Colour(0xff9fd6e2).withAlpha(.75f));g.drawEllipse(face,d*.018f);
        auto inner=face.reduced(d*.18f);
        juce::ColourGradient cap(juce::Colour(0xffd6e0df),inner.getX(),inner.getY(),juce::Colour(0xff555d60),inner.getRight(),inner.getBottom(),false);
        cap.addColour(.5,juce::Colour(0xff929fa2));g.setGradientFill(cap);g.fillEllipse(inner);
        const float a=a0+p*(a1-a0),tickR=d*.49f;
        for(int i=0;i<=20;++i){
            const float t=float(i)/20.0f,angle=a0+t*(a1-a0);
            juce::Point<float> p1(cc.x+std::sin(angle)*(tickR-d*.035f),cc.y-std::cos(angle)*(tickR-d*.035f));
            juce::Point<float> p2(cc.x+std::sin(angle)*(tickR+(i%5==0?d*.005f:-d*.01f)),cc.y-std::cos(angle)*(tickR+(i%5==0?d*.005f:-d*.01f)));
            g.setColour(t<=p?juce::Colour(0xff67e3ee):juce::Colour(0xff71858d));
            g.drawLine(p1.x,p1.y,p2.x,p2.y,juce::jmax(1.0f,d*.012f));
        }
        juce::Path arc;arc.addCentredArc(cc.x,cc.y,tickR,tickR,0.0f,a0,a,true);
        g.setColour(juce::Colour(0xff45cadc));g.strokePath(arc,juce::PathStrokeType(juce::jmax(2.0f,d*.035f)));
        juce::Path pointer;pointer.startNewSubPath(cc.x,cc.y-d*.10f);pointer.lineTo(cc.x,cc.y-d*.39f);
        pointer.applyTransform(juce::AffineTransform::rotation(a,cc.x,cc.y));
        g.setColour(juce::Colour(0xff09232d));g.strokePath(pointer,juce::PathStrokeType(juce::jmax(4.0f,d*.09f),juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xffe1faff));g.strokePath(pointer,juce::PathStrokeType(juce::jmax(2.0f,d*.04f),juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    }
};
#endif
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
    struct TextOp {int x,y,w,size;COLORREF tint;std::string value;};
    std::vector<TextOp> textOps;
    ID2D1Factory *drawFactory = nullptr;
    ID2D1DCRenderTarget *canvas = nullptr;
    std::map<COLORREF,ID2D1SolidColorBrush*> vectorBrushes;
#ifdef MULTIFX_JUCE_GUI
    class JuceCanvas final : public juce::Component {
        Editor &owner;
        bool rightPressed=false;
        LPARAM point(const juce::MouseEvent &e) const {
            return MAKELPARAM(e.x,e.y);
        }
    public:
        explicit JuceCanvas(Editor &e):owner(e){setOpaque(true);}
        void paint(juce::Graphics &g) override {owner.paint(nullptr,&g);}
        void mouseDown(const juce::MouseEvent &e) override {
            rightPressed=e.mods.isRightButtonDown();
            SendMessage(owner.window,rightPressed?WM_RBUTTONDOWN:WM_LBUTTONDOWN,
                        rightPressed?MK_RBUTTON:MK_LBUTTON,point(e));
        }
        void mouseDrag(const juce::MouseEvent &e) override {
            SendMessage(owner.window,WM_MOUSEMOVE,
                        (rightPressed?MK_RBUTTON:MK_LBUTTON)|(e.mods.isShiftDown()?MK_SHIFT:0),point(e));
        }
        void mouseUp(const juce::MouseEvent &e) override {
            SendMessage(owner.window,rightPressed?WM_RBUTTONUP:WM_LBUTTONUP,0,point(e));
            rightPressed=false;
        }
        void mouseWheelMove(const juce::MouseEvent &e,const juce::MouseWheelDetails &wheel) override {
            POINT p{e.x,e.y};ClientToScreen(owner.window,&p);
            const int delta=int(std::round(wheel.deltaY*WHEEL_DELTA));
            SendMessage(owner.window,WM_MOUSEWHEEL,
                        MAKEWPARAM(e.mods.isCtrlDown()?MK_CONTROL:0,delta),MAKELPARAM(p.x,p.y));
        }
    };
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juceInitialiser;
    std::unique_ptr<JuceCanvas> juceCanvas;
    juce::Graphics *juceGraphics=nullptr;
    MonoAnalogLook juceLook;
    juce::Slider drawingSlider;
    static juce::Colour juceColor(COLORREF c){
        return juce::Colour::fromRGB(GetRValue(c),GetGValue(c),GetBValue(c));
    }
    void requestRepaint(){if(juceCanvas)juceCanvas->repaint();}
#else
    void requestRepaint(){if(window)InvalidateRect(window,nullptr,FALSE);}
#endif
    static D2D1_COLOR_F color(COLORREF c, float alpha=1.f) {
        return D2D1::ColorF(GetRValue(c)/255.f,GetGValue(c)/255.f,GetBValue(c)/255.f,alpha);
    }
    ID2D1SolidColorBrush *brush(COLORREF tint) {
        auto &b=vectorBrushes[tint];
        if(!b && canvas)canvas->CreateSolidColorBrush(color(tint),&b);
        return b;
    }
    static D2D1_RECT_F rect(float x,float y,float w,float h) {return D2D1::RectF(x,y,x+w,y+h);}
    void ellipse(float x,float y,float radius,COLORREF tint,float alpha=1.f) {
#ifdef MULTIFX_JUCE_GUI
        juceGraphics->setColour(juceColor(tint).withAlpha(alpha));
        juceGraphics->fillEllipse(x-radius,y-radius,radius*2,radius*2);
#else
        ID2D1SolidColorBrush *b=nullptr;
        canvas->CreateSolidColorBrush(color(tint,alpha),&b);
        if(b){canvas->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x,y),radius,radius),b);b->Release();}
#endif
    }
    void gradient(float x,float y,float w,float h,COLORREF top,COLORREF bottom,bool oval=false) {
#ifdef MULTIFX_JUCE_GUI
        juceGraphics->setGradientFill(juce::ColourGradient(juceColor(top),x,y,juceColor(bottom),x+w,y+h,false));
        if(oval)juceGraphics->fillEllipse(x,y,w,h);
        else juceGraphics->fillRect(x,y,w,h);
#else
        ID2D1GradientStopCollection *stops=nullptr;
        ID2D1LinearGradientBrush *b=nullptr;
        D2D1_GRADIENT_STOP values[2]={{0,color(top)},{1,color(bottom)}};
        if(SUCCEEDED(canvas->CreateGradientStopCollection(values,2,&stops))&&stops){
            canvas->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(x,y),D2D1::Point2F(x+w,y+h)),stops,&b);
            if(b){if(oval)canvas->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x+w/2,y+h/2),w/2,h/2),b);
                  else canvas->FillRectangle(rect(x,y,w,h),b);b->Release();}
            stops->Release();
        }
#endif
    }
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
        requestRepaint();
    }
    void destroyCombos() {
        for (auto h : combos)
            DestroyWindow(h);
        combos.clear();
    }
    void showChoice(int id,int x,int y) {
        HWND list=GetDlgItem(window,10000+id);
        if(!list)return;
        HMENU menu=CreatePopupMenu();
        const int count=int(SendMessage(list,CB_GETCOUNT,0,0));
        const int selected=int(SendMessage(list,CB_GETCURSEL,0,0));
        std::vector<std::wstring> menuLabels;
        menuLabels.reserve(std::max(0,count));
        for(int option=0;option<count;++option){
            const int length=int(SendMessage(list,CB_GETLBTEXTLEN,option,0));
            if(length<0||length>4096)continue;
            std::vector<char> label(size_t(length)+1);
            SendMessageA(list,CB_GETLBTEXT,option,reinterpret_cast<LPARAM>(label.data()));
            const int n=MultiByteToWideChar(CP_UTF8,0,label.data(),-1,nullptr,0);
            std::vector<wchar_t> wide(std::max(1,n));
            MultiByteToWideChar(CP_UTF8,0,label.data(),-1,wide.data(),n);
            menuLabels.emplace_back(wide.data());
            AppendMenuW(menu,MF_OWNERDRAW|(option==selected?MF_CHECKED:0),30000+option,
                        reinterpret_cast<LPCWSTR>(&menuLabels.back()));
        }
        POINT origin{int(x/sx),int(y/sy)};
        ClientToScreen(window,&origin);
        const int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,
                                        origin.x,origin.y,0,window,nullptr);
        DestroyMenu(menu);
        if(chosen>=30000&&chosen<30000+count){
            SendMessage(list,CB_SETCURSEL,chosen-30000,0);
            SendMessage(window,WM_COMMAND,MAKEWPARAM(10000+id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(list));
        }
    }
    void fill(HDC, int x, int y, int w, int h, COLORREF tint) {
#ifdef MULTIFX_JUCE_GUI
        juceGraphics->setColour(juceColor(tint));juceGraphics->fillRect(x,y,w,h);
#else
        if(auto *b=brush(tint))canvas->FillRectangle(rect(float(x),float(y),float(w),float(h)),b);
#endif
    }
    void text(HDC, int x, int y, int w, const char *s, int size, COLORREF tint) {
        if(!s)return;
        textOps.push_back({x,y,w,size,tint,s});
    }
    void drawTextPass(HDC dc) {
#ifdef MULTIFX_JUCE_GUI
        for(const auto &op:textOps){
            juceGraphics->setColour(juceColor(op.tint));
            juceGraphics->setFont(juce::Font(juce::FontOptions(float(op.size)).withStyle("Bold")));
            juceGraphics->drawFittedText(juce::String::fromUTF8(op.value.c_str()),
                op.x,op.y,op.w,op.size+8,juce::Justification::centred,1,0.8f);
        }
#else
        for(const auto &op:textOps){
            const int fontSize=std::clamp(int(std::round(op.size/sy)),10,63);
            if(!fonts[fontSize])fonts[fontSize]=CreateFontW(fontSize,0,0,0,FW_SEMIBOLD,0,0,0,
                DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
            const auto old=SelectObject(dc,fonts[fontSize]?fonts[fontSize]:GetStockObject(DEFAULT_GUI_FONT));
            const int n=MultiByteToWideChar(CP_UTF8,0,op.value.c_str(),-1,nullptr,0);
            if(n>0){
                std::wstring wide(size_t(n),L'\0');
                MultiByteToWideChar(CP_UTF8,0,op.value.c_str(),-1,wide.data(),n);
                RECT box{int(std::round(op.x/sx)),int(std::round(op.y/sy)),
                    int(std::round((op.x+op.w)/sx)),int(std::round((op.y+op.size+8)/sy))};
                SetTextColor(dc,op.tint);SetBkMode(dc,TRANSPARENT);
                DrawTextW(dc,wide.c_str(),-1,&box,DT_SINGLELINE|DT_END_ELLIPSIS|DT_CENTER|DT_VCENTER);
            }
            SelectObject(dc,old);
        }
#endif
        textOps.clear();
    }
    void line(HDC, int x, int y, int xx, int yy, COLORREF tint, int width = 1) {
#ifdef MULTIFX_JUCE_GUI
        juceGraphics->setColour(juceColor(tint));
        juceGraphics->drawLine(float(x),float(y),float(xx),float(yy),std::max(1.2f,float(width)));
#else
        if(auto *b=brush(tint))canvas->DrawLine(D2D1::Point2F(float(x),float(y)),
            D2D1::Point2F(float(xx),float(yy)),b,
            std::max(float(width),1.5f*std::max(sx,sy)));
#endif
    }
    void disk(HDC, int x, int y, int r, COLORREF tint) {
#ifdef MULTIFX_JUCE_GUI
        juceGraphics->setColour(juceColor(tint));juceGraphics->fillEllipse(float(x-r),float(y-r),float(r*2),float(r*2));
#else
        if(auto *b=brush(tint))canvas->FillEllipse(D2D1::Ellipse(D2D1::Point2F(float(x),float(y)),float(r),float(r)),b);
#endif
    }
    void bevel(HDC dc, int x, int y, int w, int h, COLORREF tint) {
        fill(dc, x, y, w, h, tint);
        line(dc, x, y, x+w-1, y, RGB(69,88,103));
        line(dc, x, y, x, y+h-1, RGB(51,69,84));
        line(dc, x, y+h-1, x+w, y+h-1, RGB(10,19,30));
        line(dc, x+w-1, y, x+w-1, y+h, RGB(10,19,30));
    }
    void metalPanel(HDC dc, int x, int y, int w, int h, bool raised = true) {
#ifdef MULTIFX_JUCE_GUI
        auto area=juce::Rectangle<float>(float(x),float(y),float(w),float(h));
        if(raised)JerzyAudioUI::paintPanel(*juceGraphics,area,juceLook.getTheme(),4.0f);
        else{
            juceGraphics->setGradientFill(juce::ColourGradient(juce::Colour(0xff0c1923),area.getTopLeft(),
                juce::Colour(0xff1c2c36),area.getBottomRight(),false));
            juceGraphics->fillRoundedRectangle(area,3.0f);
            juceGraphics->setColour(juce::Colour(0xff536f7e));
            juceGraphics->drawRoundedRectangle(area,3.0f,1.0f);
        }
#else
        fill(dc,x+2,y+3,w,h,RGB(2,8,14));
        gradient(float(x),float(y),float(w),float(h),
            raised?RGB(37,47,53):RGB(12,25,35),raised?RGB(13,26,34):RGB(28,44,54));
        line(dc,x,y,x+w-1,y,raised?RGB(118,134,141):RGB(53,71,82));
        line(dc,x,y,x,y+h-1,RGB(80,99,108));
        line(dc,x,y+h-1,x+w,y+h-1,RGB(3,9,15),2);
        line(dc,x+w-1,y,x+w-1,y+h,RGB(3,9,15),2);
#endif
    }
    void screw(HDC dc,int x,int y){
        disk(dc,x+1,y+2,5,RGB(3,9,15));
        disk(dc,x,y,4,RGB(113,126,131));
        disk(dc,x,y,3,RGB(46,57,63));
        line(dc,x-2,y+1,x+2,y-1,RGB(157,169,171));
    }
    void lamp(HDC dc,int x,int y,bool on,COLORREF tint){
        disk(dc,x,y,10,RGB(2,8,13));
        disk(dc,x,y,8,RGB(102,132,141));
        disk(dc,x,y,6,on?tint:RGB(79,47,52));
    }
    void toggleSwitch(HDC dc,int x,int y,bool on,int scale=1){
        const int radius=scale==1?13:17;
        disk(dc,x+1,y+2,radius+2,RGB(1,7,11));
        gradient(float(x-radius),float(y-radius),float(radius*2),float(radius*2),
            RGB(195,204,204),RGB(67,77,80),true);
        disk(dc,x,y,radius-4,RGB(13,26,32));
        const int tipY=y+(on?-7:7);
        line(dc,x,y,x,tipY,RGB(9,14,18),7);
        line(dc,x-1,y,x-1,tipY,RGB(231,236,225),4);
        disk(dc,x-1,tipY,scale==1?4:5,RGB(217,224,213));
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
        // Compact vector drawing of a studio module; all lettering remains font based.
        metalPanel(dc,x+10,y+48,163,85,false);
        fill(dc,x+14,y+52,155,77,RGB(24,35,41));
        bevel(dc,x+21,y+57,141,27,RGB(5,15,20));
        const char *names[]{"PREAMP", "MODULATION", "DUAL DELAY", "REVERB", "CABINET"};
        text(dc,x+23,y+61,137,names[std::clamp(block,0,4)],12,RGB(136,219,228));
        lamp(dc,x+155,y+70,true,RGB(67,222,172));
        for(int i=0;i<3;++i){
            const int cx=x+45+i*47,cy=y+107;
            disk(dc,cx+1,cy+2,13,RGB(3,8,11));
            disk(dc,cx,cy,11,RGB(165,187,192));
            disk(dc,cx,cy,8,RGB(33,48,54));
            line(dc,cx,cy,cx+int(std::sin((i-1)*.62)*7),cy-int(std::cos((i-1)*.62)*7),RGB(226,240,240),2);
        }
    }
    void chrome(HDC dc) {
#ifdef MULTIFX_JUCE_GUI
        JerzyAudioUI::paintChassis(*juceGraphics,juce::Rectangle<float>(0,0,float(W),float(H)),juceLook.getTheme());
#else
        fill(dc,0,0,W,H,RGB(7,17,27));
#endif
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
            metalPanel(dc,x+40,247,111,35);
            fill(dc,x+43,250,105,29,off?RGB(50,21,30):RGB(6,34,29));
            lamp(dc,x+59,264,true,off?RGB(232,57,75):RGB(43,233,135));
            text(dc,x+70,254,46,off?"OFF":"ON",15,off?RGB(244,130,141):RGB(78,244,162));
            toggleSwitch(dc,x+134,264,!off);
            if(i<4){line(dc,x+188,188,x+215,188,RGB(43,207,232),2);line(dc,x+207,183,x+215,188,RGB(43,207,232),2);line(dc,x+207,193,x+215,188,RGB(43,207,232),2);}
        }
        metalPanel(dc,1135,97,126,190);
        fill(dc,1138,100,120,184,RGB(8,19,28));
        levelBar(dc,1152,"IN",needleIn);
        levelBar(dc,1208,"OUT",needleOut);
    }
    void referenceKnob(HDC dc, const Control &c, const char *label, int radius, COLORREF color) {
        // Draw the control from circles, arcs and strokes. Small radial marks and
        // brushed-metal hatching turn into isolated pixels at reduced host sizes.
        const float cx=float(c.x+c.w/2),cy=float(c.y+(radius==40?71:radius>=34?63:56));
        const float r=float(radius);
        text(dc,c.x,c.y,c.w,label,radius>=34?14:12,RGB(224,242,246));
#ifdef MULTIFX_JUCE_GUI
        const int diameter=int(std::round(r*2.65f));
        juceLook.drawRotarySlider(*juceGraphics,int(cx)-diameter/2,int(cy)-diameter/2,
            diameter,diameter,float(value(c.id)),juce::MathConstants<float>::pi*1.25f,
            juce::MathConstants<float>::pi*2.75f,drawingSlider);
#else
        auto ring=[&](float x,float y,float rr,COLORREF tint,float width=1.f){
            if(auto*b=brush(tint))canvas->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x,y),rr,rr),b,width);
        };
        auto stroke=[&](float x,float y,float xx,float yy,COLORREF tint,float width){
            if(auto*b=brush(tint))canvas->DrawLine(D2D1::Point2F(x,y),D2D1::Point2F(xx,yy),b,width);
        };
        auto arc=[&](float rr,double start,double sweep,COLORREF tint,float width){
            if(sweep<=0.001)return;
            ID2D1PathGeometry *path=nullptr;
            if(FAILED(drawFactory->CreatePathGeometry(&path))||!path)return;
            ID2D1GeometrySink *sink=nullptr;
            if(SUCCEEDED(path->Open(&sink))&&sink){
                constexpr double pi=3.141592653589793;
                const double end=start+sweep;
                sink->BeginFigure(D2D1::Point2F(cx+float(rr*std::cos(start*pi/180.)),
                                                 cy+float(rr*std::sin(start*pi/180.))),
                                  D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddArc(D2D1::ArcSegment(
                    D2D1::Point2F(cx+float(rr*std::cos(end*pi/180.)),
                                   cy+float(rr*std::sin(end*pi/180.))),
                    D2D1::SizeF(rr,rr),0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,
                    sweep>180.?D2D1_ARC_SIZE_LARGE:D2D1_ARC_SIZE_SMALL));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
                if(SUCCEEDED(sink->Close()))if(auto*b=brush(tint))canvas->DrawGeometry(path,b,width);
                sink->Release();
            }
            path->Release();
        };
        constexpr double pi=3.141592653589793;
        // The three continuous rings, rather than stippled pixels, define the form.
        ellipse(cx+1.5f,cy+2.5f,r+2.f,RGB(2,9,16),.65f);
        ellipse(cx,cy,r,RGB(20,32,43));
        ring(cx,cy,r,RGB(157,184,194),2.2f);
        ring(cx,cy,r-4.5f,RGB(55,83,96),2.f);
        ellipse(cx,cy,r-7.f,RGB(83,111,124));
        ring(cx,cy,r-7.f,RGB(212,230,234),1.8f);
        ellipse(cx-r*.20f,cy-r*.25f,r*.19f,RGB(193,220,225),.12f);
        arc(r+5.5f,135.,270.,RGB(77,113,128),2.f);
        arc(r+5.5f,135.,std::clamp(value(c.id),0.,1.)*270.,color,2.6f);
        for(int i=0;i<=8;++i){
            const double a=(135.+33.75*i)*pi/180.;
            const float ux=float(std::cos(a)),uy=float(std::sin(a));
            stroke(cx+ux*(r+9.f),cy+uy*(r+9.f),cx+ux*(r+13.f),cy+uy*(r+13.f),
                   i<=int(std::round(value(c.id)*8.))?color:RGB(117,154,169),2.f);
        }
        const double a=(135.+270.*value(c.id))*pi/180.;
        const float ux=float(std::cos(a)),uy=float(std::sin(a));
        stroke(cx+ux*r*.10f,cy+uy*r*.10f,cx+ux*r*.68f,cy+uy*r*.68f,RGB(9,25,34),6.f);
        stroke(cx+ux*r*.10f,cy+uy*r*.10f,cx+ux*r*.68f,cy+uy*r*.68f,RGB(244,251,250),3.4f);
        ellipse(cx+ux*r*.68f,cy+uy*r*.68f,1.7f,RGB(244,251,250));
#endif
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
        const int readoutY=c.y+(radius==40?121:radius>=34?109:87);
        const int readoutW=std::min(c.w-4,radius*2+58);
        bevel(dc,cx-readoutW/2,readoutY-3,readoutW,22,RGB(5,18,27));
        text(dc,cx-readoutW/2,readoutY-2,readoutW,reading.c_str(),radius>=34?13:12,
             RGB(112,231,240));
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
            const int pageStart=controlPage*6,pageEnd=std::min(total,(controlPage+1)*6);
            const int visibleCompact=std::max(0,std::min(pageEnd,int(compact.size()))-pageStart);
            const int visibleKnobs=pageEnd-pageStart-visibleCompact;
            const bool grouped=visibleCompact<=3&&visibleKnobs<=3;
            for(int index=controlPage*6;index<std::min(total,(controlPage+1)*6);++index){
                const bool isCompact=index<int(compact.size());
                const int id=isCompact?compact[index]:knobs[index-int(compact.size())];
                const int local=index-controlPage*6;
                int x=28+(local%3)*411,y=375+(local/3)*131;
                if(grouped){
                    const int count=isCompact?visibleCompact:visibleKnobs;
                    const int inRow=isCompact?local:local-visibleCompact;
                    x=28+(3-count)*205+inRow*411;
                    y=isCompact?(visibleKnobs?385:420):(visibleCompact?460:420);
                }
                controls.push_back({id,x,y,388,isCompact?54:126,isCompact});
            }
        }
        for (auto &c : controls) {
            const auto &p = controlUI()[c.id].definition;
            if (p.choices.empty() && !(p.steps > 1 && p.steps <= 64))
                continue;
            const int yy = c.y - scroll;
            if (yy < 370 || yy + 55 > controlBottom)
                continue;
            HWND h = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST,
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
        HWND factory = referenceDelay() ? nullptr : CreateWindowA("COMBOBOX", "", WS_CHILD | CBS_DROPDOWNLIST, int(430 / sx),
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
        requestRepaint();
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
    void paint(HDC supplied = nullptr
#ifdef MULTIFX_JUCE_GUI
        ,juce::Graphics *g = nullptr
#endif
    ) {
#ifdef MULTIFX_JUCE_GUI
        if(!g)return;
        juceGraphics=g;
        juce::Graphics::ScopedSaveState saved(*g);
        RECT client{};GetClientRect(window,&client);
        sx=float(W)/std::max(1L,client.right);sy=float(H)/std::max(1L,client.bottom);
        g->addTransform(juce::AffineTransform::scale(float(client.right)/W,float(client.bottom)/H));
        textOps.clear();
        HDC target=nullptr;
        auto finish=[&]{drawTextPass(nullptr);juceGraphics=nullptr;};
#else
        PAINTSTRUCT ps{};
        auto target = supplied ? supplied : BeginPaint(window, &ps);
        RECT client;
        GetClientRect(window, &client);
        sx = float(W) / std::max(1L, client.right);
        sy = float(H) / std::max(1L, client.bottom);
        if(!drawFactory){if(!supplied)EndPaint(window,&ps);return;}
        if(!canvas){
            const auto props=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE));
            if(FAILED(drawFactory->CreateDCRenderTarget(&props,&canvas))){
                if(!supplied)EndPaint(window,&ps);return;
            }
            canvas->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        }
        if(FAILED(canvas->BindDC(target,&client))){if(!supplied)EndPaint(window,&ps);return;}
        textOps.clear();
        canvas->BeginDraw();
        canvas->SetTransform(D2D1::Matrix3x2F::Scale(float(client.right)/W,float(client.bottom)/H));
        auto finish=[&] {
            const HRESULT hr=canvas->EndDraw();
            if(hr==D2DERR_RECREATE_TARGET){
                for(auto &[key,b]:vectorBrushes)if(b)b->Release();
                vectorBrushes.clear();canvas->Release();canvas=nullptr;
            }
            drawTextPass(target);
            if(!supplied)EndPaint(window,&ps);
        };
#endif
        if (referenceDelay()) {
            chrome(target);
            referenceDelayPanel(target);
            finish();
            return;
        }
        HDC dc=target;
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
            toggleSwitch(dc,cx,cy,value(effectBypass())<.5,2);
            text(dc, cx - 110, 765, 220, value(effectBypass()) >= .5 ? "BYPASS / OFF" : "EFFECT ON", 13,
                 RGB(232,221,190));
            lamp(dc,cx,cy-37,true,value(effectBypass()) >= .5 ? RGB(241,81,99) : RGB(65,236,143));
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
#ifdef MULTIFX_JUCE_GUI
        juceGraphics->saveState();
        juceGraphics->reduceClipRegion(20,370,1240,controlBottom-370);
#else
        canvas->PushAxisAlignedClip(rect(20,370,1240,float(controlBottom-370)),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
#endif
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
                    lamp(dc,c.x+17,y+38,true,on?RGB(65,236,143):RGB(241,81,99));
                    toggleSwitch(dc,c.x+43,y+38,on);
                }
                text(dc, c.x+65, y + 28, c.w-69,
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
#ifdef MULTIFX_JUCE_GUI
        juceGraphics->restoreState();
#else
        canvas->PopAxisAlignedClip();
#endif
        bevel(dc,1031,638,48,22,RGB(21,51,67));
        text(dc,1031,639,48,"<",14,controlPage>0?RGB(229,246,252):RGB(87,117,131));
        bevel(dc,1083,638,100,22,RGB(15,36,50));
        char pageLabel[32];snprintf(pageLabel,sizeof(pageLabel),"STRONA %d/%d",controlPage+1,controlPages);
        text(dc,1083,642,100,pageLabel,11,RGB(215,235,244));
        bevel(dc,1187,638,48,22,RGB(21,51,67));
        text(dc,1187,639,48,">",14,controlPage+1<controlPages?RGB(229,246,252):RGB(87,117,131));
        const char *buttons[]{"WCZYTAJ IR WAV", "WCZYTAJ PRESET", "ZAPISZ PRESET", "MASTER BYPASS"};
        buttons[3]=value(layout().bypass)>=.5 ? "MASTER WYŁĄCZONY" : "MASTER AKTYWNY";
        for (int i = 0; i < 4; ++i) {
            fill(dc, 28 + i * 250, 805, 238, 32, RGB(26,53,72));
            text(dc, 28 + i * 250, 811, 238, buttons[i], 14, RGB(226,248,252));
        }
        text(dc, 1028, 807, 225, "Prawy klik: MIDI Learn", 13, RGB(160, 191, 205));
        finish();
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
        case WM_MEASUREITEM: {
            auto *item=reinterpret_cast<MEASUREITEMSTRUCT *>(lp);
            if(item&&item->CtlType==ODT_MENU&&item->itemID>=30000){
                item->itemWidth=UINT(std::max(210,int(300/sx)));
                item->itemHeight=UINT(std::max(27,int(32/sy)));
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            auto *item=reinterpret_cast<DRAWITEMSTRUCT *>(lp);
            if(item&&item->CtlType==ODT_MENU&&item->itemID>=30000){
                const bool selected=(item->itemState&ODS_SELECTED)!=0;
                HBRUSH background=CreateSolidBrush(selected?RGB(21,95,116):RGB(12,32,44));
                FillRect(item->hDC,&item->rcItem,background);DeleteObject(background);
                if(item->itemState&ODS_CHECKED){
                    HBRUSH mark=CreateSolidBrush(RGB(54,220,238));
                    RECT bar{item->rcItem.left+8,item->rcItem.top+5,item->rcItem.left+12,item->rcItem.bottom-5};
                    FillRect(item->hDC,&bar,mark);DeleteObject(mark);
                }
                auto *caption=reinterpret_cast<const std::wstring *>(item->itemData);
                if(caption){
                    HFONT font=CreateFontW(std::max(12,int(14/sy)),0,0,0,FW_SEMIBOLD,0,0,0,
                        DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
                    auto old=SelectObject(item->hDC,font);
                    SetBkMode(item->hDC,TRANSPARENT);
                    SetTextColor(item->hDC,selected?RGB(255,255,255):RGB(217,239,245));
                    RECT label=item->rcItem;label.left+=25;label.right-=10;
                    DrawTextW(item->hDC,caption->c_str(),-1,&label,
                        DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_LEFT);
                    SelectObject(item->hDC,old);DeleteObject(font);
                }
                return TRUE;
            }
            break;
        }
        case WM_PRINTCLIENT:
#ifdef MULTIFX_JUCE_GUI
            if(auto dc=reinterpret_cast<HDC>(w)){
                RECT bounds{};GetClientRect(window,&bounds);
                const int width=std::max(1L,bounds.right),height=std::max(1L,bounds.bottom);
                juce::Image image(juce::Image::ARGB,width,height,true);
                juce::Graphics graphics(image);paint(nullptr,&graphics);
                std::vector<DWORD> pixels(size_t(width)*size_t(height));
                for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                    const auto c=image.getPixelAt(x,y);
                    pixels[size_t(y)*size_t(width)+size_t(x)]=RGB(c.getBlue(),c.getGreen(),c.getRed());
                }
                BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
                info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
                SetDIBitsToDevice(dc,0,0,width,height,0,0,0,height,pixels.data(),&info,DIB_RGB_COLORS);
            }
#else
            paint(reinterpret_cast<HDC>(w));
#endif
            return 0;
        case WM_PAINT:
#ifdef MULTIFX_JUCE_GUI
            {PAINTSTRUCT ps{};BeginPaint(window,&ps);EndPaint(window,&ps);}
#else
            paint();
#endif
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
#ifdef MULTIFX_JUCE_GUI
            if(juceCanvas)juceCanvas->setSize(LOWORD(lp),HIWORD(lp));
#endif
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
            const float previousIn=needleIn,previousOut=needleOut;
            needleIn=follow(needleIn,controller->meterIn.load());
            needleOut=follow(needleOut,controller->meterOut.load());
            if (drag < 0 && selectionSignature() != signature)
                rebuild();
            if(std::abs(needleIn-previousIn)>.002f||std::abs(needleOut-previousOut)>.002f)
                requestRepaint();
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
            if(y>=36&&y<76&&x>=430&&x<704){showChoice(layout().factoryProgram,430,76);return 0;}
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
                const auto &definition=controlUI()[id].definition;
                if(!definition.choices.empty()||(definition.steps>1&&definition.steps<=64)){
                    showChoice(id,x,y+16);return 0;
                }
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
#ifndef MULTIFX_JUCE_GUI
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&drawFactory);
#endif
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
        for(auto &[key,b]:vectorBrushes)if(b)b->Release();
        if(canvas)canvas->Release();
        if(drawFactory)drawFactory->Release();
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
#ifdef MULTIFX_JUCE_GUI
        if(!juceInitialiser)juceInitialiser=std::make_unique<juce::ScopedJuceInitialiser_GUI>();
        juceCanvas=std::make_unique<JuceCanvas>(*this);
        juceCanvas->setSize(getRect().getWidth(),getRect().getHeight());
        juceCanvas->addToDesktop(0,window);
        juceCanvas->setVisible(true);
#endif
        systemWindow = parent;
        // Meter repaint is independent of parameter edits, which invalidate immediately.
        SetTimer(window, 1, 60, nullptr);
        rebuild();
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API removed() override {
        if (window) {
#ifdef MULTIFX_JUCE_GUI
            if(juceCanvas){juceCanvas->removeFromDesktop();juceCanvas.reset();}
#endif
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
