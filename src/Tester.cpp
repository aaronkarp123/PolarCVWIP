#include <string.h>
#include <math.h>
#include "plugin.hpp"


static const int BUFFER_SIZE = 1024;//256;

Vec polarToCart(Vec point){
    Vec new_point;
    new_point.x = point.y * sin(point.x) / 2.0f;
    new_point.y = point.y * cos(point.x) / 2.0f;
    return new_point;
}

struct Tester : Module {
    enum ParamId {
        SPEED_MULT_PARAM,
        PREV_EQ_PARAM,
        NEXT_EQ_PARAM,
        SPEED_DIAL_PARAM,
        A_DIAL_PARAM,
        B_DIAL_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        R_OUT_OUTPUT,
        X_OUT_OUTPUT,
        Y_OUT_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        SPEED_MULT_LIGHT,
        PREV_EQ_LIGHT,
        NEXT_EQ_LIGHT,
        LIGHTS_LEN
    };
    
    Vec current_cursor;
    float cur_theta = 0.0f;
    float theta_delta = 0.001f;
    int current_equation = 0;
    bool prev_trig = false;
    bool double_time = false;
    
    struct Point {
        float minX[16] = {};
        float maxX[16] = {};
        float minY[16] = {};
        float maxY[16] = {};

        Point() {
            for (int c = 0; c < 16; c++) {
                minX[c] = INFINITY;
                maxX[c] = -INFINITY;
                minY[c] = INFINITY;
                maxY[c] = -INFINITY;
            }
        }
    };
    
    Point pointBuffer[BUFFER_SIZE];
    Point currentPoint;
    

    Tester() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configSwitch(SPEED_MULT_PARAM, 0.f, 1.f, 0.f, "");
        configSwitch(PREV_EQ_PARAM, 0.f, 1.f, 0.f, "");
        configSwitch(NEXT_EQ_PARAM, 0.f, 1.f, 0.f, "");
        configParam(SPEED_DIAL_PARAM,  0.0001f, 0.02f, 0.0001f, "Speed", " screen/PI", 0.f, 0.0001f);
        configParam(A_DIAL_PARAM, 0, 20, 0, "A", " V", 0.f, 0.5f);
        configParam(B_DIAL_PARAM, 0, 20, 0, "B", " V", 0.f, 0.5f);
        getParamQuantity(A_DIAL_PARAM)->snapEnabled = true;
        getParamQuantity(B_DIAL_PARAM)->snapEnabled = true;
        configOutput(R_OUT_OUTPUT, "R");
        configOutput(X_OUT_OUTPUT, "X");
        configOutput(Y_OUT_OUTPUT, "Y");
    }
    
    void process(const ProcessArgs& args) override {
        double_time = params[SPEED_MULT_PARAM].getValue();

        bool trig = !params[NEXT_EQ_PARAM].getValue();
        
        if (trig != prev_trig){
            current_equation = (current_equation + 1) % 4;
        }
        prev_trig = trig;

        //current Carts
        Vec carts = polarToCart(current_cursor);
        
        outputs[R_OUT_OUTPUT].setVoltage(current_cursor.x*20.0f);
        outputs[X_OUT_OUTPUT].setVoltage(carts.x*20.0f);
        outputs[Y_OUT_OUTPUT].setVoltage(carts.y*20.0f);
    
        updateTheta();
    }
    
    void updateTheta() {
        
        if (double_time){
            theta_delta = params[SPEED_DIAL_PARAM].getValue() * M_PI / 100.0f;
        }
        else{
            theta_delta = params[SPEED_DIAL_PARAM].getValue() * M_PI / 1000.0f;
        }
        cur_theta = cur_theta + theta_delta;
        
        float A = params[A_DIAL_PARAM].getValue() / 2.0f;
        float B = params[B_DIAL_PARAM].getValue() / 2.0f;
        
        float r;
        switch (current_equation) {
            case 0:
                r = sin(A * cur_theta) * cos(B * cur_theta);
                break;
            case 1:
                r = sin(A * cos(B * cur_theta));
                break;
            case 2:
                r = cos(A * cos(B * cur_theta));
                break;
            case 3:
                r = sin(A *cur_theta) * cos(B * cur_theta);
                break;
        }
        
        current_cursor.x = cur_theta;
        current_cursor.y = r;
    }

};

struct TesterDisplay : LedDisplay {
    Tester* module;
    ModuleWidget* moduleWidget;
    int statsFrame = 0;
    std::string fontPath;
    
