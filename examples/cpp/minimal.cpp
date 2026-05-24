#include "fluxui/FluxUI.h"

#include <iostream>
#include <vector>

int main() {
    FluxUI::Application app;
    if (!app.init("FluxUI C++ Example", 900, 600)) {
        return 1;
    }

    if (!app.renderer().loadDefaultFont(16.0f)) {
        app.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
    }
    app.renderer().warmFontCache(std::vector<float>{14.0f, 16.0f, 26.0f});
    app.renderer().releaseFontSources();
    app.addStylesheet(
        ".root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 12px; }"
        ".title { height: 36px; font-size: 26px; font-weight: 700; color: #edf3f8; }"
        ".body { height: 24px; font-size: 14px; color: rgba(237, 243, 248, 0.68); }"
        "ul, ol { display: flex; flex-direction: column; gap: 4px; padding-inline-start: 40px; }"
        "li { font-size: 15px; color: #edf3f8; }"
        ".square-list { list-style-type: square; }"
        ".circle-list { list-style-type: circle; }"
        ".roman-list { list-style-type: upper-roman; }"
        ".alpha-list { list-style-type: lower-alpha; }"
        ".none-list { list-style-type: none; }"
        ".button { width: 140px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; margin-top: 12px; }"
    );

    auto* root = app.root();
    root->add<FluxUI::Text>("List Style Type Parity Test", "title");
    root->add<FluxUI::Text>("Testing cascaded list-style-type in FluxUI:", "body");

    // Standard Unordered List (default disc)
    auto* ul = root->element("ul");
    ul->element("li", "Default disc bullet 1");
    ul->element("li", "Default disc bullet 2");

    // Standard Ordered List (default decimal)
    auto* ol = root->element("ol");
    ol->element("li", "First decimal item");
    ol->element("li", "Second decimal item");

    // Custom Bullet: Square
    auto* ulSquare = root->element("ul", "", "square-list");
    ulSquare->element("li", "Square bullet item");

    // Custom Bullet: Circle
    auto* ulCircle = root->element("ul", "", "circle-list");
    ulCircle->element("li", "Circle bullet item");

    // Custom Numeric: Upper Roman
    auto* olRoman = root->element("ol", "", "roman-list");
    olRoman->element("li", "Roman numeral item I");
    olRoman->element("li", "Roman numeral item II");

    // Custom Numeric: Lower Alpha
    auto* olAlpha = root->element("ol", "", "alpha-list");
    olAlpha->element("li", "Alpha character item a");
    olAlpha->element("li", "Alpha character item b");

    // List Style Type: None
    auto* ulNone = root->element("ul", "", "none-list");
    ulNone->element("li", "No bullet item 1");
    ulNone->element("li", "No bullet item 2");

    auto* button = root->add<FluxUI::Button>("Close", "button");
    button->onClick = [&]() { app.running = false; };

    app.run();
    app.shutdown();
    return 0;
}
