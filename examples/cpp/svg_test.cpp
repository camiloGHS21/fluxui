#include "fluxui/FluxUI.h"
#include <iostream>
#include <vector>

int main() {
    FluxUI::Application app;
    if (!app.init("FluxUI Native SVG Widget Parity Test", 1000, 700, FluxUI::RenderBackendType::Auto)) {
        return 1;
    }

    if (!app.renderer().loadDefaultFont(16.0f)) {
        app.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
    }
    app.renderer().warmFontCache(std::vector<float>{14.0f, 16.0f, 26.0f});
    app.renderer().releaseFontSources();
    
    app.addStylesheet(
        ".root { display: flex; flex-direction: column; background-color: #0f172a; padding: 40px; gap: 20px; align-items: center; }"
        ".title { font-size: 28px; font-weight: 700; color: #f8fafc; font-family: 'Segoe UI', Outfit, sans-serif; }"
        ".subtitle { font-size: 14px; color: #94a3b8; margin-bottom: 10px; }"
        ".svg-card { display: flex; flex-direction: column; background-color: #1e293b; border-radius: 16px; padding: 24px; border: 1px solid #334155; align-items: center; gap: 16px; }"
        ".button-container { display: flex; flex-direction: row; gap: 12px; margin-top: 10px; }"
        ".btn { width: 150px; height: 42px; border-radius: 8px; font-weight: 600; font-size: 14px; display: flex; justify-content: center; align-items: center; }"
        ".btn-primary { background-color: #3b82f6; color: #ffffff; }"
        ".btn-secondary { background-color: #10b981; color: #ffffff; }"
    );

    auto* root = app.root();
    root->add<FluxUI::Text>("Chromium Blink-Parity Native SVG Widgets", "title");
    root->add<FluxUI::Text>("High-performance, tree-based SVG DOM widgets with dynamic rasterization caching.", "subtitle");

    auto* card = root->element("div", "", "svg-card");

    auto* svgRoot = static_cast<FluxUI::Svg*>(card->element("svg"));
    svgRoot->setAttribute("width", "400");
    svgRoot->setAttribute("height", "300");
    svgRoot->setAttribute("viewBox", "0 0 400 300");
    svgRoot->setAttribute("preserveAspectRatio", "xMidYMid meet");
    
    svgRoot->css("width: 400px; height: 300px; background-color: #090d16; border-radius: 8px; border: 1px solid #475569;");

    auto* rect = static_cast<FluxUI::SvgRect*>(svgRoot->element("rect"));
    rect->setAttribute("x", "50");
    rect->setAttribute("y", "50");
    rect->setAttribute("width", "100");
    rect->setAttribute("height", "80");
    rect->setAttribute("rx", "10");
    rect->setAttribute("ry", "10");
    rect->setAttribute("fill", "#10b981");
    rect->setAttribute("stroke", "#f59e0b");
    rect->setAttribute("stroke-width", "4");

    auto* circle = static_cast<FluxUI::SvgCircle*>(svgRoot->element("circle"));
    circle->setAttribute("cx", "280");
    circle->setAttribute("cy", "90");
    circle->setAttribute("r", "40");
    circle->setAttribute("fill", "#ec4899");
    circle->setAttribute("stroke", "#ffffff");
    circle->setAttribute("stroke-width", "3");

    auto* group = static_cast<FluxUI::SvgG*>(svgRoot->element("g"));
    group->setAttribute("transform", "translate(50, 160)");

    auto* path = static_cast<FluxUI::SvgPath*>(group->element("path"));
    path->setAttribute("d", "M 0 0 L 100 0 L 50 80 Z");
    path->setAttribute("fill", "#6366f1");
    path->setAttribute("stroke", "#a5b4fc");
    path->setAttribute("stroke-width", "4");

    auto* line = static_cast<FluxUI::SvgLine*>(group->element("line"));
    line->setAttribute("x1", "150");
    line->setAttribute("y1", "40");
    line->setAttribute("x2", "250");
    line->setAttribute("y2", "40");
    line->setAttribute("stroke", "#f43f5e");
    line->setAttribute("stroke-width", "8");

    auto* polygon = static_cast<FluxUI::SvgPolygon*>(group->element("polygon"));
    polygon->setAttribute("points", "180,70 210,10 240,70");
    polygon->setAttribute("fill", "#eab308");

    auto* btnContainer = card->element("div", "", "button-container");
    
    auto* btnMutate = static_cast<FluxUI::Button*>(btnContainer->add<FluxUI::Button>("Mutate Shapes", "btn btn-primary"));
    btnMutate->onClick = [=]() {
        static bool toggled = false;
        toggled = !toggled;
        
        if (toggled) {
            rect->setAttribute("fill", "#f59e0b");
            rect->setAttribute("stroke", "#10b981");
            circle->setAttribute("cx", "300");
            circle->setAttribute("r", "50");
            group->setAttribute("transform", "translate(70, 160) scale(1.1)");
        } else {
            rect->setAttribute("fill", "#10b981");
            rect->setAttribute("stroke", "#f59e0b");
            circle->setAttribute("cx", "280");
            circle->setAttribute("r", "40");
            group->setAttribute("transform", "translate(50, 160)");
        }
        std::cout << "Mutated SVG attributes dynamically. Dirty-flag rasterization triggered." << std::endl;
    };

    auto* btnClose = static_cast<FluxUI::Button*>(btnContainer->add<FluxUI::Button>("Close Test", "btn btn-secondary"));
    btnClose->onClick = [&]() {
        app.running = false;
    };

    app.run();
    app.shutdown();
    return 0;
}
