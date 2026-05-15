#include "fluxui/fluxui_c.h"

static void stop_app(FluxUIWidget* widget, void* user_data) {
    (void)widget;
    fluxui_app_stop((FluxUIApp*)user_data);
}

int main(void) {
    FluxUIApp* app = fluxui_app_create();
    if (!fluxui_app_init(app, "FluxUI C Example", 900, 600)) {
        fluxui_app_destroy(app);
        return 1;
    }

    fluxui_app_load_font(app, "C:/Windows/Fonts/segoeui.ttf", 16.0f);
    fluxui_app_add_stylesheet(app,
        ".root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 16px; }"
        ".title { height: 36px; font-size: 26px; font-weight: 700; color: #edf3f8; }"
        ".body { height: 24px; font-size: 14px; color: rgba(237, 243, 248, 0.68); }"
        ".button { width: 140px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; }"
    );

    FluxUIWidget* root = fluxui_app_root(app);
    fluxui_widget_add_text(root, "Hello from C", "title");
    fluxui_widget_add_text(root, "This UI is built through the FluxUI C ABI.", "body");
    FluxUIWidget* button = fluxui_widget_add_button(root, "Close", "button");
    fluxui_widget_set_on_click(button, stop_app, app);

    fluxui_app_run(app);
    fluxui_app_destroy(app);
    return 0;
}
