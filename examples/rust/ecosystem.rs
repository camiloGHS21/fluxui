// FluxUI Ecosystem Demo (Rust) — the modern Next.js/Zod/Zustand-style toolkit.
//
// Shows the five ecosystem features in one app:
//   1. Routing + params  — add_route("/user/:id"), dsl::param("id"), dsl::query()
//   2. Store<T>          — Zustand-style global state shared across views
//   3. Schema + Rule     — Zod-style runtime validation
//   4. Query<T>          — fetch with Idle/Loading/Success/Error + view()
//   5. Skeleton          — loading placeholders for a pending Query
//
// NOTE: compiled by the caller's crate; this file uses the public `fluxui` API.
extern crate fluxui;

use fluxui::dsl::*;
use fluxui::App;
use std::collections::HashMap;

#[derive(Clone, Default)]
struct CartState {
    count: i32,
    last: String,
}

fn signup_schema() -> Schema {
    Schema::new()
        .field("email", Rule::string().email())
        .field("age", Rule::number().min(18.0).max(120.0))
        .field("nick", Rule::string().min_length(3).max_length(12))
}

fn main() -> Result<(), fluxui::Error> {
    let app = App::create()?;
    app.init("FluxUI Ecosystem (Rust)", 1100, 760)?;
    app.load_default_font(16.0);

    app.add_css(
        ".root { display:flex; flex-direction:row; background:#0d1117; color:#e6edf3; } \
         .sidebar { display:flex; flex-direction:column; width:220px; background:#161b22; padding:18px; gap:6px; } \
         .sidebar button { text-align:left; padding:10px 12px; border-radius:8px; color:#c9d1d9; } \
         .sidebar button.active { background:#1f6feb; color:white; } \
         .content { display:flex; flex-direction:column; flex-grow:1; padding:28px; gap:18px; } \
         h1 { font-size:26px; font-weight:700; } \
         .card { background:#161b22; border:1px solid #30363d; border-radius:12px; padding:20px; gap:10px; display:flex; flex-direction:column; } \
         .row { display:flex; flex-direction:row; gap:10px; align-items:center; } \
         .primary { background:#238636; color:white; padding:10px 18px; border-radius:8px; } \
         .ok { color:#3fb950; } .danger { color:#f85149; } \
         .skeleton { display:flex; flex-direction:column; gap:10px; } \
         .skeleton-line { height:14px; border-radius:6px; background:#21262d; }",
    );

    // Global store (Zustand-style): Rc-backed, cloned into each closure.
    let cart: Store<CartState> = Store::new(CartState::default());

    // ---- Route 1: Store ----
    let cart_store = cart.clone();
    app.add_route("/store", move || {
        let c1 = cart_store.clone();
        let c2 = cart_store.clone();
        let c3 = cart_store.clone();
        let c_count = cart_store.clone();
        let c_last = cart_store.clone();
        div(vec![
            h1("Store (Zustand-style)"),
            div(vec![
                p("A single global store, mutated via a reducer."),
                text_fn(move || format!("Items in cart: {}", c_count.get().count)),
                text_fn(move || {
                    let last = c_last.get().last;
                    format!("Last added: {}", if last.is_empty() { "(none)".into() } else { last })
                }),
                div(vec![
                    button("Add apple").class("primary").on_click(move || {
                        c1.set(|s| { s.count += 1; s.last = "apple".into(); });
                    }),
                    button("Add book").class("primary").on_click(move || {
                        c2.set(|s| { s.count += 1; s.last = "book".into(); });
                    }),
                    button("Reset").on_click(move || {
                        c3.set(|s| { s.count = 0; s.last = String::new(); });
                    }),
                ])
                .class("row"),
            ])
            .class("card"),
        ])
    });

    // ---- Route 2: Schema validation ----
    app.add_route("/form", || {
        let result: State<String> =
            State::new("Press a button to validate a sample payload.".to_string());
        let r_good = result.clone();
        let r_bad = result.clone();
        div(vec![
            h1("Schema validation (Zod-style)"),
            div(vec![
                {
                    let r = result.clone();
                    text_fn(move || r.get()).class("ok")
                },
                div(vec![
                    button("Validate good payload").class("primary").on_click(move || {
                        let mut d = HashMap::new();
                        d.insert("email".into(), "a@b.com".into());
                        d.insert("age".into(), "25".into());
                        d.insert("nick".into(), "joe".into());
                        let v = signup_schema().validate(&d);
                        r_good.set(if v.ok {
                            "Valid! Welcome aboard.".to_string()
                        } else {
                            format!("Invalid: {}", v.errors.values().next().cloned().unwrap_or_default())
                        });
                    }),
                    button("Validate bad payload").on_click(move || {
                        let mut d = HashMap::new();
                        d.insert("email".into(), "nope".into());
                        d.insert("age".into(), "12".into());
                        d.insert("nick".into(), "x".into());
                        let v = signup_schema().validate(&d);
                        r_bad.set(if v.ok {
                            "Valid! Welcome aboard.".to_string()
                        } else {
                            format!("Invalid: {}", v.errors.values().next().cloned().unwrap_or_default())
                        });
                    }),
                ])
                .class("row"),
            ])
            .class("card"),
        ])
    });

    // ---- Route 3: Query + Skeleton ----
    app.add_route("/query", || {
        // Synchronous fetcher (Rust FFI is single-threaded here). The Query is
        // rebuilt fresh on each navigation and resolves immediately on start().
        let q: Query<String> =
            Query::new(|| Ok("Ada Lovelace, Alan Turing, Grace Hopper".to_string()));
        q.start();
        div(vec![
            h1("Query (React-Query-style)"),
            div(vec![
                p("Runs a fetcher and renders per state. Skeleton on loading."),
                q.view(
                    || skeleton(3),
                    |data| text(&format!("Users: {}", data)).class("ok"),
                    |err| text(&format!("Error: {}", err)).class("danger"),
                ),
            ])
            .class("card"),
        ])
    });

    // ---- Route 4: Route params + query string ----
    app.add_route("/user/:id", || {
        let id = param("id");
        let tab = {
            let t = query("tab");
            if t.is_empty() { "profile".to_string() } else { t }
        };
        div(vec![
            h1(&format!("User #{}", id)),
            div(vec![
                p("Route pattern: /user/:id — captured from the URL path."),
                text(&format!("id param  = {}", id)),
                text(&format!("tab query = {}", tab)),
                div(vec![
                    button("Go to user 42").on_click(|| navigate("/user/42?tab=settings")),
                    button("Go to user 99").on_click(|| navigate("/user/99?tab=info")),
                ])
                .class("row"),
            ])
            .class("card"),
        ])
    });

    // Layout shell: sidebar highlights active route + content slot.
    app.set_layout(|content| {
        let nav_btn = |label: &str, path: &'static str| {
            let cls = if route() == path { "active" } else { "" };
            button(label).class(cls).on_click(move || navigate(path))
        };
        let mut content_children = Vec::new();
        if let Some(c) = content {
            content_children.push(c);
        }
        div(vec![
            nav(vec![
                el("h2", vec![text("FluxUI")]),
                nav_btn("Store", "/store"),
                nav_btn("Form (Zod)", "/form"),
                nav_btn("Query", "/query"),
                nav_btn("User #7", "/user/7?tab=info"),
            ])
            .class("sidebar"),
            div(content_children).id("__content__").class("content"),
        ])
        .class("root")
    });

    app.build("/store");
    app.run_reactive();
    Ok(())
}
