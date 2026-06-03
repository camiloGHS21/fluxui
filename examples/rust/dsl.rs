// FluxUI Declarative DSL example in Rust — modern HTML/Blink-named API.
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
        ".app { display: flex; flex-direction: row; width: 100%; height: 100%; } \
         .sidebar { display: flex; flex-direction: column; width: 250px; background-color: #111115; padding: 16px; gap: 8px; } \
         .content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; gap: 16px; } \
         h1 { font-size: 24px; font-weight: 700; } \
         .metrics { display: flex; flex-direction: row; gap: 16px; } \
         .metric-card { display: flex; flex-direction: column; background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; } \
         .primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }",
    );

    let devices = State::new(128);

    app.set_root(
        div(vec![
            nav(vec![
                button("Dashboard"),
                button("Dispositivos"),
                button("Backups"),
                button("Seguridad"),
            ])
            .class("sidebar"),
            div(vec![
                h1("CompanyGuard"),
                div(vec![
                    div(vec![
                        span("Equipos activos"),
                        text_fn({
                            let d = devices.clone();
                            move || d.get().to_string()
                        }),
                    ])
                    .class("metric-card"),
                    div(vec![span("Alertas"), span("7")]).class("metric-card"),
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
