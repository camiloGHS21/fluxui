// DataLeak Guard — Enterprise DLP Console
// Built with FluxUI declarative DSL.
//
// File-based routing: views are auto-registered from views/*.h by CMake.
// Just add a new .h file → reconfigure → route exists. Like Next.js.
#include <fluxui/dsl.h>
#include "embedded_font_atlas.h"
#include "embedded_theme.h"
#include "layout.h"
#include "dlg_routes_gen.h"   // ← auto-generated from views/*.h

using namespace fluxui;

int main() {
    App app(1400, 900, "DataLeak Guard - Enterprise DLP");
    app.addCSS(dataleakguardEmbeddedThemeCss());

    // File-based routing: all views from views/ registered automatically.
    dlg::generated::registerRoutes(app);

    // Shell layout (sidebar + content slot). Starts at /dashboard.
    app.setLayout([&](const Element& content) {
        return dlg::ShellLayout(app, content);
    });
    app.build("/dashboard");

    return app.run();
}
