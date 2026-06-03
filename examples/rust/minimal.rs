// FluxUI Rust minimal example — modern HTML/Blink-named declarative DSL.
extern crate fluxui;

use fluxui::dsl::*;
use fluxui::App;

fn main() -> Result<(), fluxui::Error> {
    let app = App::create()?;
    app.init("FluxUI Rust Example", 900, 600)?;

    if !app.load_default_font(16.0) {
        app.load_font("C:/Windows/Fonts/segoeui.ttf", 16.0);
    }

    app.add_css(
        ".root { display: flex; flex-direction: column; background-color: #0c0f12; padding: 32px; gap: 20px; } \
         .title { font-size: 28px; font-weight: 700; color: #edf3f8; } \
         .body { font-size: 15px; color: rgba(237, 243, 248, 0.75); line-height: 1.6; } \
         .card-container { display: flex; flex-direction: row; gap: 20px; margin-top: 10px; } \
         .card { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; border-radius: 12px; background-color: #14191f; gap: 8px; } \
         .card-title { font-size: 18px; font-weight: 600; color: #edf3f8; } \
         .card-value { font-size: 32px; font-weight: 700; color: #37c6a3; } \
         .button { width: 200px; height: 46px; border-radius: 10px; background-color: #37c6a3; color: #06100d; font-weight: 600; display: flex; justify-content: center; align-items: center; margin-top: 20px; }",
    );

    let clicks = State::new(0);

    app.set_root(
        div(vec![
            div(vec![h1("FluxUI \u{26A1} Rust").class("title")]).class("header"),
            p("Welcome to the Rust bindings of FluxUI — a high-performance modern \
               rendering engine, now with a declarative HTML-named DSL.")
                .class("body"),
            div(vec![
                div(vec![
                    span("Vulkan Renderer").class("card-title"),
                    span("60 FPS").class("card-value"),
                ])
                .class("card"),
                div(vec![
                    span("FFI Memory Safety").class("card-title"),
                    span("Zero Overhead").class("card-value"),
                ])
                .class("card"),
            ])
            .class("card-container"),
            text_fn({
                let c = clicks.clone();
                move || format!("Clicked {} times", c.get())
            })
            .class("body"),
            button("Click Me").class("button").on_click({
                let c = clicks.clone();
                move || c.set(c.get() + 1)
            }),
        ])
        .class("root"),
    );

    app.run_reactive();
    Ok(())
}
