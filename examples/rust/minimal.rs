extern crate fluxui;

use fluxui::App;

fn main() -> fluxui::Result<()> {
    let app = App::create()?;
    app.init("FluxUI Rust Example", 900, 600)?;
    app.load_font("C:/Windows/Fonts/segoeui.ttf", 16.0);

    app.add_stylesheet(
        ".root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 16px; }\
         .title { height: 36px; font-size: 26px; font-weight: 700; color: #edf3f8; }\
         .body { height: 24px; font-size: 14px; color: rgba(237, 243, 248, 0.68); }\
         .button { width: 140px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; }",
    )?;

    let root = app.root().expect("FluxUI root widget missing");
    root.add_text("Hello from Rust", "title")?;
    root.add_text("This imports FluxUI as a rustc library crate.", "body")?;

    let close = root
        .add_button("Close", "button")?
        .expect("failed to create close button");
    close.set_on_click_stop_app(&app);

    app.run();
    Ok(())
}
