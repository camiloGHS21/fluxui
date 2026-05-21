extern crate fluxui;

use fluxui::App;

fn main() -> Result<(), fluxui::Error> {
    // Create the FluxUI App instance
    let app = App::create()?;
    
    // Initialize the window using FluxUI Vulkan backend (default fallback)
    app.init("FluxUI Rust Example", 900, 600)?;
    
    // Load and warm the default UI font before the first frame.
    if !app.load_default_font(16.0) {
        app.load_font("C:/Windows/Fonts/segoeui.ttf", 16.0);
    }
    app.warm_font_cache(&[14.0, 16.0, 18.0, 26.0, 28.0, 32.0], "default");
    app.release_font_sources();
    
    // Add modern and sleek dark mode style sheet
    app.add_stylesheet(
        ".root { \
            display: flex; \
            flex-direction: column; \
            background-color: #0c0f12; \
            padding: 32px; \
            gap: 20px; \
        } \
        .header { \
            display: flex; \
            flex-direction: row; \
            align-items: center; \
            gap: 16px; \
        } \
        .title { \
            height: 38px; \
            font-size: 28px; \
            font-weight: 700; \
            color: #edf3f8; \
        } \
        .subtitle { \
            height: 20px; \
            font-size: 14px; \
            color: #37c6a3; \
            font-weight: 600; \
        } \
        .body { \
            font-size: 15px; \
            color: rgba(237, 243, 248, 0.75); \
            line-height: 1.6; \
        } \
        .card-container { \
            display: flex; \
            flex-direction: row; \
            gap: 20px; \
            margin-top: 10px; \
        } \
        .card { \
            flex-grow: 1; \
            padding: 24px; \
            border-radius: 12px; \
            background-color: #14191f; \
            display: flex; \
            flex-direction: column; \
            gap: 8px; \
        } \
        .card-title { \
            font-size: 18px; \
            font-weight: 600; \
            color: #edf3f8; \
        } \
        .card-value { \
            font-size: 32px; \
            font-weight: 700; \
            color: #37c6a3; \
        } \
        .button { \
            width: 160px; \
            height: 46px; \
            border-radius: 10px; \
            background-color: #37c6a3; \
            color: #06100d; \
            font-weight: 600; \
            display: flex; \
            justify-content: center; \
            align-items: center; \
            margin-top: 20px; \
        }"
    )?;
    
    // Get the root widget of the application
    let root = app.root().ok_or(fluxui::Error::InitFailed)?;
    root.reserve_children(4);
    
    // Header section
    let header = root.add_panel("header")?.ok_or(fluxui::Error::InitFailed)?;
    header.add_text("FluxUI ⚡ Rust", "title")?;
    
    // Description text
    root.add_text("Welcome to the Rust bindings of FluxUI - a high-performance modern rendering engine running natively on Vulkan.", "body")?;
    
    // Card Section
    let cards = root.add_panel("card-container")?.ok_or(fluxui::Error::InitFailed)?;
    cards.reserve_children(2);
    
    // Card 1
    let card1 = cards.add_panel("card")?.ok_or(fluxui::Error::InitFailed)?;
    card1.add_text("Vulkan Renderer", "card-title")?;
    card1.add_text("Active Backend", "subtitle")?;
    card1.add_text("60 FPS", "card-value")?;
    
    // Card 2
    let card2 = cards.add_panel("card")?.ok_or(fluxui::Error::InitFailed)?;
    card2.add_text("FFI Memory Safety", "card-title")?;
    card2.add_text("Safe Rust Wrappers", "subtitle")?;
    card2.add_text("Zero Overhead", "card-value")?;

    // Close button
    let close = root.add_button("Close Application", "button")?.ok_or(fluxui::Error::InitFailed)?;
    close.set_on_click_stop_app(&app);
    
    // Start running the application main event loop
    app.run();
    
    Ok(())
}
