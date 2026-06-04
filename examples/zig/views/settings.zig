// View: /settings — file-based routing (call register() once at startup).
const fluxui = @import("../fluxui.zig");
const d = fluxui.dsl;

pub fn view() d.Node {
    return d.div(&.{
        d.h1("Settings").class("title"),
        d.p("Configure your preferences here.").class("body"),
    }).class("page");
}

pub fn register() void {
    d.registerView("/settings", view);
}
