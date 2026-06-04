// FluxUI Rust file-based routing demo — file-per-view, no codegen.
//
// Each views/*.rs exposes view() + register(). main calls each register() once
// (Rust has no auto-init), then use_views() wires them into the router.
extern crate fluxui;

use fluxui::dsl::*;
use fluxui::App;

#[path = "views/dashboard.rs"]
mod dashboard;
#[path = "views/settings.rs"]
mod settings;

fn main() -> Result<(), fluxui::Error> {
    let app = App::create()?;
    app.init("FluxUI Rust Router Demo", 1000, 700)?;
    app.load_default_font(16.0);

    app.add_css(
        ".app { display: flex; flex-direction: row; width: 100%; height: 100%; } \
         .sidebar { display: flex; flex-direction: column; width: 220px; background-color: #111; padding: 16px; gap: 8px; } \
         .content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; } \
         .title { font-size: 24px; font-weight: 700; }",
    );

    // File-based routing: each view self-registers, then use_views() wires them.
    dashboard::register();
    settings::register();
    app.use_views();

    app.set_layout(|content| {
        let mut content_children = Vec::new();
        if let Some(c) = content {
            content_children.push(c);
        }
        div(vec![
            nav(vec![
                button("Dashboard").on_click(|| navigate("/dashboard")),
                button("Settings").on_click(|| navigate("/settings")),
            ])
            .class("sidebar"),
            div(content_children).class("content"),
        ])
        .class("app")
    });

    app.run_reactive();
    Ok(())
}
