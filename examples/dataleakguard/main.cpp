// DataLeak Guard — Enterprise DLP Console
// Built with FluxUI declarative DSL (modern HTML/Blink-named API).
// Multi-view architecture: each view is a separate header file.
#include <fluxui/dsl.h>
#include "embedded_font_atlas.h"
#include "embedded_theme.h"

#include "state.h"
#include "layout.h"
#include "views/dashboard.h"
#include "views/scanner.h"
#include "views/alerts.h"
#include "views/rules.h"
#include "views/reports.h"
#include "views/settings.h"
#include "views/blink.h"

using namespace fluxui;

int main() {
    App app(1400, 900, "DataLeak Guard - Enterprise DLP");

    // Load embedded theme CSS (generated at build time from assets/styles/theme.css).
    app.addCSS(dataleakguardEmbeddedThemeCss());

    // Register views — each is a pure function returning an Element tree.
    app.addRoute("/dashboard", dlg::DashboardView);
    app.addRoute("/scanner",   dlg::ScannerView);
    app.addRoute("/alerts",    dlg::AlertsView);
    app.addRoute("/rules",     dlg::RulesView);
    app.addRoute("/reports",   dlg::ReportsView);
    app.addRoute("/settings",  dlg::SettingsView);
    app.addRoute("/blink",     dlg::BlinkView);

    // Shell layout wraps a persistent sidebar + content slot.
    app.setLayout([&](const Element& content) {
        return dlg::ShellLayout(app, content);
    });

    // Build and navigate to the initial route.
    app.build("/dashboard");

    return app.run();
}
