const fluxui = @import("fluxui");

pub fn main() !void {
    const app = try fluxui.App.create();
    defer app.deinit();

    try app.init("FluxUI Zig Example", 900, 600);
    _ = app.loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0);
    app.addStylesheet(
        ".root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 16px; }" ++
        ".title { height: 36px; font-size: 26px; font-weight: 700; color: #edf3f8; }" ++
        ".body { height: 24px; font-size: 14px; color: rgba(237, 243, 248, 0.68); }" ++
        ".button { width: 140px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; }"
    );

    const root = try app.root();
    _ = try root.addText("Hello from Zig", "title");
    _ = try root.addText("This imports FluxUI as a Zig module.", "body");
    const close = try root.addButton("Close", "button");
    close.setOnClickStopApp(app);

    app.run();
}
