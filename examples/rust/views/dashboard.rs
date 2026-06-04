// View: /dashboard — file-based routing (call register() once at startup).
use fluxui::dsl::*;

pub fn view() -> Node {
    div(vec![
        h1("Dashboard").class("title"),
        p("Welcome to the auto-routed dashboard.").class("body"),
    ])
    .class("page")
}

pub fn register() {
    register_view("/dashboard", view);
}