    Vec current_points[BUFFER_SIZE] = {};
    int currentLayer = 0;
    float prev_settings[3] = {};

    struct Stats {
        float min = INFINITY;
        float max = -INFINITY;
    };
    Stats statsX;
    Stats statsY;

    TesterDisplay() {
        fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");
    }
    
    void drawEq(const DrawArgs& args, float A, float B, float active_t, Vec current_points[]) {
        if (!module)
            return;

        nvgSave(args.vg);
        Rect b = box.zeroPos().shrink(Vec(0, 15));
        nvgScissor(args.vg, RECT_ARGS(b));
        nvgBeginPath(args.vg);
        
        for (int i = 0; i < BUFFER_SIZE; i++){
            Vec p = current_points[i];
            p = b.interpolate(p);
            if (i == 0)
                nvgMoveTo(args.vg, p.x,  p.y);
            else
                nvgLineTo(args.vg, p.x,  p.y);
        }
        
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        nvgStrokeColor(args.vg, nvgRGB(0xde, 0xde, 0xff));
        nvgStrokeWidth(args.vg, 0.3f);
        nvgStroke(args.vg);
        nvgResetScissor(args.vg);
        nvgRestore(args.vg);
    }
    
    void drawCursor(const DrawArgs& args, float A, float B, float active_t, Vec current_cursor) {
        if (!module)
            return;

        nvgSave(args.vg);
        Rect b = box.zeroPos().shrink(Vec(0, 15));
        nvgScissor(args.vg, RECT_ARGS(b));
        nvgBeginPath(args.vg);
        
        Vec p = polarToCart(current_cursor);
        p.x += 0.5f;
        p.y += 0.5f;
        p = b.interpolate(p);
        nvgMoveTo(args.vg, p.x-0.008f,  p.y-0.008f);
        nvgLineTo(args.vg, p.x+0.008f,  p.y-0.008f);
        nvgLineTo(args.vg, p.x+0.008f,  p.y+0.008f);
        nvgLineTo(args.vg, p.x-0.008f,  p.y+0.008f);
        nvgMoveTo(args.vg, p.x-0.008f,  p.y-0.008f);
        nvgClosePath(args.vg);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        nvgStrokeColor(args.vg, nvgRGB(0xFF, 0x00, 0x00));
        nvgStrokeWidth(args.vg, 2.0f);
        nvgStroke(args.vg);
        //nvgFill(args.vg);
        nvgResetScissor(args.vg);
        nvgRestore(args.vg);
    }
    
