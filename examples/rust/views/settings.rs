// View: /settings — file-based routing (call register() once at startup).
use fluxui::dsl::*;

pub fn view() -> Node {
    div(vec![
        h1("Settings").class("title"),
        p("Configure your preferences here.").class("body"),
    ])
    .class("page")
}

pub fn register() {
    register_view("/settings", view);
}
