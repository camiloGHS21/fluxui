// FluxUI Ecosystem Demo — the modern, Next.js/Zod/Zustand-style toolkit.
//
// Shows, in one file, the five "ecosystem" features that ship with the DSL:
//   1. Routing + params   — addRoute("/user/:id"), App::param("id"), query()
//   2. Store<T>           — Zustand-style global state shared across views
//   3. Schema + Rule      — Zod-style runtime validation of form input
//   4. Query<T>           — React-Query-style async fetch (Idle/Loading/.../view)
//   5. Skeleton           — loading placeholders while a Query is pending
//
// Everything is the modern declarative API (HTML/Blink-named builders). There
// is no imperative widget code here at all.
#include <fluxui/dsl.h>
#include <chrono>
#include <thread>
using namespace fluxui;

// ---- Global store (Zustand-style): a tiny cart shared by every view. --------
struct CartState { int count = 0; std::string lastItem; };

// ---- A schema (Zod-style) validating a sign-up form. ------------------------
static Schema signupSchema() {
    return Schema()
        .field("email", Rule::string().email())
        .field("age",   Rule::number().min(18).max(120))
        .field("nick",  Rule::string().minLength(3).maxLength(12));
}

int main() {
    App app(1100, 760, "FluxUI Ecosystem");
    app.addCSS(R"(
        .root { display:flex; flex-direction:row; background:#0d1117; color:#e6edf3; }
        .sidebar { display:flex; flex-direction:column; width:220px; background:#161b22; padding:18px; gap:6px; }
        .sidebar h2 { font-size:18px; font-weight:700; margin-bottom:10px; color:#58a6ff; }
        .sidebar button { text-align:left; padding:10px 12px; border-radius:8px; color:#c9d1d9; background:transparent; }
        .sidebar button:hover { background:#21262d; }
        .sidebar button.active { background:#1f6feb; color:white; }
        .content { display:flex; flex-direction:column; flex-grow:1; padding:28px; gap:18px; overflow-y:auto; }
        h1 { font-size:26px; font-weight:700; }
        .card { background:#161b22; border:1px solid #30363d; border-radius:12px; padding:20px; gap:10px; display:flex; flex-direction:column; }
        .row { display:flex; flex-direction:row; gap:10px; align-items:center; }
        .primary { background:#238636; color:white; padding:10px 18px; border-radius:8px; }
        .primary:hover { background:#2ea043; }
        .danger { color:#f85149; }
        .ok { color:#3fb950; }
        input { background:#0d1117; border:1px solid #30363d; border-radius:6px; padding:8px; color:#e6edf3; }
        /* Skeleton shimmer (used by Query loading state). */
        .skeleton { display:flex; flex-direction:column; gap:10px; }
        .skeleton-line { height:14px; border-radius:6px; background:#21262d; }
        .skeleton-box { background:#21262d; border-radius:10px; }
    )");

    // -------------------------------------------------------------------------
    //  Layout shell: a sidebar that highlights the active route + a content slot
    //  (Div with id "__content__") where each route's view mounts.
    // -------------------------------------------------------------------------
    app.setLayout([&](const Element& content) {
        auto navBtn = [&](const std::string& label, const std::string& path) {
            std::string cls = (app.route() == path) ? "active" : "";
            return Button(label).className(cls).onClick([&app, path]{ app.navigate(path); });
        };
        return Div({
            Nav({
                El("h2", "FluxUI"),
                navBtn("Store",     "/store"),
                navBtn("Form (Zod)","/form"),
                navBtn("Query",     "/query"),
                navBtn("User #7",   "/user/7?tab=info"),
            }).className("sidebar"),
            Div({ content }).id("__content__").className("content"),
        }).className("root");
    });

    // -------------------------------------------------------------------------
    //  Route 1 — Store<T>: global state shared between this view and the sidebar.
    // -------------------------------------------------------------------------
    app.addRoute("/store", []{
        auto& cart = useStore<CartState>();
        return Div({
            H1("Store (Zustand-style)"),
            Div({
                P("A single global store, mutated via a reducer. Reactive Text re-renders on change."),
                Text([&cart]{ return "Items in cart: " + std::to_string(cart.get().count); }),
                Text([&cart]{ return "Last added: " + (cart.get().lastItem.empty() ? "(none)" : cart.get().lastItem); }),
                Div({
                    Button("Add apple").className("primary")
                        .onClick([&cart]{ cart.set([](CartState& s){ s.count++; s.lastItem = "apple"; }); }),
                    Button("Add book").className("primary")
                        .onClick([&cart]{ cart.set([](CartState& s){ s.count++; s.lastItem = "book"; }); }),
                    Button("Reset")
                        .onClick([&cart]{ cart.set([](CartState& s){ s.count = 0; s.lastItem = ""; }); }),
                }).className("row"),
            }).className("card"),
        });
    });

    // -------------------------------------------------------------------------
    //  Route 2 — Schema + Rule: Zod-style validation of a tiny form.
    // -------------------------------------------------------------------------
    app.addRoute("/form", []{
        static auto result = State<std::string>("Fill the form and submit.");
        // Capture the inputs with Refs so we can read their value on submit.
        static auto emailRef = Ref<FluxUI::TextInput>();
        static auto ageRef   = Ref<FluxUI::TextInput>();
        static auto nickRef  = Ref<FluxUI::TextInput>();

        auto onSubmit = []{
            std::string email = emailRef ? emailRef->value : "";
            std::string age   = ageRef   ? ageRef->value   : "";
            std::string nick  = nickRef  ? nickRef->value  : "";
            auto res = signupSchema().validate({
                {"email", email}, {"age", age}, {"nick", nick},
            });
            result.set(res.ok ? "Valid! Welcome aboard." : ("Invalid: " + res.first()));
        };

        return Div({
            H1("Schema validation (Zod-style)"),
            Div({
                Input("email", "email").onMount(emailRef),
                Input("number", "age (18-120)").onMount(ageRef),
                Input("text", "nick (3-12 chars)").onMount(nickRef),
                Button("Submit").className("primary").onClick(onSubmit),
                Text([&]{ return result.get(); })
                    .onMount([](FluxUI::Widget* w){ w->className = "ok"; }),
            }).className("card"),
        });
    });

    // -------------------------------------------------------------------------
    //  Route 3 — Query<T> + Skeleton: async fetch with loading placeholders.
    // -------------------------------------------------------------------------
    app.addRoute("/query", []{
        // A fake API call that takes ~1.2s and returns a payload.
        static Query<std::string> users([]{
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
            return std::string("Ada Lovelace, Alan Turing, Grace Hopper");
        });
        if (users.status() == QueryStatus::Idle) users.start();

        return Div({
            H1("Query (React-Query-style)"),
            Div({
                P("Fetches on a background thread. Shows a Skeleton while loading."),
                // .view() picks the right Element per state — Skeleton on loading.
                users.view(
                    []{ return Skeleton(3); },
                    [](const std::string& data){ return Text("Users: " + data).onMount([](FluxUI::Widget* w){ w->className = "ok"; }); },
                    [](const std::string& err){ return Text("Error: " + err).onMount([](FluxUI::Widget* w){ w->className = "danger"; }); }
                ),
                Button("Refetch").className("primary").onClick([]{ users.refetch(); }),
            }).className("card"),
        });
    });

    // -------------------------------------------------------------------------
    //  Route 4 — Route params + query string: "/user/:id?tab=..."
    // -------------------------------------------------------------------------
    app.addRoute("/user/:id", []{
        std::string id  = App::param("id");
        std::string tab = App::query("tab", "profile");
        return Div({
            H1("User #" + id),
            Div({
                P("Route pattern: /user/:id  — captured from the URL path."),
                Text("id param  = " + id),
                Text("tab query = " + tab),
                Div({
                    Button("Go to user 42").onClick([&]{ App::current().navigate("/user/42?tab=settings"); }),
                    Button("Go to user 99").onClick([&]{ App::current().navigate("/user/99?tab=info"); }),
                }).className("row"),
            }).className("card"),
        });
    });

    app.build("/store");
    return app.run();
}