    void drawStats(const DrawArgs& args, Vec pos, const char* title, const Stats& stats, float A, float B, int current_equation) {
        std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
        if (!font)
            return;
        nvgFontSize(args.vg, 13);
        nvgFontFaceId(args.vg, font->handle);
        nvgTextLetterSpacing(args.vg, -2);

        nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x40));
        nvgText(args.vg, pos.x + 6, pos.y + 11, title, NULL);

        nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x80));
        pos = pos.plus(Vec(22, 11));

        std::string text;
        text = "pp ";
        float pp = stats.max - stats.min;
        text += isNear(pp, 0.f, 100.f) ? string::f("% 6.2f", pp) : "  ---";
        nvgText(args.vg, pos.x, pos.y, text.c_str(), NULL);
        switch (current_equation) {
          case 0:
                text = std::to_string((floor((A*2)+0.5)/2)).substr(0, 3) + "sin(theta) + " + std::to_string((floor((B*2)+0.5)/2)).substr(0, 3) + "cos(theta)";
                break;
          case 1:
                text = "sin("+std::to_string((floor((A*2)+0.5)/2)).substr(0, 3)+"cos(" + std::to_string((floor((B*2)+0.5)/2)).substr(0, 3) + "theta))";
                break;
        case 2:
                text = "cos("+std::to_string((floor((A*2)+0.5)/2)).substr(0, 3)+"cos(" + std::to_string((floor((B*2)+0.5)/2)).substr(0, 3) + "theta))";
                break;
            case 3:
                    text = "sin("+std::to_string((floor((A*2)+0.5)/2)).substr(0, 3)+") * cos(" + std::to_string((floor((B*2)+0.5)/2)).substr(0, 3) + "theta)";
                    break;
        }
        nvgText(args.vg, pos.x + 58 * 1, pos.y, text.c_str(), NULL);
    }
    
    void drawBackground(const DrawArgs& args) {
        Rect b = box.zeroPos().shrink(Vec(0, 15));

        nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
        for (int i = 0; i < 5; i++) {
            nvgBeginPath(args.vg);

            Vec p;
            p.x = 0.0;
            p.y = float(i) / (5 - 1);
            nvgMoveTo(args.vg, VEC_ARGS(b.interpolate(p)));

            p.x = 1.0;
            nvgLineTo(args.vg, VEC_ARGS(b.interpolate(p)));
            nvgStroke(args.vg);
        }
    }
    
    void updatePoints(float A, float B, int current_equation){
        
        float theta = 0.00f;
        float r;
        float min_repeat_pi;
        switch (current_equation) {
            case 0:
                r = sin(A * theta) * cos(B * theta);
                min_repeat_pi  = 8.0f * M_PI;
                break;
            case 1:
                r = sin(A * cos(B * theta));
                min_repeat_pi  = 4.0f * M_PI;
                break;
            case 2:
                r = cos(A * cos(B * theta));
                min_repeat_pi  = 4.0f * M_PI;
                break;
            case 3:
                r = sin(A *theta) * cos(B * theta);
                min_repeat_pi  = 8.0f * M_PI;
                break;
                
        }
        //float min_repeat_pi = findLCM((2.0f/A + 2.0f/B)*M_PI, 2.0f*M_PI);
        
        
        for (int i = 0; i < BUFFER_SIZE; i++){
            Vec tempp;
            tempp.x = theta;
            tempp.y = r;
            Vec p = polarToCart(tempp);
            p.x += 0.5f;
            p.y += 0.5f;
            current_points [i] = p;
            
            theta = min_repeat_pi * (float) i / (BUFFER_SIZE - 1) * M_PI;
            switch (current_equation) {
                case 0:
                    r = sin(A * theta) * cos(B * theta);
                    break;
                case 1:
                    r = sin(A * cos(B * theta));
                    break;
                case 2:
                  r = cos(A * cos(B * theta));
                  break;
                case 3:
                    r = sin(A *theta) * cos(B * theta);
                    break;
            }
        }
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        
        if (layer != 1)
            return;
        
        currentLayer += 1;

        // Background lines
        //drawBackground(args);

        if (!module)
            return;

        float A = module->params[Tester::A_DIAL_PARAM].getValue() / 2.0f;
        float B = module->params[Tester::B_DIAL_PARAM].getValue() / 2.0f;
        
        
        if (currentLayer % 3 == 0){
            updatePoints(A, B, module->current_equation);
        }
        
        drawEq(args, A, B, module->cur_theta, current_points);
        drawCursor(args, A, B, module->cur_theta, module->current_cursor);
        
        drawStats(args, Vec(0, 0 + 1), "1", statsX, A, B, module->current_equation);
    }
    
};


struct TesterWidget : ModuleWidget {
    TesterWidget(Tester* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Tester.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        //addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.306, 86.87)), module, Tester::SPEED_MULT_PARAM));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(8.306, 86.87)), module, Tester::SPEED_MULT_PARAM, Tester::SPEED_MULT_LIGHT));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(33.005, 86.87)), module, Tester::PREV_EQ_PARAM, Tester::PREV_EQ_LIGHT));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(57.412, 86.87)), module, Tester::NEXT_EQ_PARAM, Tester::NEXT_EQ_LIGHT));
        //addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(33.005, 86.87)), module, Tester::PREV_EQ_PARAM));
        //addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(57.412, 86.87)), module, Tester::NEXT_EQ_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.643, 102.815)), module, Tester::SPEED_DIAL_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(41.147, 102.812)), module, Tester::A_DIAL_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(57.397, 102.812)), module, Tester::B_DIAL_PARAM));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.833, 119.111)), module, Tester::R_OUT_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.195, 119.092)), module, Tester::X_OUT_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(45.212, 119.111)), module, Tester::Y_OUT_OUTPUT));
        
        // mm2px(Vec(66.04, 66.04))
        TesterDisplay* display = createWidget<TesterDisplay>(mm2px(Vec(0.0, 13.039)));
        display->box.size = mm2px(Vec(66.04, 66.04));//mm2px(Vec(66.04, 55.88));
        display->module = module;
        display->moduleWidget = this;
        addChild(display);
    }
};


Model* modelTester = createModel<Tester, TesterWidget>("Tester");
