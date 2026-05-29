#include "fluxui/FluxUI.h"
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    FluxUI::Application app;
    if (!app.init("FluxUI Native SVG Blink-Parity Engine Test", 1080, 750, FluxUI::RenderBackendType::Auto)) {
        return 1;
    }

    if (!app.renderer().loadDefaultFont(16.0f)) {
        app.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
    }
    app.renderer().warmFontCache(std::vector<float>{14.0f, 16.0f, 18.0f, 28.0f});
    app.renderer().releaseFontSources();
    
    app.addStylesheet(
        ".root { display: flex; flex-direction: column; background-color: #0b0f19; padding: 40px; gap: 24px; align-items: center; color: #f8fafc; }"
        ".title-container { display: flex; flex-direction: column; align-items: center; gap: 8px; }"
        ".title { font-size: 28px; font-weight: 700; color: #f8fafc; font-family: 'Segoe UI', Outfit, sans-serif; }"
        ".subtitle { font-size: 14px; color: #64748b; font-family: 'Segoe UI', sans-serif; }"
        ".container { display: flex; flex-direction: row; gap: 28px; width: 1000px; height: 520px; }"
        
        ".sidebar { display: flex; flex-direction: column; width: 280px; gap: 12px; background-color: #111827; border-radius: 16px; padding: 24px; border: 1px solid #1f2937; }"
        ".sidebar-title { font-size: 12px; font-weight: 700; color: #4b5563; text-transform: uppercase; letter-spacing: 0.1em; margin-bottom: 8px; font-family: 'Segoe UI', sans-serif; }"
        ".sidebar-btn { width: 232px; height: 42px; border-radius: 10px; font-weight: 600; font-size: 13px; display: flex; justify-content: flex-start; padding-left: 16px; align-items: center; background-color: #1f2937; color: #9ca3af; border: 1px solid transparent; cursor: pointer; transition: all 0.2s; }"
        ".sidebar-btn:hover { background-color: #374151; color: #f3f4f6; }"
        ".sidebar-btn-active { background-color: #2563eb; color: #ffffff; border: 1px solid #3b82f6; }"
        
        ".preview-area { display: flex; flex-direction: column; flex-grow: 1; background-color: #111827; border-radius: 16px; padding: 32px; border: 1px solid #1f2937; align-items: center; justify-content: center; gap: 20px; position: relative; }"
        ".svg-viewport { width: 500px; height: 380px; background-color: #030712; border-radius: 12px; border: 1px solid #1f2937; display: flex; align-items: center; justify-content: center; overflow: hidden; color: #ffffff; }"
        
        ".button-container { display: flex; flex-direction: row; gap: 12px; }"
        ".btn { width: 160px; height: 42px; border-radius: 10px; font-weight: 600; font-size: 14px; display: flex; justify-content: center; align-items: center; cursor: pointer; }"
        ".btn-primary { background-color: #2563eb; color: #ffffff; border: 1px solid #3b82f6; }"
        ".btn-primary:hover { background-color: #1d4ed8; }"
        ".btn-secondary { background-color: #1f2937; color: #9ca3af; border: 1px solid #374151; }"
        ".btn-secondary:hover { background-color: #374151; color: #f3f4f6; }"
    );

    auto* root = app.root();
    
    auto* titleContainer = root->element("div", "", "title-container");
    titleContainer->add<FluxUI::Text>("Blink-Parity Native SVG Dashboard", "title");
    titleContainer->add<FluxUI::Text>("High-fidelity SVG parsing and rasterization matching Chromium standards.", "subtitle");

    auto* container = root->element("div", "", "container");
    
    // Sidebar for selecting SVGs
    auto* sidebar = container->element("div", "", "sidebar");
    auto* labelSelect = sidebar->element("div", "Select SVG Source", "sidebar-title");
    
    std::vector<FluxUI::Button*> sidebarBtns;
    
    auto* btnD = static_cast<FluxUI::Button*>(sidebar->add<FluxUI::Button>("Dynamic SVG (Mutable)", "sidebar-btn sidebar-btn-active"));
    sidebarBtns.push_back(btnD);
    
    auto* btnT = static_cast<FluxUI::Button*>(sidebar->add<FluxUI::Button>("Classic Tiger SVG (Complex)", "sidebar-btn"));
    sidebarBtns.push_back(btnT);
    
    auto* btnG = static_cast<FluxUI::Button*>(sidebar->add<FluxUI::Button>("Font-Awesome Gear SVG", "sidebar-btn"));
    sidebarBtns.push_back(btnG);
    
    auto* btnS = static_cast<FluxUI::Button*>(sidebar->add<FluxUI::Button>("Font-Awesome Shield SVG", "sidebar-btn"));
    sidebarBtns.push_back(btnS);
    
    auto* btnH = static_cast<FluxUI::Button*>(sidebar->add<FluxUI::Button>("Font-Awesome Ghost SVG", "sidebar-btn"));
    sidebarBtns.push_back(btnH);
    
    auto* btnF = static_cast<FluxUI::Button*>(sidebar->add<FluxUI::Button>("Font-Awesome Firefox SVG", "sidebar-btn"));
    sidebarBtns.push_back(btnF);

    // Preview Area
    auto* previewArea = container->element("div", "", "preview-area");
    auto* viewport = previewArea->element("div", "", "svg-viewport");

    // 1. Dynamic programmatically built Svg Widget
    auto* dynamicSvgRoot = static_cast<FluxUI::Svg*>(viewport->element("svg"));
    dynamicSvgRoot->setAttribute("width", "500");
    dynamicSvgRoot->setAttribute("height", "380");
    dynamicSvgRoot->setAttribute("viewBox", "0 0 500 380");
    dynamicSvgRoot->setAttribute("preserveAspectRatio", "xMidYMid meet");
    
    // Shapes inside dynamic SVG
    auto* rect = static_cast<FluxUI::SvgRect*>(dynamicSvgRoot->element("rect"));
    rect->setAttribute("x", "40");
    rect->setAttribute("y", "40");
    rect->setAttribute("width", "110");
    rect->setAttribute("height", "90");
    rect->setAttribute("rx", "12");
    rect->setAttribute("ry", "12");
    rect->setAttribute("fill", "#10b981");
    rect->setAttribute("stroke", "#f59e0b");
    rect->setAttribute("stroke-width", "4");

    auto* circle = static_cast<FluxUI::SvgCircle*>(dynamicSvgRoot->element("circle"));
    circle->setAttribute("cx", "390");
    circle->setAttribute("cy", "80");
    circle->setAttribute("r", "45");
    circle->setAttribute("fill", "#ec4899");
    circle->setAttribute("stroke", "#ffffff");
    circle->setAttribute("stroke-width", "3");

    auto* group = static_cast<FluxUI::SvgG*>(dynamicSvgRoot->element("g"));
    group->setAttribute("transform", "translate(40, 180)");

    // Standard Polygon
    auto* polygon = static_cast<FluxUI::SvgPolygon*>(group->element("polygon"));
    polygon->setAttribute("points", "0,120 40,30 80,120");
    polygon->setAttribute("fill", "#eab308");
    polygon->setAttribute("stroke", "#ffffff");
    polygon->setAttribute("stroke-width", "2");

    // New S/s Smooth Cubic Bezier Path (W3C standard test)
    auto* smoothCubicPath = static_cast<FluxUI::SvgPath*>(group->element("path"));
    smoothCubicPath->setAttribute("d", "M 120 80 C 120 20, 180 20, 180 80 S 240 140, 240 80");
    smoothCubicPath->setAttribute("fill", "none");
    smoothCubicPath->setAttribute("stroke", "#a855f7");
    smoothCubicPath->setAttribute("stroke-width", "5");

    // New T/t Smooth Quadratic Bezier Path (W3C standard test)
    auto* smoothQuadPath = static_cast<FluxUI::SvgPath*>(group->element("path"));
    smoothQuadPath->setAttribute("d", "M 280 80 Q 320 20, 360 80 T 440 80");
    smoothQuadPath->setAttribute("fill", "none");
    smoothQuadPath->setAttribute("stroke", "#06b6d4");
    smoothQuadPath->setAttribute("stroke-width", "5");

    // Line shape
    auto* line = static_cast<FluxUI::SvgLine*>(group->element("line"));
    line->setAttribute("x1", "120");
    line->setAttribute("y1", "140");
    line->setAttribute("x2", "440");
    line->setAttribute("y2", "140");
    line->setAttribute("stroke", "#f43f5e");
    line->setAttribute("stroke-width", "6");

    // 2. File-based complex SVG Images
    auto* tigerImage = static_cast<FluxUI::Image*>(viewport->image("assets/tiger.svg"));
    tigerImage->visible = false;
    tigerImage->css("width: 500px; height: 380px; object-fit: contain;");

    auto* gearImage = static_cast<FluxUI::Image*>(viewport->image("assets/gear.svg"));
    gearImage->visible = false;
    gearImage->css("width: 200px; height: 200px; object-fit: contain; filter: drop-shadow(0 4px 12px rgba(0,0,0,0.5));");

    auto* shieldImage = static_cast<FluxUI::Image*>(viewport->image("assets/shield.svg"));
    shieldImage->visible = false;
    shieldImage->css("width: 200px; height: 200px; object-fit: contain; filter: drop-shadow(0 4px 12px rgba(0,0,0,0.5));");

    auto* ghostImage = static_cast<FluxUI::Image*>(viewport->image("assets/ghost.svg"));
    ghostImage->visible = false;
    ghostImage->css("width: 200px; height: 200px; object-fit: contain; filter: drop-shadow(0 4px 12px rgba(0,0,0,0.5));");

    auto* firefoxImage = static_cast<FluxUI::Image*>(viewport->image("assets/firefox.svg"));
    firefoxImage->visible = false;
    firefoxImage->css("width: 200px; height: 200px; object-fit: contain; filter: drop-shadow(0 4px 12px rgba(0,0,0,0.5));");

    // Bottom controls
    auto* btnContainer = previewArea->element("div", "", "button-container");
    
    auto* btnMutate = static_cast<FluxUI::Button*>(btnContainer->add<FluxUI::Button>("Mutate Shapes", "btn btn-primary"));
    btnMutate->onClick = [=]() {
        static bool toggled = false;
        toggled = !toggled;
        
        if (toggled) {
            rect->setAttribute("fill", "#f59e0b");
            rect->setAttribute("stroke", "#10b981");
            circle->setAttribute("cx", "410");
            circle->setAttribute("r", "50");
            group->setAttribute("transform", "translate(40, 180) scale(1.05)");
            smoothCubicPath->setAttribute("stroke", "#3b82f6");
            smoothQuadPath->setAttribute("stroke", "#ef4444");
        } else {
            rect->setAttribute("fill", "#10b981");
            rect->setAttribute("stroke", "#f59e0b");
            circle->setAttribute("cx", "390");
            circle->setAttribute("r", "45");
            group->setAttribute("transform", "translate(40, 180)");
            smoothCubicPath->setAttribute("stroke", "#a855f7");
            smoothQuadPath->setAttribute("stroke", "#06b6d4");
        }
        std::cout << "[Dashboard] Mutated shapes programmatically. Redraw triggered." << std::endl;
    };

    auto* btnClose = static_cast<FluxUI::Button*>(btnContainer->add<FluxUI::Button>("Close Dashboard", "btn btn-secondary"));
    btnClose->onClick = [&]() {
        app.running = false;
    };

    // Controller to manage visibility
    auto updateActiveButton = [&](size_t activeIdx) {
        for (size_t i = 0; i < sidebarBtns.size(); ++i) {
            if (i == activeIdx) {
                sidebarBtns[i]->className = "sidebar-btn sidebar-btn-active";
            } else {
                sidebarBtns[i]->className = "sidebar-btn";
            }
            sidebarBtns[i]->markStyleDirty();
        }
    };

    auto showSvg = [&](size_t index) {
        dynamicSvgRoot->visible = (index == 0);
        tigerImage->visible = (index == 1);
        gearImage->visible = (index == 2);
        shieldImage->visible = (index == 3);
        ghostImage->visible = (index == 4);
        firefoxImage->visible = (index == 5);
        
        btnMutate->visible = (index == 0);
        
        updateActiveButton(index);
        app.requestRedraw();
    };

    // Bind sidebar buttons
    btnD->onClick = [&]() { showSvg(0); };
    btnT->onClick = [&]() { showSvg(1); };
    btnG->onClick = [&]() { showSvg(2); };
    btnS->onClick = [&]() { showSvg(3); };
    btnH->onClick = [&]() { showSvg(4); };
    btnF->onClick = [&]() { showSvg(5); };

    int startIndex = 2;
    if (argc > 1) {
        try {
            startIndex = std::stoi(argv[1]);
        } catch (...) {}
    }
    showSvg(startIndex);
    app.run();
    app.shutdown();
    return 0;
}
