#include "fluxui/FluxUI.h"

#include <iostream>

int main() {
    FluxUI::Application app;
    if (!app.init("FluxUI C++ Example", 900, 600)) {
        return 1;
    }

    app.renderer().loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
    app.addStylesheet(
        ".root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 16px; }"
        ".title { height: 36px; font-size: 26px; font-weight: 700; color: #edf3f8; }"
        ".body { height: 24px; font-size: 14px; color: rgba(237, 243, 248, 0.68); }"
        ".button { width: 140px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; }"
    );

    auto* root = app.root();
    root->add<FluxUI::Text>("Hello from C++", "title");
    root->add<FluxUI::Text>("This app uses the native FluxUI C++ API.", "body");
    auto* button = root->add<FluxUI::Button>("Close", "button");
    button->onClick = [&]() { app.running = false; };

    app.run();
    app.shutdown();
    return 0;
}
