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
#include "BinaryData.h"
#endif
namespace multifx {
#ifdef MULTIFX_JUCE_GUI
struct Gui3DAssets final {
    enum Family { master, fx, select };
    enum Accent { neutral, amber, cyan, red, purple };
    juce::Image knobs[3][5];
    juce::Image shadows[3];
    juce::Image chassis;
    juce::Image module[5];
    juce::Image buttons[5][5];
    juce::Image leds[5][2];
    juce::Image vuFace, vuNeedle, vuGlass;

    static juce::Image decode(const void *data, int bytes) {
        return juce::ImageFileFormat::loadFrom(data, size_t(bytes));
    }
    Gui3DAssets() {
#define LOAD(target, resource) target = decode(BinaryData::resource, BinaryData::resource##Size)
        LOAD(knobs[master][neutral], knob_master_neutral_101_png); LOAD(knobs[master][amber], knob_master_amber_101_png);
        LOAD(knobs[master][cyan], knob_master_cyan_101_png); LOAD(knobs[master][red], knob_master_red_101_png);
        LOAD(knobs[master][purple], knob_master_purple_101_png);
        LOAD(knobs[fx][neutral], knob_fx_neutral_101_png); LOAD(knobs[fx][amber], knob_fx_amber_101_png);
        LOAD(knobs[fx][cyan], knob_fx_cyan_101_png); LOAD(knobs[fx][red], knob_fx_red_101_png);
        LOAD(knobs[fx][purple], knob_fx_purple_101_png);
        LOAD(knobs[select][neutral], knob_select_neutral_101_png); LOAD(knobs[select][amber], knob_select_amber_101_png);
        LOAD(knobs[select][cyan], knob_select_cyan_101_png); LOAD(knobs[select][red], knob_select_red_101_png);
        LOAD(knobs[select][purple], knob_select_purple_101_png);
        LOAD(shadows[master], knob_master_shadow_png); LOAD(shadows[fx], knob_fx_shadow_png);
        LOAD(shadows[select], knob_select_shadow_png);
        LOAD(chassis, chassis_dark_256_png);
        LOAD(module[neutral], module_neutral_320x180_png); LOAD(module[amber], module_amber_320x180_png);
        LOAD(module[cyan], module_cyan_320x180_png); LOAD(module[red], module_red_320x180_png);
        LOAD(module[purple], module_purple_320x180_png);
#define LOAD_BUTTON(colour) \
        LOAD(buttons[colour][0], button_##colour##_off_png); LOAD(buttons[colour][1], button_##colour##_on_png); \
        LOAD(buttons[colour][2], button_##colour##_hover_png); LOAD(buttons[colour][3], button_##colour##_pressed_png); \
        LOAD(buttons[colour][4], button_##colour##_disabled_png); \
        LOAD(leds[colour][0], led_##colour##_off_png); LOAD(leds[colour][1], led_##colour##_on_png)
        LOAD_BUTTON(neutral); LOAD_BUTTON(amber); LOAD_BUTTON(cyan); LOAD_BUTTON(red); LOAD_BUTTON(purple);
#undef LOAD_BUTTON
        LOAD(vuFace, vu_face_png); LOAD(vuNeedle, vu_needle_png); LOAD(vuGlass, vu_glass_png);
#undef LOAD
    }
    static Gui3DAssets &get() { static Gui3DAssets assets; return assets; }
    static Accent closestAccent(juce::Colour c) {
        const std::array<juce::Colour,5> colours{juce::Colour(0xffbec6c9),juce::Colour(0xffff9914),
            juce::Colour(0xff24daf2),juce::Colour(0xffff4946),juce::Colour(0xffb84dff)};
        int best=0; int distance=std::numeric_limits<int>::max();
        for(int i=0;i<5;++i){
            const int dr=int(c.getRed())-int(colours[i].getRed());
            const int dg=int(c.getGreen())-int(colours[i].getGreen());
            const int db=int(c.getBlue())-int(colours[i].getBlue());
            const int candidate=dr*dr+dg*dg+db*db;
            if(candidate<distance){distance=candidate;best=i;}
        }
        return Accent(best);
    }
};
class MonoAnalogLook final : public JerzyAudioUI::HardwareLookAndFeel {
public:
    MonoAnalogLook():HardwareLookAndFeel(JerzyAudioUI::steel()){}
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float p,float a0,float a1,juce::Slider& slider) override {
        juce::Graphics::ScopedSaveState saved(g);
        g.setOpacity(slider.isEnabled()?1.0f:.45f);
        auto &assets=Gui3DAssets::get();
        const int diameter=juce::jmin(w,h);
        const auto family=diameter>=100?Gui3DAssets::master:(diameter>=78?Gui3DAssets::fx:Gui3DAssets::select);
        const auto accent=Gui3DAssets::closestAccent(slider.findColour(juce::Slider::rotarySliderFillColourId));
        const auto &strip=assets.knobs[family][accent];
        const auto &shadow=assets.shadows[family];
        const int frameWidth=strip.getHeight();
        const int frame=juce::jlimit(0,100,int(std::lround(juce::jlimit(0.0f,1.0f,p)*100.0f)));
        const int d=juce::jmin(w,h),dx=x+(w-d)/2,dy=y+(h-d)/2;
        if(shadow.isValid())g.drawImage(shadow,dx,dy,d,d,0,0,shadow.getWidth(),shadow.getHeight());
        if(strip.isValid())g.drawImage(strip,dx,dy,d,d,frame*frameWidth,0,frameWidth,strip.getHeight());
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
    static constexpr int controlTop = 218;
    static constexpr int controlBottom = 520;
    float sx = 1, sy = 1;
    static constexpr int W = 1280, H = 720;
    static constexpr int DefaultW = 1024, DefaultH = 576;
    struct Control {
        int id, x, y, w, h;
        bool compact;
    };
    std::vector<Control> controls;
    std::vector<HWND> combos;
    std::string signature;
    bool globalSettings = false;
    bool rackHome = true; // approved Rack Hybrid: detail editor stays available.
    bool engineMenuOpen = false;
    bool referenceDelay() const { return !rackHome && view.block == 2 && !view.advanced && !globalSettings; }
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
    double preparedParameterValue(int id,double v) const {
        if(id<0)return 0;
        v=clean(float(v));
        const int steps=controlUI()[id].definition.steps;
        return steps>0?std::round(v*steps)/steps:v;
    }
    void beginParameterGesture(int id){
        if(id>=0)controller->beginEdit(id);
    }
    void performParameterGesture(int id,double v){
        if(id<0)return;
        v=preparedParameterValue(id,v);
        controller->setParamNormalized(id,v);
        controller->performEdit(id,v);
        requestRepaint();
    }
    void endParameterGesture(int id){
        if(id>=0)controller->endEdit(id);
    }
    void learnParameter(int id){
        if(id<0)return;
        if(referenceDelay()){
            for(int bank=0;bank<2;++bank)for(int kind=0;kind<2;++kind)
                if(id==delayControl(bank,kind))
                    id=parameterId(std::string(bank?"delayB.":"delayA.")+(kind?"feedback":"time"));
        }
        change(layout().midiLearn,double(id+1)/layout().controlCount);
    }
#ifdef MULTIFX_JUCE_GUI
    bool jucePointerDispatch=false;
    void dispatchJucePointer(UINT message,WPARAM flags,LPARAM position){
        const bool previous=jucePointerDispatch;
        jucePointerDispatch=true;
        // Keep pointer handling inside the JUCE event callback. Sending the
        // message through the hidden parent HWND changes native focus/capture
        // in some hosts (including FL Studio) before JUCE can finish its drag.
        this->message(message,flags,position);
        jucePointerDispatch=previous;
    }
    class JuceCanvas final : public juce::Component {
        Editor &owner;
        bool rightPressed=false;
        LPARAM point(const juce::MouseEvent &e) const {
            return MAKELPARAM(e.x,e.y);
        }
    public:
        explicit JuceCanvas(Editor &e):owner(e){setOpaque(true);}
        float getDesktopScaleFactor() const override {
            // The VST3 host already supplies the editor's final pixel bounds. Letting
            // JUCE apply the monitor DPI scale again makes the child HWND larger than
            // the host viewport (most visibly in FL Studio at 125% and 150% DPI).
            return 1.0f;
        }
        void paint(juce::Graphics &g) override {owner.paint(nullptr,&g);}
        void mouseDown(const juce::MouseEvent &e) override {
            rightPressed=e.mods.isRightButtonDown();
            // Page navigation is part of the native JUCE canvas, not a
            // transient child control. Forward it through the existing editor
            // hit-test just like section and preset navigation.
            // This preserves one input path across Windows/FL Studio hosts.
            owner.dispatchJucePointer(rightPressed?WM_RBUTTONDOWN:WM_LBUTTONDOWN,
                                      rightPressed?MK_RBUTTON:MK_LBUTTON,point(e));
        }
        void mouseDrag(const juce::MouseEvent &e) override {
            owner.dispatchJucePointer(WM_MOUSEMOVE,
                                      (rightPressed?MK_RBUTTON:MK_LBUTTON)|(e.mods.isShiftDown()?MK_SHIFT:0),point(e));
        }
        void mouseUp(const juce::MouseEvent &e) override {
            owner.dispatchJucePointer(rightPressed?WM_RBUTTONUP:WM_LBUTTONUP,0,point(e));
            rightPressed=false;
        }
        void mouseWheelMove(const juce::MouseEvent &e,const juce::MouseWheelDetails &wheel) override {
            POINT p{e.x,e.y};ClientToScreen(owner.window,&p);
            const int delta=int(std::round(wheel.deltaY*WHEEL_DELTA));
            owner.dispatchJucePointer(WM_MOUSEWHEEL,
                                      MAKEWPARAM(e.mods.isCtrlDown()?MK_CONTROL:0,delta),MAKELPARAM(p.x,p.y));
        }
    };
    class JuceParameterSlider final : public juce::Slider {
        Editor &owner;
        int parameter=-1;
        bool gesture=false;
        juce::Rectangle<int> designBounds;
    public:
        JuceParameterSlider(Editor &e,int id,bool horizontal,juce::Rectangle<int> bounds)
            :juce::Slider(horizontal?juce::Slider::LinearHorizontal:
                                     juce::Slider::RotaryHorizontalVerticalDrag,
                          juce::Slider::NoTextBox),owner(e),parameter(id),designBounds(bounds){
            setRange(0.0,1.0,0.0);
            setValue(owner.value(parameter),juce::dontSendNotification);
            setMouseDragSensitivity(180);
            setScrollWheelEnabled(true);
            setDoubleClickReturnValue(true,controlUI()[parameter].definition.initial);
            setWantsKeyboardFocus(false);
            onDragStart=[this]{gesture=true;owner.beginParameterGesture(parameter);};
            onValueChange=[this]{
                if(gesture)owner.performParameterGesture(parameter,getValue());
                else owner.change(parameter,getValue());
            };
            onDragEnd=[this]{
                if(gesture){owner.endParameterGesture(parameter);gesture=false;}
            };
        }
        void paint(juce::Graphics &) override {}
        void mouseDown(const juce::MouseEvent &e) override {
            if(e.mods.isRightButtonDown()){
                // MX Delay convention: a plain right click restores the declared
                // parameter default. Shift+right click keeps Jerzy MultiFX MIDI Learn.
                // Some Windows hosts omit keyboard modifiers from JUCE's
                // right-click snapshot while SHIFT is physically held.
                // Honor both JUCE modifiers and actual Windows key state.
                const bool learn=e.mods.isShiftDown() ||
                                 (GetAsyncKeyState(VK_SHIFT)&0x8000)!=0 ||
                                 (GetKeyState(VK_SHIFT)&0x8000)!=0;
                if(learn)owner.learnParameter(parameter);
                else{
                    const double initial=controlUI()[parameter].definition.initial;
                    owner.change(parameter,initial);
                    setValue(initial,juce::dontSendNotification);
                    repaint();
                }
                return;
            }
            juce::Slider::mouseDown(e);
        }
        void mouseUp(const juce::MouseEvent &e) override {
            if(e.mods.isRightButtonDown())return;
            juce::Slider::mouseUp(e);
        }
        void fit(float scaleX,float scaleY){
            setBounds(int(std::lround(designBounds.getX()*scaleX)),
                      int(std::lround(designBounds.getY()*scaleY)),
                      std::max(1,int(std::lround(designBounds.getWidth()*scaleX))),
                      std::max(1,int(std::lround(designBounds.getHeight()*scaleY))));
        }
        int parameterId() const {return parameter;}
    };
    // Route buttons are actual JUCE mouse targets, not merely painted areas.
    // This avoids relying on forwarding synthetic pointer messages from the
    // desktop child HWND to the hidden VST3 parent for this discrete control.
    class JuceRoutingButton final : public juce::Component {
        Editor &owner;
        int route;
    public:
        JuceRoutingButton(Editor &editor, int selected) : owner(editor), route(selected) {
            setInterceptsMouseClicks(true, false);
        }
        void paint(juce::Graphics &) override {} // The parent draws the hardware-style button.
        void mouseDown(const juce::MouseEvent &event) override {
            // Discrete routing selection must commit on mouse DOWN. Some VST3
            // hosts transfer capture before mouseUp, so waiting for mouseUp
            // silently discards a valid routing click.
            // A synthetic WM_LBUTTONDOWN from a VST3 host/test may not
            // update Windows' global VK_LBUTTON state. JUCE has already
            // dispatched mouseDown to this component, so do not discard it
            // merely because the global modifier snapshot says "up".
            if (!event.mods.isRightButtonDown())
                owner.change(layout().delayMode, double(route) / 2.0);
        }
    };
    std::unique_ptr<juce::ScopedJuceInitialiser_GUI> juceInitialiser;
    std::unique_ptr<JuceCanvas> juceCanvas;
    std::vector<std::unique_ptr<JuceParameterSlider>> juceParameterSliders;
    std::vector<std::unique_ptr<JuceRoutingButton>> juceRoutingButtons;
    juce::Graphics *juceGraphics=nullptr;
    MonoAnalogLook juceLook;
    juce::Slider drawingSlider;
    void fitJuceCanvas(int width,int height){
        if(!juceCanvas)return;
        // A JUCE desktop component attached to a foreign HWND inherits that
        // window's platform DPI. VST3 already gives us the final physical
        // viewport, so inheriting FL Studio's 125/150% DPI here would enlarge
        // the child HWND for a second time and clip its right and bottom edges.
        if(auto *peer=juceCanvas->getPeer())peer->setCustomPlatformScaleFactor(1.0);
        juceCanvas->setBounds(0,0,width,height);
        const float scaleX=float(width)/W,scaleY=float(height)/H;
        for(auto &slider:juceParameterSliders)slider->fit(scaleX,scaleY);
        for(int i=0;i<int(juceRoutingButtons.size());++i)
            juceRoutingButtons[i]->setBounds(int(std::lround(1007*scaleX)),
                                             int(std::lround((282+i*44)*scaleY)),
                                             std::max(1,int(std::lround(207*scaleX))),
                                             std::max(1,int(std::lround(36*scaleY))));

    }
    void rebuildJuceParameterSliders(){
        juceParameterSliders.clear();
        juceRoutingButtons.clear();
        // Do NOT delete the page button receiving this click. JUCE maintains
        // capture and drag bookkeeping until mouseUp, even after callbacks.
        if(!juceCanvas)return;
        auto add=[&](int id,bool horizontal,juce::Rectangle<int> bounds){
            if(id<0)return;
            auto slider=std::make_unique<JuceParameterSlider>(*this,id,horizontal,bounds);
            juceCanvas->addAndMakeVisible(*slider);
            slider->toFront(false);
            juceParameterSliders.push_back(std::move(slider));
        };
        if(referenceDelay()){
            const int bank=std::clamp(view.section,0,1);
            add(layout().digital[bank]+2,true,{866,177,298,23});
        }
        for(const auto &c:controls){
            const auto &definition=controlUI()[c.id].definition;
            if(isToggle(c.id)||!definition.choices.empty()||definition.steps>0)continue;
            const int y=c.y-scroll;
            if(referenceDelay()||(y>=controlTop&&y+c.h<=controlBottom))
                add(c.id,c.compact,{c.x,y,c.w,c.h});
        }
        RECT client{};GetClientRect(window,&client);
        const float scaleX=float(std::max(1L,client.right))/W;
        const float scaleY=float(std::max(1L,client.bottom))/H;
        for(auto &slider:juceParameterSliders)slider->fit(scaleX,scaleY);
        if(referenceDelay()){
            for(int i=0;i<3;++i){
                auto button=std::make_unique<JuceRoutingButton>(*this,i);
                button->setBounds(int(std::lround(1007*scaleX)),
                                  int(std::lround((282+i*44)*scaleY)),
                                  std::max(1,int(std::lround(207*scaleX))),
                                  std::max(1,int(std::lround(36*scaleY))));
                juceCanvas->addAndMakeVisible(*button);
                button->toFront(false);
                juceRoutingButtons.push_back(std::move(button));
            }
        }

    }
    void syncJuceParameterSliders(){
        for(auto &slider:juceParameterSliders){
            const double current=value(slider->parameterId());
            if(std::abs(slider->getValue()-current)>1.0e-7)
                slider->setValue(current,juce::dontSendNotification);
        }
    }
    static juce::Colour juceColor(COLORREF c){
        return juce::Colour::fromRGB(GetRValue(c),GetGValue(c),GetBValue(c));
    }
    void requestRepaint(){if(juceCanvas)juceCanvas->repaint();}
#else
    void requestRepaint(){if(window)InvalidateRect(window,nullptr,FALSE);}
#endif
    void capturePointer(){
#ifdef MULTIFX_JUCE_GUI
        // The JUCE peer already owns native mouse capture. Moving capture to
        // the hidden parent HWND makes JUCE synthesize mouseUp immediately,
        // which ends knob drags at the initial press.
        if(jucePointerDispatch)return;
#endif
        SetCapture(window);
    }
    void releasePointer(){
#ifdef MULTIFX_JUCE_GUI
        if(jucePointerDispatch)return;
#endif
        ReleaseCapture();
    }
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
        if (referenceDelay() || globalSettings || view.block != 2 || y < 548 || y >= 596 || x < 250 || x >= 1000)
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
        beginParameterGesture(id);
        performParameterGesture(id,v);
        endParameterGesture(id);
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
        if(raised){
            JerzyAudioUI::paintPanel(*juceGraphics,area,juceLook.getTheme(),4.0f);
            auto &texture=Gui3DAssets::get().module[Gui3DAssets::neutral];
            if(texture.isValid()){
                juce::Graphics::ScopedSaveState saved(*juceGraphics);
                juceGraphics->reduceClipRegion(area.getSmallestIntegerContainer());
                juceGraphics->setOpacity(.16f);
                juceGraphics->drawImage(texture,area);
            }
        }
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
#ifdef MULTIFX_JUCE_GUI
        const auto accent=Gui3DAssets::closestAccent(juceColor(tint));
        const auto &image=Gui3DAssets::get().leds[accent][on?1:0];
        if(image.isValid()){
            juceGraphics->drawImage(image,juce::Rectangle<float>(float(x-12),float(y-12),24.0f,24.0f));
            return;
        }
#endif
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
#ifdef MULTIFX_JUCE_GUI
    void miniVu(int x,int y,int w,int h,float amplitude){
        auto &assets=Gui3DAssets::get();
        const auto target=juce::Rectangle<float>(float(x),float(y),float(w),float(h));
        if(assets.vuFace.isValid())juceGraphics->drawImage(assets.vuFace,target);
        const float db=20.0f*std::log10(std::max(amplitude,1.e-6f));
        const float normalized=std::clamp((db+54.0f)/54.0f,0.0f,1.0f);
        const float angle=juce::degreesToRadians(-58.0f+116.0f*normalized);
        const float cx=x+w*.5f,cy=y+h*.89f,length=h*.72f;
        juceGraphics->setColour(juce::Colour(0xff49130b));
        juceGraphics->drawLine(cx,cy,cx+std::sin(angle)*length,cy-std::cos(angle)*length,2.2f);
        juceGraphics->setColour(juce::Colour(0xffff4e2e));
        juceGraphics->drawLine(cx,cy,cx+std::sin(angle)*length,cy-std::cos(angle)*length,1.0f);
        if(assets.vuGlass.isValid())juceGraphics->drawImage(assets.vuGlass,target);
    }
#endif

    int rackParameter(int block,int column) const {
        static constexpr const char *keys[5][3]={
            {"drive.gain","drive.tone","drive.level"},
            {"clone.rate","clone.depth","clone.mix"},
            {"delayA.time","delayA.feedback","delayA.mix"},
            {"reverb.size","reverb.damping","reverb.mix"},
            {"ir.mix","ir.output","ir.mix"}
        };
        if(block==2 && column<2) return delayControl(0,column);
        return parameterId(keys[std::clamp(block,0,4)][column]);
    }
#ifdef MULTIFX_JUCE_GUI
    void paintRackHome() {
        auto &g=*juceGraphics;
        auto &a=Gui3DAssets::get();
        const juce::Colour ink(0xffe4e5e7),muted(0xffa0a9b0),gold(0xfff9ac33);
        const juce::Colour accents[5]={juce::Colour(0xfff4a02c),juce::Colour(0xff26d3ee),
            juce::Colour(0xfff05b57),juce::Colour(0xffb778ed),juce::Colour(0xffa4c6cb)};
        const char *names[5]={"DRIVE","CHORUS","DELAY","REVERB","CAB / IR"};
        const char *labels[5][3]={{"GAIN","TONE","LEVEL"},{"RATE","DEPTH","MIX"},
            {"TIME","FEEDBACK","MIX"},{"SIZE","DAMP","MIX"},{"MIX","OUTPUT","MIX"}};
        const auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
        auto title=[&](juce::String text,int x,int y,int w,int h,float size,juce::Colour color) {
            g.setColour(color);g.setFont(size);
            g.drawFittedText(text,juce::Rectangle<int>(x,y,w,h),juce::Justification::centred,1);
        };
        auto panel=[&](juce::Rectangle<float> r,juce::Colour base,float rad) {
            g.setColour(juce::Colours::black.withAlpha(.85f));
            g.fillRoundedRectangle(r.translated(1,5),rad);
            g.setGradientFill(juce::ColourGradient(base.brighter(.22f),r.getTopLeft(),
                base.darker(.64f),r.getBottomRight(),false));
            g.fillRoundedRectangle(r,rad);
            g.setColour(juce::Colour(0xff767b82));g.drawRoundedRectangle(r,rad,1.15f);
        };
        auto bolt=[&](float x,float y) {
            g.setColour(juce::Colour(0xff08090b));g.fillEllipse(x-6,y-6,12,12);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd4d5d6),x-4,y-4,
                juce::Colour(0xff3a4045),x+4,y+4,false));g.fillEllipse(x-4,y-4,8,8);
            g.setColour(juce::Colour(0xff17191b));g.drawLine(x-2,y+2,x+2,y-2,1.5f);
        };
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b3038),0,0,
            juce::Colour(0xff050608),1280,720,false));g.fillAll();
        panel({10,12,1260,692},juce::Colour(0xff292c30),20);
        g.setColour(juce::Colour(0xff08090c));
        g.fillRoundedRectangle(juce::Rectangle<float>(23,91,1234,473),12);
        g.setColour(juce::Colour(0xff777a7e));
        g.drawRoundedRectangle(juce::Rectangle<float>(23,91,1234,473),12,1.7f);
        for(int xx:{29,1251})for(int yy:{28,687})bolt(float(xx),float(yy));
        title("Jerzy",41,25,112,46,35,ink);
        title("MultiFX",150,25,185,46,37,gold);
        title("MULTI EFFECTS PROCESSOR",340,34,209,30,13,muted);
        panel({559,28,330,47},juce::Colour(0xff17181b),7);
        title("Factory / Analog Space",569,34,309,34,19,ink);
        panel({902,28,146,47},juce::Colour(0xff1b1e21),7);
        title("PRESET",906,35,137,32,16,ink);
        panel({1059,28,194,47},juce::Colour(0xff30353d),7);
        title("SZCZEGOLY  >",1062,35,187,32,17,gold);
        // Two independent hardware-inspired meter and trim columns.
        for(int side=0;side<2;++side) {
            const int x=side?1090:31;
            const float v=side?needleOut:needleIn;
            const int id=side?layout().masterGain:parameterId("comp.input");
            panel({float(x),112,158,439},juce::Colour(0xff242426),12);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffd99b),float(x+12),130,
                juce::Colour(0xffb87829),float(x+147),273,false));
            g.fillRoundedRectangle(juce::Rectangle<float>(x+12,130,134,149),8);
            g.setColour(juce::Colour(0xff533413));
            g.drawRoundedRectangle(juce::Rectangle<float>(x+12,130,134,149),8,2);
            const float cx=x+79,cy=256;
            for(int t=0;t<=10;++t) {
                const float angle=juce::degreesToRadians(-68.0f+13.6f*t);
                g.setColour(juce::Colour(0xff4e2e18));
                g.drawLine(cx+std::sin(angle)*90,cy-std::cos(angle)*90,
                           cx+std::sin(angle)*79,cy-std::cos(angle)*79,1.25f);
            }
            title("VU",x+45,197,70,27,19,juce::Colour(0xff4e2912));
            const float db=20*std::log10(std::max(v,1.e-6f));
            const float needle=juce::degreesToRadians(-68.0f+136.0f*std::clamp((db+48)/48,0.0f,1.0f));
            g.setColour(juce::Colour(0xff562118));
            g.drawLine(cx,cy,cx+std::sin(needle)*82,cy-std::cos(needle)*82,2.5f);
            g.setColour(juce::Colour(0xff271411));g.fillEllipse(cx-5,cy-5,10,10);
            title(side?"OUTPUT":"INPUT",x+11,302,136,30,20,ink);
            const auto &strip=a.knobs[Gui3DAssets::master][Gui3DAssets::amber];
            if(strip.isValid()&&id>=0){
                const int fw=strip.getHeight(),frame=std::clamp(int(std::round(value(id)*100)),0,100);
                g.drawImage(strip,x+25,344,108,108,frame*fw,0,fw,strip.getHeight());
            }
            title(side?"MASTER OUTPUT":"COMP INPUT",x+10,457,138,26,12,muted);
            if(id>=0)title(juce::String(formatted(id)),x+9,483,139,25,14,gold);
            bolt(x+13,125);bolt(x+145,535);
        }
        for(int slot=0;slot<4;++slot){
            const int block=order[slot],x=199+slot*220;
            const juce::Colour accent=accents[block];
            panel({float(x),112,213,439},juce::Colour(0xff242629),11);
            g.setColour(juce::Colour(0xff090b0e));
            g.fillRoundedRectangle(juce::Rectangle<float>(x+5,118,203,426),8);
            g.setGradientFill(juce::ColourGradient(accent.darker(.46f),float(x+8),123,
                              juce::Colour(0xff181b20),float(x+203),202,false));
            g.fillRoundedRectangle(juce::Rectangle<float>(x+8,121,197,77),7);
            g.setColour(accent);
            g.drawRoundedRectangle(juce::Rectangle<float>(x+6,120,201,424),8,1.8f);
            title("FX "+juce::String(slot+1),x+9,132,49,29,17,accent);
            title(names[block],x+55,130,142,32,19,ink);
            g.setColour(value(layout().blockBypass[block])>=.5?juce::Colour(0xff743c40):accent);
            g.fillEllipse(x+17.0f,173,15,15);
            title("POWER",x+34,170,75,23,11,muted);
            title("EDIT >",x+132,170,66,23,12,accent);
            for(int k=0;k<3;++k){
                const int id=rackParameter(block,k);
                if(id<0)continue;
                const int xx=x+11+k*65;
                const auto &strip=a.knobs[Gui3DAssets::fx][Gui3DAssets::closestAccent(accent)];
                if(strip.isValid()){
                    const int fw=strip.getHeight(),frame=std::clamp(int(std::round(value(id)*100)),0,100);
                    g.drawImage(strip,xx,287,62,62,frame*fw,0,fw,strip.getHeight());
                }
                title(labels[block][k],xx-3,351,69,25,12,ink);
                title(juce::String(formatted(id)),xx-4,380,70,23,11,muted);
            }
            g.setColour(juce::Colour(0xff464c51));g.drawLine(x+17.0f,412,x+196.0f,412,1);
            panel({float(x+14),428,185,47},juce::Colour(0xff1e2124),6);
            const int selector=block==0?parameterId("drive.type"):block==1?parameterId("clone.model"):
                block==2?layout().delayType[0]:block==3?parameterId("reverb.engine"):-1;
            title(selector>=0?juce::String(formatted(selector)):"CAB / IR",x+18,435,178,31,14,ink);
            title("TYPE / MODEL     v",x+23,491,166,26,12,accent);
            bolt(x+15,127);bolt(x+199,127);bolt(x+15,536);bolt(x+199,536);
        }
        panel({37,582,1207,101},juce::Colour(0xff26282b),12);
        title("DELAY A/B",48,601,144,26,15,muted);
        const int route=std::clamp(int(std::round(value(layout().delayMode)*2)),0,2);
        for(int i=0;i<2;++i) {
            const int bx=202+i*166;
            panel({float(bx),603,155,51},route==i?juce::Colour(0xff71471b):juce::Colour(0xff1e2023),7);
            title(i==0?"SERIAL":"PARALLEL",bx+7,609,142,37,15,route==i?gold:ink);
        }
        title("CHAIN",556,608,111,29,16,muted);
        for(int i=0;i<4;++i){
            const int block=order[i],xx=672+i*138;
            panel({float(xx),599,112,57},juce::Colour(0xff21242a),7);
            title(juce::String(i+1)+"  "+names[block],xx+2,608,108,34,13,accents[block]);
            if(i<3)title(">",xx+108,608,28,34,15,ink);
        }
        title("CAB / IR + WSZYSTKIE PARAMETRY W SZCZEGOLACH",674,656,554,23,12,muted);
    }
