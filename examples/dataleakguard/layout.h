// DataLeak Guard — App shell layout (sidebar + status bar + content slot)
#pragma once
#include "components.h"

namespace dlg {

// The persistent shell: sidebar (left) + content area (center) + status bar (bottom).
// The content slot has id "__content__" where views are swapped.
inline Element ShellLayout(App& app, const Element& content) {
    static auto blockMode = State<bool>(true);

    return Div({
        // Sidebar
        Nav({
            Div({
                Div({ Icon("shield").className("logo-icon") }).className("logo-mark"),
                Div({
                    Span("DataLeak Guard").className("sidebar-logo"),
                    Span("Enterprise DLP Console").className("sidebar-subtitle")
                }).className("logo-copy")
            }).className("logo-row sidebar-header"),

            // Nav section wraps the workspace label + nav items (gives horizontal padding).
            Div({
                Span("WORKSPACE").className("nav-label"),
                Button("Dashboard").className(app.route() == "/dashboard" ? "nav-item active" : "nav-item")
                    .onClick([&app]{ app.navigate("/dashboard"); }),
                Button("Scanner").className(app.route() == "/scanner" ? "nav-item active" : "nav-item")
                    .onClick([&app]{ app.navigate("/scanner"); }),
                Button("Alerts").className(app.route() == "/alerts" ? "nav-item active" : "nav-item")
                    .onClick([&app]{ app.navigate("/alerts"); }),
                Button("Rules").className(app.route() == "/rules" ? "nav-item active" : "nav-item")
                    .onClick([&app]{ app.navigate("/rules"); }),
                Button("Reports").className(app.route() == "/reports" ? "nav-item active" : "nav-item")
                    .onClick([&app]{ app.navigate("/reports"); }),
                Button("Settings").className(app.route() == "/settings" ? "nav-item active" : "nav-item")
                    .onClick([&app]{ app.navigate("/settings"); })
            }).className("nav-section"),

            // Posture card (pushed to bottom via nav-section flex-grow)
            Div({
                Span("POSTURE").className("nav-label"),
                Text([]{
                    static auto& bm = blockMode;
                    return bm.get() ? std::string("Containment active")
                                    : std::string("Monitor only");
                }).className("posture-title"),
                Span("4 high-risk events require review").className("posture-copy"),
                Div({ Pill("DLP","ok"), Pill("SIEM","info") }).className("posture-pills")
            }).className("posture-card")
        }).className("sidebar"),

        // Main content column: scrollable content + fixed status bar.
        Div({
            Div({ content }).className("main-scroll").id("__content__"),
            StatusBar()
        }).className("content")
    });
}

} // namespace dlg
