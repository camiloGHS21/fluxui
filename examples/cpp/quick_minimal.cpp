#include "fluxui/FluxUI.h"
#include <iostream>

int main() {
    // Zero-boilerplate QuickApp with auto-configured dark theme and high-quality fonts!
    FluxUI::QuickApp app("FluxUI Premium QuickApp", 960, 640);

    // Get the root panel and style it using semantic tags and flex layout
    auto* root = app.root();

    // Premium UI Title & Description
    root->h1("FluxUI Premium Desktop Experience");
    root->p("This high-performance desktop application is built with 0 lines of initialization boilerplate. Optimizations and GPU/CPU auto-fallbacks are active by default.");

    // Create an elegant layout grid/card container
    auto* grid = root->div("card");
    grid->h2("CPU & GPU Performance Statistics");

    // Add some stat display widgets with premium responsive metrics
    auto* metrics = grid->div();
    metrics->css("display: flex; flex-direction: row; gap: 20px; margin-top: 10px;");

    auto* statCpu = metrics->div("card");
    statCpu->css("flex: 1; align-items: center; border-left: 4px solid #6c5ce7; background-color: #1a222f;");
    statCpu->h2("0.55 us");
    statCpu->p("Style Resolution");

    auto* statGpu = metrics->div("card");
    statGpu->css("flex: 1; align-items: center; border-left: 4px solid #37c6a3; background-color: #1a222f;");
    statGpu->h2("120 FPS");
    statGpu->p("Target Framerate");

    // Micro-interactive interactive element
    auto* controlRow = root->div();
    controlRow->css("display: flex; flex-direction: row; gap: 16px; align-items: center; margin-top: 20px;");

    auto* clickCounter = controlRow->h2("Clicks: 0");
    clickCounter->css("min-width: 150px; color: #a29bfe;");

    static int counter = 0;
    controlRow->button("Increment", "btn", [clickCounter]() {
        counter++;
        clickCounter->content = "Clicks: " + std::to_string(counter);
    });

    controlRow->button("Exit App", "btn", [&app]() {
        app.app().running = false;
    })->css("background: linear-gradient(135deg, #e74c3c, #ff7675); box-shadow: 0 4px 15px rgba(231, 76, 60, 0.25);");

    // Run the app! Build callback is optional.
    return app.run();
}
