// FluxUI Zig file-based routing demo — file-per-view, no codegen.
//
// Each views/*.zig exposes view() + register(). main calls each register()
// (Zig has no auto-init), then app.useViews() wires them into the router.
const fluxui = @import("fluxui.zig");
const d = fluxui.dsl;
const dashboard = @import("views/dashboard.zig");
const settings = @import("views/settings.zig");

fn gotoDashboard(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    d.navigate("/dashboard");
}
fn gotoSettings(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
    d.navigate("/settings");
}

// The app shell: sidebar nav + the active view content.
fn layout(content: d.Node) d.Node {
    return d.div(&.{
        d.nav(&.{
            d.button("Dashboard").onClick(gotoDashboard, null),
            d.button("Settings").onClick(gotoSettings, null),
        }).class("sidebar"),
        d.div(&.{content}).class("content"),
    }).class("app");
}

pub fn main() !void {
    const app = try fluxui.App.create();
    defer app.deinit();

    try app.init("FluxUI Zig Router Demo", 1000, 700);
    _ = app.loadDefaultFont(16.0);

    app.addStylesheet(
        ".app { display: flex; flex-direction: row; width: 100%; height: 100%; }" ++
        ".sidebar { display: flex; flex-direction: column; width: 220px; background-color: #111; padding: 16px; gap: 8px; }" ++
        ".content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; }" ++
        ".title { font-size: 24px; font-weight: 700; }",
    );

    // File-based routing: each view self-registers, then useViews() wires them.
    dashboard.register();
    settings.register();
    app.useViews();
    app.setLayout(layout);

    app.runReactive();
}
