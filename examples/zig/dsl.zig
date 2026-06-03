// FluxUI Declarative DSL example in Zig — mirrors the C++ demo_dsl.cpp.
//
// Zig has no closures, so reactive state and click handlers are wired through a
// small Ctx struct holding the State plus C-ABI callbacks. The declarative tree
// itself reads exactly like the C++/Go/Rust/Python versions.
const std = @import("std");
const fluxui = @import("fluxui.zig");

const Ctx = struct {
    var devices = fluxui.dsl.State(i32).init(128);
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
        ".app { width: 100%; height: 100%; }" ++
        ".sidebar { width: 250px; background-color: #111115; padding: 16px; gap: 8px; }" ++
        ".content { flex-grow: 1; padding: 24px; gap: 16px; }" ++
        ".h1 { font-size: 24px; font-weight: 700; }" ++
        ".metric-card { background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }" ++
        ".primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }",
    );

    const d = fluxui.dsl;
    try app.setRoot(
        d.row(&.{
            d.sidebar(&.{
                d.navItem("Dashboard"),
                d.navItem("Dispositivos"),
                d.navItem("Backups"),
                d.navItem("Seguridad"),
            }).class("sidebar"),

            d.column(&.{
                d.text("CompanyGuard").class("h1"),
                d.row(&.{
                    d.card(&.{
                        d.text("Equipos activos"),
                        d.textFn(Ctx.deviceText, null),
                    }).class("metric-card"),
                    d.card(&.{
                        d.text("Alertas"),
                        d.text("7"),
                    }).class("metric-card"),
                }).class("metrics"),
                d.button("Escanear ahora").class("primary").onClick(Ctx.scan, null),
            }).class("content"),
        }).class("app"),
    );

    app.runReactive();
}
