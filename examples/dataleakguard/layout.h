// DataLeak Guard — App shell layout (sidebar + content slot)
#pragma once
#include "state.h"
#include "components.h"

namespace dlg {

// Navigation item that triggers route change on the App.
inline Element NavItem(const std::string& label, const std::string& icon,
                       const std::string& route, App& app) {
    return Button(label)
        .className(app.route() == route ? "nav-item active" : "nav-item")
        .onClick([&app, route]{ app.navigate(route); });
}

// The persistent shell wrapping all views.
inline Element ShellLayout(App& app, const Element& content) {
    return Div({
        // Sidebar
        Nav({
            Div({
                Div({
                    Icon("shield").className("logo-icon")
                }).className("logo-mark"),
                Div({
                    Span("DataLeak Guard").className("sidebar-logo"),
                    Span("Enterprise DLP Console").className("sidebar-subtitle")
                }).className("logo-copy")
            }).className("logo-row"),

            Span("WORKSPACE").className("nav-label"),
            NavItem("Dashboard", "dashboard", "/dashboard", app),
            NavItem("Scanner",   "scanner",   "/scanner",   app),
            NavItem("Alerts",    "alert",     "/alerts",    app),
            NavItem("Rules",     "rules",     "/rules",     app),
            NavItem("Reports",   "report",    "/reports",   app),
            NavItem("Settings",  "settings",  "/settings",  app),
            NavItem("Blink UI",  "rules",     "/blink",     app),

            Div({
                Span("POSTURE").className("nav-label"),
                Text([]{
                    return blockMode().get() ? std::string("Containment active")
                                            : std::string("Monitor only");
                }).className("posture-title"),
                Span("4 high-risk events require review").className("posture-copy"),
                Div({ Pill("DLP","ok"), Pill("SIEM","info") }).className("posture-pills")
            }).className("posture-card")
        }).className("sidebar"),

        // Content area — views get mounted here.
        Div({ content }).className("content").id("__content__")
    }).className("app-shell");
}

} // namespace dlg
