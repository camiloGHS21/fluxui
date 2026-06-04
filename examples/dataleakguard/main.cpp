// DataLeak Guard — Enterprise DLP Console
// Built with FluxUI declarative DSL.
//
// File-based routing (like Next.js): views are auto-registered from the
// views/ folder. Just add a new .h file there and CMake picks it up on
// reconfigure — no manual addRoute() calls needed.
#include <fluxui/dsl.h>
#include "embedded_font_atlas.h"
#include "embedded_theme.h"

#include "state.h"
#include "layout.h"
#include "dlg_routes_gen.h"   // ← auto-generated from views/*.h by CMake

using namespace fluxui;

int main() {
    App app(1400, 900, "DataLeak Guard - Enterprise DLP");
    app.addCSS(dataleakguardEmbeddedThemeCss());

    // Auto-register all views from the views/ folder (file-based routing).
    dlg::generated::registerRoutes(app);

    // Shell layout wraps a persistent sidebar + content slot.
    app.setLayout([&](const Element& content) {
        return dlg::ShellLayout(app, content);
    });

    // Build and navigate to the initial route.
    app.build("/dashboard");

    return app.run();
}
