// View: /dashboard — file-based routing (call register() once at startup).
const fluxui = @import("../fluxui.zig");
const d = fluxui.dsl;

pub fn view() d.Node {
    return d.div(&.{
        d.h1("Dashboard").class("title"),
        d.p("Welcome to the auto-routed dashboard.").class("body"),
    }).class("page");
}

pub fn register() void {
    d.registerView("/dashboard", view);
}
