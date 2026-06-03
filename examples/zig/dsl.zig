// FluxUI Declarative DSL example in Zig — modern HTML/Blink-named API.
//
// Zig has no closures, so reactive state and click handlers are wired through a
// small Ctx struct holding the State plus C-ABI callbacks. The declarative tree
// reads like HTML, exactly like the C++/Go/Rust/Python/Java versions.
const std = @import("std");
const fluxui = @import("fluxui.zig");
const d = fluxui.dsl;

const Ctx = struct {
    var devices = d.State(i32).init(128);
    var buf: [32]u8 = undefined;

    // Reactive text source: formats the current device count into a C string.
    fn deviceText(_: ?*anyopaque) callconv(.c) [*:0]const u8 {
        const s = std.fmt.bufPrintZ(&buf, "{d}", .{devices.get()}) catch return "?";
        return s.ptr;
    }

    // Click handler: bump the count. The reactive pump pushes the new value.
    fn scan(_: ?*fluxui.c.FluxUIWidget, _: ?*anyopaque) callconv(.c) void {
        devices.set(devices.get() + 1);
    }
};

pub fn main() !void {
    const app = try fluxui.App.create();
    defer app.deinit();

    try app.init("CompanyGuard", 1200, 800);
    _ = app.loadDefaultFont(16.0);

    app.addStylesheet(
        ".app { display: flex; flex-direction: row; width: 100%; height: 100%; }" ++
        ".sidebar { display: flex; flex-direction: column; width: 250px; background-color: #111115; padding: 16px; gap: 8px; }" ++
        ".content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; gap: 16px; }" ++
        "h1 { font-size: 24px; font-weight: 700; }" ++
        ".metrics { display: flex; flex-direction: row; gap: 16px; }" ++
        ".metric-card { display: flex; flex-direction: column; background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }" ++
        ".primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }",
    );

    try app.setRoot(
        d.div(&.{
            d.nav(&.{
                d.button("Dashboard"),
                d.button("Dispositivos"),
                d.button("Backups"),
                d.button("Seguridad"),
            }).class("sidebar"),

            d.div(&.{
                d.h1("CompanyGuard"),
                d.div(&.{
                    d.div(&.{
                        d.span("Equipos activos"),
                        d.textFn(Ctx.deviceText, null),
                    }).class("metric-card"),
                    d.div(&.{
                        d.span("Alertas"),
                        d.span("7"),
                    }).class("metric-card"),
                }).class("metrics"),
                d.button("Escanear ahora").class("primary").onClick(Ctx.scan, null),
            }).class("content"),
        }).class("app"),
    );

    app.runReactive();
}
