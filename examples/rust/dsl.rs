// FluxUI Declarative DSL example in Rust — mirrors the C++ demo_dsl.cpp.
//
// Build (after building fluxui_shared):
//   rustc --crate-name fluxui --crate-type rlib bindings/rust/lib.rs \
//     -L native=build/Release -o build/Release/libfluxui.rlib
//   rustc examples/rust/dsl.rs --extern fluxui=build/Release/libfluxui.rlib \
//     -L native=build/Release -o build/Release/dsl_rust
extern crate fluxui;

use fluxui::dsl::*;
use fluxui::App;

fn main() -> Result<(), fluxui::Error> {
    let app = App::create()?;
    app.init("CompanyGuard", 1200, 800)?;
    app.load_default_font(16.0);

    app.add_css(
        ".app { width: 100%; height: 100%; } \
         .sidebar { width: 250px; background-color: #111115; padding: 16px; gap: 8px; } \
         .content { flex-grow: 1; padding: 24px; gap: 16px; } \
         .h1 { font-size: 24px; font-weight: 700; } \
         .metric-card { background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; } \
         .primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }",
    );

    let devices = State::new(128);

    app.set_root(
        row(vec![
            sidebar(vec![
                nav_item("Dashboard"),
                nav_item("Dispositivos"),
                nav_item("Backups"),
                nav_item("Seguridad"),
            ])
            .class("sidebar"),
            column(vec![
                text("CompanyGuard").class("h1"),
                row(vec![
                    card(vec![
                        text("Equipos activos"),
                        text_fn({
                            let d = devices.clone();
                            move || d.get().to_string()
                        }),
                    ])
                    .class("metric-card"),
                    card(vec![text("Alertas"), text("7")]).class("metric-card"),
                ])
                .class("metrics"),
                button("Escanear ahora").class("primary").on_click({
                    let d = devices.clone();
                    move || d.set(d.get() + 1)
                }),
            ])
            .class("content"),
        ])
        .class("app"),
    );

    app.run_reactive();
    Ok(())
}
