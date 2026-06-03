// FluxUI Zig minimal example — modern HTML/Blink-named declarative DSL.
const std = @import("std");
const fluxui = @import("fluxui.zig");
const d = fluxui.dsl;

const Ctx = struct {
    var clicks = d.State(i32).init(0);
    var buf: [48]u8 = undefined;

    fn clickText(_: ?*anyopaque) callconv(.c) [*:0]const u8 {
        const s = std.fmt.bufPrintZ(&buf, "Clicked {d} times", .{clicks.get()}) catch return "?";
        return s.ptr;
    }

    fn onClick(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
        clicks.set(clicks.get() + 1);
    }
};

pub fn main() !void {
    const app = try fluxui.App.create();
    defer app.deinit();

    try app.init("FluxUI Zig Example", 900, 600);
    if (!app.loadDefaultFont(16.0)) {
        _ = app.loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0);
    }

    app.addStylesheet(
        ".root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 16px; }" ++
        ".title { font-size: 26px; font-weight: 700; color: #edf3f8; }" ++
        ".body { font-size: 14px; color: rgba(237, 243, 248, 0.68); }" ++
        ".button { width: 200px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; }",
    );

    try app.setRoot(
        d.div(&.{
            d.h1("Hello from Zig").class("title"),
            d.p("This window is built with the declarative HTML-named DSL.").class("body"),
            d.textFn(Ctx.clickText, null).class("body"),
            d.button("Click Me").class("button").onClick(Ctx.onClick, null),
        }).class("root"),
    );

    app.runReactive();
}
