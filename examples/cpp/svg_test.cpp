// FluxUI Native SVG Blink-Parity Engine Test — modern HTML/Blink-named declarative DSL.
#include <fluxui/dsl.h>
#include <iostream>
#include <vector>
#include <memory>
using namespace fluxui;

int main(int argc, char* argv[]) {
    App app(1080, 750, "FluxUI Native SVG Blink-Parity Engine Test");
    app.addCSS(
        ".root { display: flex; flex-direction: column; background-color: #0b0f19; padding: 40px; gap: 24px; align-items: center; color: #f8fafc; }"
        ".title-container { display: flex; flex-direction: column; align-items: center; gap: 8px; }"
        ".title { font-size: 28px; font-weight: 700; color: #f8fafc; }"
        ".subtitle { font-size: 14px; color: #64748b; }"
        ".container { display: flex; flex-direction: row; gap: 28px; width: 1000px; height: 520px; }"
        ".sidebar { display: flex; flex-direction: column; width: 280px; gap: 12px; background-color: #111827; border-radius: 16px; padding: 24px; border: 1px solid #1f2937; }"
        ".sidebar-title { font-size: 12px; font-weight: 700; color: #4b5563; text-transform: uppercase; letter-spacing: 0.1em; margin-bottom: 8px; }"
        ".sidebar-btn { width: 232px; height: 42px; border-radius: 10px; font-weight: 600; font-size: 13px; display: flex; justify-content: flex-start; padding-left: 16px; align-items: center; background-color: #1f2937; color: #9ca3af; border: 1px solid transparent; cursor: pointer; }"
        ".sidebar-btn:hover { background-color: #374151; color: #f3f4f6; }"
        ".sidebar-btn-active { background-color: #2563eb; color: #ffffff; border: 1px solid #3b82f6; }"
        ".preview-area { display: flex; flex-direction: column; flex-grow: 1; background-color: #111827; border-radius: 16px; padding: 32px; border: 1px solid #1f2937; align-items: center; justify-content: center; gap: 20px; position: relative; }"
        ".svg-viewport { width: 500px; height: 380px; background-color: #030712; border-radius: 12px; border: 1px solid #1f2937; display: flex; align-items: center; justify-content: center; overflow: hidden; color: #ffffff; }"
        ".button-container { display: flex; flex-direction: row; gap: 12px; }"
        ".btn { width: 160px; height: 42px; border-radius: 10px; font-weight: 600; font-size: 14px; display: flex; justify-content: center; align-items: center; cursor: pointer; }"
        ".btn-primary { background-color: #2563eb; color: #ffffff; border: 1px solid #3b82f6; } .btn-primary:hover { background-color: #1d4ed8; }"
        ".btn-secondary { background-color: #1f2937; color: #9ca3af; border: 1px solid #374151; } .btn-secondary:hover { background-color: #374151; color: #f3f4f6; }"
    );

    // Captured widgets for dynamic mutation / visibility switching.
    auto rect   = std::make_shared<FluxUI::Widget*>(nullptr);
    auto circle = std::make_shared<FluxUI::Widget*>(nullptr);
    auto group  = std::make_shared<FluxUI::Widget*>(nullptr);
    auto cubic  = std::make_shared<FluxUI::Widget*>(nullptr);
    auto quad   = std::make_shared<FluxUI::Widget*>(nullptr);
    auto svgRoot = std::make_shared<FluxUI::Widget*>(nullptr);
    auto btnMutate = std::make_shared<FluxUI::Widget*>(nullptr);
    auto images = std::make_shared<std::vector<FluxUI::Widget*>>(5, nullptr);   // tiger,gear,shield,ghost,firefox
    auto sideBtns = std::make_shared<std::vector<FluxUI::Widget*>>(6, nullptr);

    auto imgMount = [images](int idx) {
        return [images, idx](FluxUI::Widget* w) { (*images)[idx] = w; w->visible = false; };
    };
    auto sideMount = [sideBtns](int idx) {
        return [sideBtns, idx](FluxUI::Widget* w) { (*sideBtns)[idx] = w; };
    };

    // showSvg toggles which graphic is visible and which sidebar button is active.
    auto showSvg = [=, &app](size_t index) {
        if (*svgRoot) (*svgRoot)->visible = (index == 0);
        for (size_t i = 0; i < images->size(); ++i)
            if ((*images)[i]) (*images)[i]->visible = (index == i + 1);
        if (*btnMutate) (*btnMutate)->visible = (index == 0);
        for (size_t i = 0; i < sideBtns->size(); ++i) {
            if (!(*sideBtns)[i]) continue;
            (*sideBtns)[i]->className = (i == index) ? "sidebar-btn sidebar-btn-active" : "sidebar-btn";
            (*sideBtns)[i]->markStyleDirty();
        }
        app.raw().requestRedraw();
    };

    const char* svgClass = "width: 200px; height: 200px; object-fit: contain; filter: drop-shadow(0 4px 12px rgba(0,0,0,0.5));";

    app.setRoot(
        Div({
            Div({
                Span("Blink-Parity Native SVG Dashboard").className("title"),
                Span("High-fidelity SVG parsing and rasterization matching Chromium standards.").className("subtitle")
            }).className("title-container"),

            Div({
                Div({
                    El("div", "Select SVG Source").className("sidebar-title"),
                    Button("Dynamic SVG (Mutable)").className("sidebar-btn sidebar-btn-active")
                        .onMount(sideMount(0)).onClick([showSvg]{ showSvg(0); }),
                    Button("Classic Tiger SVG (Complex)").className("sidebar-btn")
                        .onMount(sideMount(1)).onClick([showSvg]{ showSvg(1); }),
                    Button("Font-Awesome Gear SVG").className("sidebar-btn")
                        .onMount(sideMount(2)).onClick([showSvg]{ showSvg(2); }),
                    Button("Font-Awesome Shield SVG").className("sidebar-btn")
                        .onMount(sideMount(3)).onClick([showSvg]{ showSvg(3); }),
                    Button("Font-Awesome Ghost SVG").className("sidebar-btn")
                        .onMount(sideMount(4)).onClick([showSvg]{ showSvg(4); }),
                    Button("Font-Awesome Firefox SVG").className("sidebar-btn")
                        .onMount(sideMount(5)).onClick([showSvg]{ showSvg(5); })
                }).className("sidebar"),

                Div({
                    Div({
                        // Dynamic, programmatically mutable SVG
                        El("svg", {
                            El("rect").attr("x","40").attr("y","40").attr("width","110").attr("height","90")
                                .attr("rx","12").attr("ry","12").attr("fill","#10b981").attr("stroke","#f59e0b")
                                .attr("stroke-width","4").onMount([rect](FluxUI::Widget* w){ *rect = w; }),
                            El("circle").attr("cx","390").attr("cy","80").attr("r","45")
                                .attr("fill","#ec4899").attr("stroke","#ffffff").attr("stroke-width","3")
                                .onMount([circle](FluxUI::Widget* w){ *circle = w; }),
                            El("g", {
                                El("polygon").attr("points","0,120 40,30 80,120").attr("fill","#eab308")
                                    .attr("stroke","#ffffff").attr("stroke-width","2"),
                                El("path").attr("d","M 120 80 C 120 20, 180 20, 180 80 S 240 140, 240 80")
                                    .attr("fill","none").attr("stroke","#a855f7").attr("stroke-width","5")
                                    .onMount([cubic](FluxUI::Widget* w){ *cubic = w; }),
                                El("path").attr("d","M 280 80 Q 320 20, 360 80 T 440 80")
                                    .attr("fill","none").attr("stroke","#06b6d4").attr("stroke-width","5")
                                    .onMount([quad](FluxUI::Widget* w){ *quad = w; }),
                                El("line").attr("x1","120").attr("y1","140").attr("x2","440").attr("y2","140")
                                    .attr("stroke","#f43f5e").attr("stroke-width","6")
                            }).attr("transform","translate(40, 180)").onMount([group](FluxUI::Widget* w){ *group = w; })
                        }).attr("width","500").attr("height","380").attr("viewBox","0 0 500 380")
                            .attr("preserveAspectRatio","xMidYMid meet")
                            .onMount([svgRoot](FluxUI::Widget* w){ *svgRoot = w; }),

                        // File-based SVG images (hidden until selected).
                        Img("assets/tiger.svg").style("width","500px").style("height","380px").style("object-fit","contain").onMount(imgMount(0)),
                        Img("assets/gear.svg").attr("style", svgClass).onMount(imgMount(1)),
                        Img("assets/shield.svg").attr("style", svgClass).onMount(imgMount(2)),
                        Img("assets/ghost.svg").attr("style", svgClass).onMount(imgMount(3)),
                        Img("assets/firefox.svg").attr("style", svgClass).onMount(imgMount(4))
                    }).className("svg-viewport"),

                    Div({
                        Button("Mutate Shapes").className("btn btn-primary")
                            .onMount([btnMutate](FluxUI::Widget* w){ *btnMutate = w; })
                            .onClick([=]{
                                static bool toggled = false;
                                toggled = !toggled;
                                if (toggled) {
                                    if (*rect) { (*rect)->setAttribute("fill","#f59e0b"); (*rect)->setAttribute("stroke","#10b981"); }
                                    if (*circle) { (*circle)->setAttribute("cx","410"); (*circle)->setAttribute("r","50"); }
                                    if (*group) (*group)->setAttribute("transform","translate(40, 180) scale(1.05)");
                                    if (*cubic) (*cubic)->setAttribute("stroke","#3b82f6");
                                    if (*quad) (*quad)->setAttribute("stroke","#ef4444");
                                } else {
                                    if (*rect) { (*rect)->setAttribute("fill","#10b981"); (*rect)->setAttribute("stroke","#f59e0b"); }
                                    if (*circle) { (*circle)->setAttribute("cx","390"); (*circle)->setAttribute("r","45"); }
                                    if (*group) (*group)->setAttribute("transform","translate(40, 180)");
                                    if (*cubic) (*cubic)->setAttribute("stroke","#a855f7");
                                    if (*quad) (*quad)->setAttribute("stroke","#06b6d4");
                                }
                                std::cout << "[Dashboard] Mutated shapes programmatically. Redraw triggered." << std::endl;
                            }),
                        Button("Close Dashboard").className("btn btn-secondary")
                            .onClick([&app]{ app.raw().running = false; })
                    }).className("button-container")
                }).className("preview-area")
            }).className("container")
        }).className("root")
    );

    int startIndex = 2;
    if (argc > 1) {
        try { startIndex = std::stoi(argv[1]); } catch (...) {}
    }
    showSvg(startIndex);
    return app.run();
}