#endif
    void chrome(HDC dc) {
#ifdef MULTIFX_JUCE_GUI
        JerzyAudioUI::paintChassis(*juceGraphics,juce::Rectangle<float>(0,0,float(W),float(H)),juceLook.getTheme());
#else
        fill(dc,0,0,W,H,RGB(7,17,27));
#endif
        metalPanel(dc,0,0,W,74);
        fill(dc,0,0,W,70,RGB(8,19,29));
        line(dc,0,73,W,73,RGB(38,68,83));
        text(dc,23,8,138,"JERZY",25,RGB(224,248,252));
        text(dc,157,8,190,"MULTI FX",25,RGB(46,196,235));
        text(dc,24,40,210,"BASS GUITAR EFFECTS STUDIO",10,RGB(124,172,193));
        text(dc,238,42,115,MULTIFX_VERSION,8,RGB(64,212,231));
        text(dc,365,7,100,"PRESET",10,RGB(199,221,232));
        bevel(dc,365,28,322,36,RGB(15,29,43));
        text(dc,371,34,18,"<",18,RGB(187,219,230));
        text(dc,668,34,18,">",18,RGB(187,219,230));
        const int preset=std::clamp(int(std::round(value(layout().factoryProgram)*7)),0,7);
        char presetLabel[96];
        snprintf(presetLabel,sizeof(presetLabel),"%02d   %s",preset+1,presetNames[preset]);
        text(dc,405,36,245,presetLabel,13,RGB(223,242,248));
        text(dc,640,34,22,"v",12,RGB(174,210,225));
        bevel(dc,696,28,56,36,RGB(15,29,43));
        text(dc,697,36,54,"ZAPISZ",10,RGB(194,226,237));
        text(dc,765,8,180,"MIDI MAPOWANIE",10,RGB(198,220,231));
        bevel(dc,765,28,144,36,RGB(8,30,39));
        bool mapped=false;
        for (int i=0;i<128;++i) if (value(layout().midiMap+i)>0) {mapped=true;break;}
        disk(dc,780,46,5,mapped?RGB(55,226,145):RGB(65,198,219));
        text(dc,790,36,111,mapped?"AKTYWNE":"MIDI LEARN",11,RGB(204,241,246));
        bevel(dc,920,28,162,36,RGB(14,32,47));
        text(dc,923,36,156,globalSettings||view.advanced?"WRÓĆ":"USTAWIENIA",11,RGB(204,231,240));
        bevel(dc,1091,28,164,36,RGB(14,32,47));
        text(dc,1094,36,158,"RACK HOME",11,RGB(204,231,240));
        metalPanel(dc,10,80,1260,72,false);
        fill(dc,13,83,1254,66,RGB(7,18,28));
        auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
        for(int i=0;i<5;++i) {
            const int b=order[i],x=20+i*216;
            const bool selected=b==view.block;
            metalPanel(dc,x,87,201,58);
            fill(dc,x+3,90,195,52,selected?RGB(22,43,55):RGB(16,30,40));
            if(selected) {
                line(dc,x-1,86,x+202,86,RGB(49,224,240),3);
                line(dc,x-1,86,x-1,146,RGB(15,95,112),2);
                line(dc,x+201,86,x+201,146,RGB(15,95,112),2);
            }
            char number[8];snprintf(number,sizeof(number),"%d",i+1);
            bevel(dc,x+9,101,27,29,RGB(30,48,64));
            text(dc,x+9,105,27,number,17,selected?RGB(61,227,244):RGB(226,240,245));
            const char* labels[]{"PREAMP / DRIVE","MODULACJA","DELAY A + B","REVERB","CAB / IR"};
            text(dc,x+44,103,143,labels[b],13,RGB(231,246,250));
            disk(dc,x+182,115,7,RGB(5,18,23));lamp(dc,x+182,115,true,RGB(43,233,135));
            const bool off=value(layout().blockBypass[b])>=.5;
            fill(dc,x+43,121,104,21,off?RGB(50,21,30):RGB(6,34,29));
            text(dc,x+47,123,98,off?"BYPASS":"AKTYWNY",10,off?RGB(244,130,141):RGB(78,244,162));
            toggleSwitch(dc,x+167,132,!off);
            if(i<4){line(dc,x+202,116,x+213,116,RGB(43,207,232),2);line(dc,x+208,111,x+214,116,RGB(43,207,232),2);line(dc,x+208,121,x+214,116,RGB(43,207,232),2);}
        }
        metalPanel(dc,1110,87,151,58);fill(dc,1113,90,145,52,RGB(8,19,28));
        const float levels[]{needleIn,needleOut};const char* meterNames[]{"IN","OUT"};
        for(int row=0;row<2;++row){
            const int y=99+row*22;const float db=20.f*std::log10(std::max(levels[row],1.e-6f));
#ifdef MULTIFX_JUCE_GUI
            text(dc,1115,y,25,meterNames[row],8,RGB(224,238,242));
            miniVu(1140,y-6,111,21,levels[row]);
#else
            const int lit=std::clamp(int((db+36.f)/2.5f),0,12);
            text(dc,1117,y,27,meterNames[row],9,RGB(190,219,229));
            for(int i=0;i<12;++i)fill(dc,1145+i*5,y+2,4,8,i<lit?(i>9?RGB(245,174,78):RGB(55,222,155)):RGB(27,52,65));
            char dbText[20];snprintf(dbText,sizeof(dbText),"%.0f",db);text(dc,1208,y,42,dbText,9,RGB(190,219,229));
#endif
        }
    }
    void referenceKnob(HDC dc, const Control &c, const char *label, int radius, COLORREF color) {
        // Draw the control from circles, arcs and strokes. Small radial marks and
        // brushed-metal hatching turn into isolated pixels at reduced host sizes.
        const float cx=float(c.x+c.w/2),cy=float(c.y+(radius==40?71:radius>=34?63:radius>=26?50:56));
        const float r=float(radius);
        text(dc,c.x,c.y,c.w,label,radius>=34?14:12,RGB(224,242,246));
#ifdef MULTIFX_JUCE_GUI
        const int diameter=int(std::round(r*2.65f));
        drawingSlider.setColour(juce::Slider::rotarySliderFillColourId,juceColor(color));
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
        const int readoutY=c.y+(radius==40?121:radius>=34?109:radius>=26?88:87);
        const int readoutW=std::min(c.w-4,radius*2+58);
        bevel(dc,cx-readoutW/2,readoutY-3,readoutW,22,RGB(5,18,27));
        text(dc,cx-readoutW/2,readoutY-2,readoutW,reading.c_str(),radius>=34?13:12,
             RGB(112,231,240));
    }
    void referenceDelayPanel(HDC dc) {
        metalPanel(dc,11,160,1258,548); fill(dc,14,163,1252,542,RGB(11,25,36));
        screw(dc,21,170); screw(dc,1259,170); screw(dc,21,699); screw(dc,1259,699);
        text(dc,29,170,285,"DELAY A + B",26,RGB(230,250,252));
        text(dc,321,178,390,"DWA NIEZALEŻNE SILNIKI OPÓŹNIENIA",11,RGB(132,178,195));
        const int bank=std::clamp(view.section,0,1),mix=layout().digital[bank]+2;
        text(dc,823,177,48,"MIX",13,RGB(225,245,249)); metalPanel(dc,866,180,298,14,false);
        fill(dc,868,184,294,6,RGB(2,9,15)); fill(dc,868,184,int(294*value(mix)),6,RGB(43,210,235));
        disk(dc,868+int(294*value(mix))+1,190,10,RGB(2,9,15)); disk(dc,868+int(294*value(mix)),188,8,RGB(154,171,179));
        disk(dc,868+int(294*value(mix))-2,186,4,RGB(235,244,246));
        char mixText[24]; snprintf(mixText,sizeof(mixText),"%d%%",int(std::round(value(mix)*100))); text(dc,1170,174,70,mixText,14,RGB(232,248,250));
        const int type=int(plain(layout().delayType[bank],float(value(layout().delayType[bank]))));
        const int model=int(std::round(value(layout().mxAlgorithms[bank][0])*8));
        const char* names[]{"OIL DRUM","MX ANALOG","REEL / TAPE","DIGITAL","MEMORY","VOLANTE","OLIVERA","BRIG / BBD"};
        const int families[]{4,1,3,0,2,1,1,1},models[]{-1,-1,-1,-1,-1,0,2,4};
        const char* engine=names[0];
        for(int i=0;i<8;++i) if(type==families[i]&&(models[i]<0||model==models[i])&&(type!=1||i!=1||(model!=0&&model!=2&&model!=4))) engine=names[i];
        bevel(dc,35,207,206,34,RGB(14,39,53)); text(dc,43,214,182,engine,12,RGB(88,229,244)); text(dc,219,213,18,"v",13,RGB(183,212,225));
        text(dc,264,214,145,"AKTYWNY BANK",10,RGB(134,187,205));
        for(int b=0;b<2;++b){const int bx=412+b*132;fill(dc,bx,207,126,34,bank==b?RGB(14,112,137):RGB(18,39,53));text(dc,bx,215,126,b==0?"A — AKTYWNY":"B — AKTYWNY",11,bank==b?RGB(230,251,255):RGB(154,186,202));}
        const bool bypass=value(layout().slotBypass[bank])>=.5;
        fill(dc,1007,207,207,34,bypass?RGB(76,31,43):RGB(17,76,57)); text(dc,1007,215,207,bypass?"BYPASS / WYŁĄCZONY":"ON / AKTYWNY",11,RGB(226,247,244));
        line(dc,28,250,1251,250,RGB(31,68,88));
        const char* primaryLabels[]{"CZAS DELAYA","FEEDBACK","CZAS DELAYA","FEEDBACK"};
        for(size_t i=0;i<controls.size();++i){const auto &c=controls[i];if(c.y==260)referenceKnob(dc,c,primaryLabels[i%4],40,i<2?RGB(53,215,236):RGB(174,128,247));}
        text(dc,1010,263,211,"TRYB PRACY",13,RGB(217,236,243));
        const char* routes[]{"SZEREGOWY A > B","RÓWNOLEGŁY","MIX A / B"};
        const int routing=int(plain(layout().delayMode,float(value(layout().delayMode))));
        for(int i=0;i<3;++i){
            const int y=282+i*44;
#ifdef MULTIFX_JUCE_GUI
            const auto &button=Gui3DAssets::get().buttons[Gui3DAssets::cyan][routing==i?1:0];
            if(button.isValid())juceGraphics->drawImage(button,juce::Rectangle<float>(1007.0f,float(y),207.0f,36.0f));
            else metalPanel(dc,1007,y,207,36,false);
#else
            metalPanel(dc,1007,y,207,36,false);fill(dc,1010,y+3,201,30,routing==i?RGB(23,124,153):RGB(15,36,52));
#endif
            lamp(dc,1023,y+18,routing==i,RGB(75,231,246));text(dc,1035,y+9,168,routes[i],11,RGB(223,240,246));
        }
        line(dc,28,415,1251,415,RGB(31,68,88));
        int featureCount=0;for(int i=0;i<8&&delayFeature(i)>=0;++i)++featureCount;
        const int groupCount=(featureCount+1)/2,totalWidth=groupCount*298+std::max(0,groupCount-1)*8;
        const int groupStart=(W-totalWidth)/2;
        const char* groups[5][4]={{"DIGITAL TONE","","",""},{"INPUT / MACHINE","MODULATION","EFFECTS","STEREO / DUCK"},
          {"TONE / DEPTH","MODULATION","LEVEL / BLEND",""},{"WOW / FLUTTER","TAPE AGE","SATURATION","WIDTH / NOISE"},
          {"FILTRACJA","MODULACJA","SATURACJA","STEREO / MECHANIKA"}};
        for(int group=0;group<groupCount;++group){const int gx=groupStart+group*306;metalPanel(dc,gx,422,298,159,false);fill(dc,gx+3,425,292,153,RGB(11,25,37));text(dc,gx+5,429,288,groups[std::clamp(type,0,4)][group],12,RGB(202,222,231));}
        const char* labels[5][8]={{"FILTER","LEVEL"},{"IN TRIM","OUT TRIM","MACHINE","PAN","FX AMOUNT","FX RATE","FX DEPTH","DUCK"},{"BOOST","SQUELCH","DEPTH","MOD RATE","LEVEL","BLEND"},{"WOW","FLUTTER","AGE","MOTOR","SATURATION","BIAS","WIDTH","HISS"},{"IN TONE","OUT TONE","DEPTH","SPEED","IN SAT","OUT SAT","WIDTH","DRIFT"}};
        for(const auto &stored:controls)if(stored.y==449)for(int i=0;i<featureCount;++i)if(delayFeature(i)==stored.id){
            const int group=i/2,local=i%2,gx=groupStart+group*306;
            Control c=stored;c.x=gx+27+local*135;c.w=130;
            referenceKnob(dc,c,labels[std::clamp(type,0,4)][i],featureCount<=2?34:26,i<2?RGB(64,212,231):RGB(173,126,236));break;
        }
        line(dc,28,607,1251,607,RGB(31,68,88));
        text(dc,35,621,676,"PRAWY KLIK: MIDI LEARN     •     PEŁNA LISTA PARAMETRÓW: USTAWIENIA",10,RGB(123,171,191));
        char status[96];snprintf(status,sizeof(status),"GR %.1f dB   /   PDC %d smp",controller->meterGR.load(),controller->meterLatency.load());
        text(dc,948,621,273,status,10,RGB(166,208,222));
        if(engineMenuOpen){
            metalPanel(dc,35,241,206,210,false);fill(dc,38,244,200,204,RGB(8,21,32));
            for(int i=0;i<8;++i){const int y=245+i*25;const bool selected=std::string_view(engine)==names[i];
                if(selected)fill(dc,39,y,198,24,RGB(14,84,104));
                text(dc,48,y+5,184,names[i],11,selected?RGB(104,236,247):RGB(202,222,231));
            }
        }
    }

    void rebuild() {
        if (!window)
            return;
        destroyCombos();
        controls.clear();
        if (rackHome) {
            controlPage=0;controlPages=1;
            const auto chain=permutation(int(plain(layout().order,float(value(layout().order)))));
            for(int slot=0;slot<4;++slot)
                for(int k=0;k<3;++k) {
                    const int id=rackParameter(chain[slot],k);
                    if(id>=0)controls.push_back({id,210+slot*220+k*65,287,62,62,false});
                }
            const int input=parameterId("comp.input");
            if(input>=0)controls.push_back({input,56,344,108,108,false});
            controls.push_back({layout().masterGain,1115,344,108,108,false});
        } else if (referenceDelay()) {
            controlPage=0;controlPages=1;
            for (int bank = 0; bank < 2; ++bank)
                for (int kind = 0; kind < 2; ++kind) {
                    const int id = delayControl(bank, kind);
                    if (id >= 0) controls.push_back({id, 45 + bank * 470 + kind * 200, 260, 150, 145, false});
                }
            int featureCount=0;for(int i=0;i<8&&delayFeature(i)>=0;++i)++featureCount;
            const int groupCount=(featureCount+1)/2,totalWidth=groupCount*298+std::max(0,groupCount-1)*8;
            const int groupStart=(W-totalWidth)/2;
            for (int i = 0; i < featureCount; ++i) {
                const int id = delayFeature(i);
                if (id >= 0) controls.push_back({id, groupStart+(i/2)*306+27+(i%2)*135, 449, 130, 120, false});
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
                int x=28+(local%3)*411,y=228+(local/3)*132;
                if(grouped){
                    const int count=isCompact?visibleCompact:visibleKnobs;
                    const int inRow=isCompact?local:local-visibleCompact;
                    x=28+(3-count)*205+inRow*411;
                    y=isCompact?(visibleKnobs?238:280):(visibleCompact?370:280);
                }
                controls.push_back({id,x,y,388,isCompact?54:126,isCompact});
            }
        }
        if(rackHome) {
#ifdef MULTIFX_JUCE_GUI
            rebuildJuceParameterSliders();
#endif
            signature=selectionSignature();
            requestRepaint();
            return;
        }
        for (auto &c : controls) {
            const auto &p = controlUI()[c.id].definition;
            if (p.choices.empty() && !(p.steps > 1 && p.steps <= 64))
                continue;
            const int yy = c.y - scroll;
            if (yy < controlTop || yy + 55 > controlBottom)
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
#ifdef MULTIFX_JUCE_GUI
        rebuildJuceParameterSliders();
#endif
        HWND factory = referenceDelay() ? nullptr : CreateWindowA("COMBOBOX", "", WS_CHILD | CBS_DROPDOWNLIST, int(405 / sx),
                                     int(34 / sy), int(245 / sx), 260, window,
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
#ifdef MULTIFX_JUCE_GUI
        if(rackHome) {
            paintRackHome();
            finish();
            return;
        }
#endif
        if (referenceDelay()) {
            chrome(target);
            referenceDelayPanel(target);
            finish();
            return;
        }
        HDC dc=target;
        chrome(dc);
        text(dc,24,157,1190,"WYBIERZ BLOK I SEKCJĘ, POTEM USTAW PARAMETRY    •    PRAWY KLIK: MIDI LEARN",11,RGB(169,211,225));
        auto sections = sectionNames(view.block);
        for (size_t i = 0; i < sections.size(); ++i) {
            int x = 28 + int(i) * 214;
            fill(dc, x, 177, 205, 34, int(i) == view.section ? RGB(32,106,125) : RGB(20,42,59));
            text(dc, x, 184, 205, sections[i].c_str(), 14,
                 int(i) == view.section ? RGB(238, 252, 255) : RGB(157, 187, 202));
        }
        if (view.block == 2 && view.advanced) {
            fill(dc, 900, 177, 160, 34, RGB(28, 59, 77));
            text(dc, 900, 184, 160, "MX A / B", 13, RGB(221,244,250));
        }
        fill(dc, 1075, 177, 175, 34, RGB(28, 59, 77));
        text(dc, 1075, 184, 175, view.advanced ? "BASIC" : "USTAWIENIA", 13, RGB(221,244,250));
        fill(dc, 20, 218, 1240, 302, RGB(16,31,47));
        line(dc,20,218,1260,218,RGB(37,85,105));
        bevel(dc, 24, 528, 1232, 82, RGB(15,33,49));
        if (view.block == 2 && !globalSettings) {
            const char *labels[]{"MIDI LEARN / TIME", "MIDI LEARN / FEEDBACK", "MIDI LEARN / MIX"};
            for (int i = 0; i < 3; ++i) {
                bevel(dc, 250 + i * 250, 545, 230, 38, RGB(24, 58, 72));
                text(dc, 255 + i * 250, 555, 220, labels[i], 12, RGB(219, 244, 250));
            }
            text(dc, 250, 586, 740, "To samo CC steruje wybranym silnikiem DELAY", 10, RGB(158, 197, 211));
        }
        if (view.block == 2 && view.advanced && !globalSettings)
            text(dc,35,555,200,view.mxSlot == 0 ? "MX ENGINE A" : "MX ENGINE B",15,RGB(226,245,251));
        for (int x : {30,1248}) for(int y : {228,603}) {
            disk(dc,x,y,5,RGB(198,199,184));
            line(dc,x-3,y,x+3,y,RGB(50,51,46));
        }
        if (view.block == 1 && !globalSettings) {
            if (view.section == 1) {

                text(dc, 250, 545, 260, "ATTACK / DECAY", 22, RGB(226, 245, 251));
                fill(dc, 525, 545, 180, 32, RGB(55, 61, 62));
                text(dc, 525, 552, 180, "TRIGGER", 14, RGB(238, 225, 198));
            } else
                text(dc, 250, 545, 340, view.section == 0 ? "CLONE COLLECTION" : "ATOMIC CLUSTER", 22,
                     RGB(226,245,251));
        }
        if (effectBypass() >= 0 && !globalSettings) {
            const int cx = view.section == 1 ? 1080 : W / 2, cy = 570;
            toggleSwitch(dc,cx,cy,value(effectBypass())<.5,2);
            text(dc, cx - 110, 592, 220, value(effectBypass()) >= .5 ? "BYPASS / OFF" : "EFFECT ON", 11,
                 RGB(232,221,190));
            lamp(dc,cx,cy-37,true,value(effectBypass()) >= .5 ? RGB(241,81,99) : RGB(65,236,143));
        }
        if (!globalSettings && view.block == 0 && view.section == 2 && value(layout().fuzzModel) * 19 >= 5.5)
            text(dc, 248, 585, 990,
                 "Circuit models use the original nonlinear solver and require much more CPU. Legacy models "
                 "are lighter for live use.",
                 14, RGB(232,221,190));
        if (globalSettings)
            text(dc, 248, 570, 990,
                 "The audio device and buffer are configured in your DAW. Effect latency is reported to the "
                 "host.",
                 15, RGB(232,221,190));
        if (view.block == 4 && !controller->fileStatus.empty())
            text(dc, 248, 585, 990, controller->fileStatus.c_str(), 13, RGB(222,245,250));
        char status[96];
        snprintf(status,sizeof(status),"GR %.1f dB / PDC %d smp",controller->meterGR.load(),controller->meterLatency.load());
        text(dc,30,533,320,status,10,RGB(166,208,222));
#ifdef MULTIFX_JUCE_GUI
        juceGraphics->saveState();
        juceGraphics->reduceClipRegion(20,controlTop,1240,controlBottom-controlTop);
#else
        canvas->PushAxisAlignedClip(rect(20,controlTop,1240,float(controlBottom-controlTop)),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
#endif
        for (auto &c : controls) {
            int y = c.y - scroll;
            if (y + c.h < controlTop || y > controlBottom)
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
        bevel(dc,1031,532,48,22,RGB(21,51,67));
        text(dc,1031,533,48,"<",14,controlPage>0?RGB(229,246,252):RGB(87,117,131));
        bevel(dc,1083,532,100,22,RGB(15,36,50));
        char pageLabel[32];snprintf(pageLabel,sizeof(pageLabel),"STRONA %d/%d",controlPage+1,controlPages);
        text(dc,1083,536,100,pageLabel,10,RGB(215,235,244));
        bevel(dc,1187,532,48,22,RGB(21,51,67));
        text(dc,1187,533,48,">",14,controlPage+1<controlPages?RGB(229,246,252):RGB(87,117,131));
        const char *buttons[]{"WCZYTAJ IR WAV", "WCZYTAJ PRESET", "ZAPISZ PRESET", "MASTER BYPASS"};
        buttons[3]=value(layout().bypass)>=.5 ? "MASTER WYŁĄCZONY" : "MASTER AKTYWNY";
        for (int i = 0; i < 4; ++i) {
            fill(dc, 28 + i * 250, 638, 238, 38, RGB(26,53,72));
            text(dc, 28 + i * 250, 648, 238, buttons[i], 12, RGB(226,248,252));
        }
        text(dc, 1028, 680, 225, "Prawy klik: MIDI Learn", 11, RGB(160, 191, 205));
        finish();
    }
    int hit(int x, int y) const {
        if (referenceDelay()) {
            for (const auto &c:controls)
                if (x>=c.x && x<c.x+c.w && y>=c.y && y<c.y+c.h) return c.id;
            return -1;
        }
        for (auto &c : controls)
            if (y >= controlTop && y < controlBottom && x >= c.x && x < c.x + c.w && y + scroll >= c.y && y + scroll < c.y + c.h)
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
        // Pointer coordinates must follow the *current* embedded host viewport.
        // FL Studio can resize the VST3 editor before the next paint, leaving
        // the cached paint transform stale and making GUI hit tests miss.
        if (window && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
                       msg == WM_RBUTTONUP || msg == WM_MOUSEMOVE ||
                       msg == WM_MOUSEWHEEL)) {
            RECT bounds{};
            if (GetClientRect(window, &bounds) && bounds.right > 0 && bounds.bottom > 0) {
                sx = float(W) / float(bounds.right);
                sy = float(H) / float(bounds.bottom);
            }
        }
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
                juce::Image image(juce::Image::ARGB,width,height,true,juce::SoftwareImageType{});
                {
                    juce::Graphics graphics(image);
                    paint(nullptr,&graphics);
                }
                std::vector<DWORD> pixels(size_t(width)*size_t(height));
                for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                    const auto c=image.getPixelAt(x,y);
                    pixels[size_t(y)*size_t(width)+size_t(x)]=RGB(c.getBlue(),c.getGreen(),c.getRed());
                }
                BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
                info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
                StretchDIBits(dc,0,0,width,height,0,0,width,height,
                    pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
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
            fitJuceCanvas(LOWORD(lp),HIWORD(lp));
#endif
            if (window)
                PostMessage(window, WM_APP + 1, 0, 0);
            return 0;
        case WM_APP + 1:
            rebuild();
            return 0;
        case WM_APP + 30: {
            const int next=std::clamp(controlPage+(w?1:-1),0,controlPages-1);
            if(next!=controlPage){controlPage=next;rebuild();}
            return 0;
        }
        case WM_APP + 31:
            // Test-only, read-only window diagnostics.
            return MAKELRESULT(controlPage,controlPages);
        case WM_APP + 32:
            // Test-only current section: low word block ID, high word
            // reference-delay flag plus section index.
            return MAKELRESULT(view.block,(referenceDelay()?1:0)|(view.section<<1));
#ifdef MULTIFX_JUCE_GUI
        case WM_APP + 29:
            if(juceCanvas)
                if(auto *peer=juceCanvas->getPeer())
                    if(const auto custom=peer->getCustomPlatformScaleFactor())
                        return LRESULT(std::lround(*custom*1000.0));
            return 0;
#endif
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
#ifdef MULTIFX_JUCE_GUI
            syncJuceParameterSliders();
#endif
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
            if(rackHome){
                const auto chain=permutation(int(plain(layout().order,float(value(layout().order)))));
                if(y>=28&&y<78&&x>=1059){rackHome=false;rebuild();return 0;}
                if(y>=120&&y<544&&x>=199&&x<1078){
                    const int slot=std::clamp((x-199)/220,0,3),block=chain[slot];
                    const int local=x-(199+slot*220);
                    if(y>=169&&y<=198&&local<110){
                        const int id=layout().blockBypass[block];
                        change(id,value(id)>=.5?0:1);return 0;
                    }
                    if(y>=423&&y<=524){
                        const int id=block==0?parameterId("drive.type"):
                            block==1?parameterId("clone.model"):
                            block==2?layout().delayType[0]:
                            block==3?parameterId("reverb.engine"):-1;
                        if(id>=0) {
                            const auto &d=controlUI()[id].definition;
                            const int steps=std::max(1,!d.choices.empty()?
                                int(std::count(d.choices.begin(),d.choices.end(),'|')):d.steps);
                            const int index=std::clamp(int(std::lround(value(id)*steps)),0,steps);
                            change(id,double((index+1)%(steps+1))/steps);
                        }else{view.block=block;rackHome=false;rebuild();}
                        return 0;
                    }
                    if(y<285){view.block=block;view.section=0;view.advanced=false;
                        rackHome=false;globalSettings=false;rebuild();return 0;}
                }
                if(y>=600&&y<=655&&x>=202&&x<523){
                    if(x<357)change(layout().delayMode,0);
                    else if(x>=368)change(layout().delayMode,.5);
                    return 0;
                }
                if(y>=600&&y<=655&&x>=672&&x<1215){
                    const int slot=std::clamp((x-672)/138,0,3);
                    // Arrows physically swap neighbouring blocks in the actual
                    // five-block DSP permutation, not merely their labels.
                    if(slot<3 && x>=672+slot*138+108){
                        auto swapped=chain;
                        std::swap(swapped[slot],swapped[slot+1]);
                        auto current=std::array<int,5>{0,1,2,3,4};
                        int rank=0;
                        while(current!=swapped&&rank<119){
                            std::next_permutation(current.begin(),current.end());++rank;
                        }
                        change(layout().order,double(rank)/119);
                        rebuild();return 0;
                    }
                    view.block=chain[slot];view.section=0;rackHome=false;
                    globalSettings=false;rebuild();return 0;
                }
                return 0;
            }
#ifdef MULTIFX_JUCE_GUI
            if(!jucePointerDispatch)
#endif
            SetFocus(window);
            if (referenceDelay()) {
                const int bank=std::clamp(view.section,0,1);
                if(y>=28&&y<65){
                    if(x>=371&&x<402){change(layout().factoryProgram,std::max(0.0,value(layout().factoryProgram)-1./7));return 0;}
                    if(x>=650&&x<687){change(layout().factoryProgram,std::min(1.0,value(layout().factoryProgram)+1./7));return 0;}
                    if(x>=405&&x<650){HMENU menu=CreatePopupMenu();for(int i=0;i<8;++i)AppendMenuA(menu,MF_STRING,20000+i,presetNames[i]);POINT pt{int(405/sx),int(65/sy)};ClientToScreen(window,&pt);int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);if(choice>=20000&&choice<20008)change(layout().factoryProgram,double(choice-20000)/7);return 0;}
                    if(x>=696&&x<752){file("savePreset",true);return 0;}
                    if(x>=920&&x<1082){view.advanced=true;rebuild();return 0;}
                    if(x>=1091&&x<1255){MessageBoxA(window,"Choose a block, then adjust its controls. Right-click any control for MIDI Learn. Use SETTINGS for all parameters.","Jerzy MultiFX",MB_OK|MB_ICONINFORMATION);return 0;}
                }
                if(y>=87&&y<146&&x>=20&&x<1100){
                    const int at=std::clamp((x-20)/216,0,4),local=x-(20+at*216);
                    auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
                    if(local>=135){const int id=layout().blockBypass[order[at]];change(id,value(id)>=.5?0:1);return 0;}
                    reorder=at;reorderHorizontal=true;view.block=order[at];view.section=0;scroll=0;controlPage=0;rebuild();capturePointer();return 0;
                }
                if(y>=180&&y<197&&x>=866&&x<1163){const int id=layout().digital[bank]+2;change(id,double(x-868)/294);drag=id;startX=x;startY=y;startValue=value(id);capturePointer();return 0;}
                if(engineMenuOpen){
                    if(x>=35&&x<241&&y>=245&&y<445){
                        const int choice=std::clamp((y-245)/25,0,7);
                        const int fam[]{4,1,3,0,2,1,1,1},mod[]{-1,-1,-1,-1,-1,0,2,4};
                        change(layout().delayType[bank],double(fam[choice])/4);
                        if(mod[choice]>=0)change(layout().mxAlgorithms[bank][0],double(mod[choice])/8);
                        if(value(layout().digital[bank]+2)<.01)change(layout().digital[bank]+2,.5);
                        engineMenuOpen=false;rebuild();return 0;
                    }
                    engineMenuOpen=false;requestRepaint();
                }
                if(y>=207&&y<242){
                    if(x>=35&&x<241){engineMenuOpen=true;requestRepaint();return 0;}
                    if(x>=412&&x<670){view.section=x<538?0:1;rebuild();return 0;}
                    if(x>=1007&&x<1214){const int id=layout().slotBypass[bank];change(id,value(id)>=.5?0:1);return 0;}
                }
                if(y>=282&&y<414&&x>=1007&&x<1214){const int route=(y-282)/44;if(route<3)change(layout().delayMode,double(route)/2);return 0;}
                const int id=hit(x,y);if(id>=0){drag=id;startX=x;startY=y;startValue=value(id);capturePointer();return 0;}
                return 0;
            }
            if (const int id = delayMidiTarget(x, y); id >= 0) {
                change(layout().midiLearn, double(id + 1) / layout().controlCount);
                return 0;
            }
            if (view.block == 1 && !globalSettings && y >= 545 && y <= 610) {
                const int center = view.section == 1 ? 1080 : W / 2;
                if (x >= center - 40 && x <= center + 40) {
                    const int id = effectBypass();
                    change(id, value(id) >= .5 ? 0 : 1);
                    return 0;
                }
                if (view.section == 1 && x >= 525 && x <= 705) {
                    change(layout().attackTrigger, 1);
                    SetTimer(window, 2, 120, nullptr);
                    return 0;
                }
            }
            if (y >= 28 && y < 65 && x >= 371 && x < 402) {
                change(layout().factoryProgram,std::max(0.0,value(layout().factoryProgram)-1./7));return 0;
            }
            if (y >= 28 && y < 65 && x >= 650 && x < 687) {
                change(layout().factoryProgram,std::min(1.0,value(layout().factoryProgram)+1./7));return 0;
            }
            if(y>=28&&y<65&&x>=1091&&x<1255){rackHome=true;rebuild();return 0;}
            if (y >= 28 && y < 65 && x >= 696 && x < 752) {file("savePreset",true);return 0;}
            if(y>=28&&y<65&&x>=405&&x<650){showChoice(layout().factoryProgram,405,65);return 0;}
            if (y >= 28 && y < 65 && x >= 920 && x < 1082) {
                if(view.block==2)view.advanced=!view.advanced;
                else globalSettings=!globalSettings;
                scroll = 0;controlPage=0;
                rebuild();
                return 0;
            }
            if (y >= 87 && y < 146 && x >= 20 && x < 1100) {
                int at = std::clamp((x - 20) / 216, 0, 4);
                auto order = permutation(int(plain(layout().order, float(value(layout().order)))));
                if (x - (20 + at * 216) >= 135) {const int id=layout().blockBypass[order[at]];change(id,value(id)>=.5?0:1);return 0;}
                if (y >= 122)
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
                    capturePointer();
                }
                return 0;
            }
            if (y >= 177 && y < 211) {
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
            if(y>=532&&y<555&&x>=1031&&x<1235){
                if(x<1079&&controlPage>0)--controlPage;
                if(x>=1187&&controlPage+1<controlPages)++controlPage;
                rebuild();
                return 0;
            }
            if (y >= 638 && x >= 28 && x < 1028) {
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
            if (y < controlTop || y > controlBottom)
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
                capturePointer();
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
                int dest = reorderHorizontal ? std::clamp((x - 20) / 216, 0, 4)
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
            releasePointer();
            return 0;
        case WM_RBUTTONUP: {
            if(referenceDelay()){
                int id=-1;
                const int bank=std::clamp(view.section,0,1);
                if(y>=87&&y<146&&x>=20&&x<1100){
                    const int at=std::clamp((x-20)/216,0,4);
                    if(x-(20+at*216)>=135){auto order=permutation(int(plain(layout().order,float(value(layout().order)))));id=layout().blockBypass[order[at]];}
                }
                if(y>=180&&y<197&&x>=866&&x<1163)id=layout().digital[bank]+2;
                if(y>=207&&y<242&&x>=1007&&x<1214)id=layout().slotBypass[bank];
                if(id<0)id=hit(x,y);
                for(int b=0;b<2;++b)for(int kind=0;kind<2;++kind)
                    if(id==delayControl(b,kind))id=parameterId(std::string(b?"delayB.":"delayA.")+(kind?"feedback":"time"));
                if(id>=0)change(layout().midiLearn,double(id+1)/layout().controlCount);
                return 0;
            }
            int id = delayMidiTarget(x, y);
            if (id < 0) id = hit(x, y);
            if (view.block == 1 && y >= 545 && y <= 610) {
                const int center = view.section == 1 ? 1080 : W / 2;
                if (std::abs(x - center) < 60)
                    id = effectBypass();
            }
            if (y >= 122 && y < 146 && x >= 20 && x < 1100) {
                auto order=permutation(int(plain(layout().order,float(value(layout().order)))));
                const int at=std::clamp((x-20)/216,0,4);
                if(x-(20+at*216)>=135)id=layout().blockBypass[order[at]];
            }
            if (y >= 638 && y < 676 && x >= 778 && x < 1016) id=layout().bypass;
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
        Steinberg::ViewRect r(0, 0, DefaultW, DefaultH);
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
        juceCanvas->addToDesktop(0,window);
        fitJuceCanvas(getRect().getWidth(),getRect().getHeight());
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
            juceParameterSliders.clear();
            juceRoutingButtons.clear();
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
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect *r) override {
        if (!r)
            return Steinberg::kInvalidArgument;
        int width=std::clamp(((r->getWidth()+8)/16)*16,640,1920);
        const int height=width*9/16;
        r->right=r->left+width;
        r->bottom=r->top+height;
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect *r) override {
        if (!r || r->getWidth() < 640 || r->getHeight() < 360)
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
