"""FluxUI Ecosystem Demo (Python) — the modern Next.js/Zod/Zustand-style toolkit.

Shows the five ecosystem features in one app:
  1. Routing + params  — add_route("/user/:id"), app.param("id"), app.query()
  2. Store             — Zustand-style global state shared across views
  3. Schema + Rule     — Zod-style runtime validation
  4. Query             — async fetch with idle/loading/success/error + view()
  5. Skeleton          — loading placeholders while a Query is pending

Run with: python examples/python/ecosystem.py
"""
import os
import sys
import time

sys.path.append(os.path.join(os.path.dirname(__file__), "../../bindings/python"))

import fluxui

# Global store (Zustand-style): a tiny cart shared by every view.
cart = fluxui.Store({"count": 0, "last": ""})


def signup_schema():
    return (fluxui.Schema()
            .field("email", fluxui.Rule.string().email())
            .field("age", fluxui.Rule.number().min(18).max(120))
            .field("nick", fluxui.Rule.string().min_length(3).max_length(12)))


# A fake API query that takes ~1.2s to resolve.
def fetch_users():
    time.sleep(1.2)
    return "Ada Lovelace, Alan Turing, Grace Hopper"


users = fluxui.Query(fetch_users)


def main():
    app = fluxui.DslApp(1100, 760, "FluxUI Ecosystem (Python)")
    app.add_css("""
        .root { display:flex; flex-direction:row; background:#0d1117; color:#e6edf3; }
        .sidebar { display:flex; flex-direction:column; width:220px; background:#161b22; padding:18px; gap:6px; }
        .sidebar h2 { font-size:18px; font-weight:700; margin-bottom:10px; color:#58a6ff; }
        .sidebar button { text-align:left; padding:10px 12px; border-radius:8px; color:#c9d1d9; }
        .sidebar button.active { background:#1f6feb; color:white; }
        .content { display:flex; flex-direction:column; flex-grow:1; padding:28px; gap:18px; }
        h1 { font-size:26px; font-weight:700; }
        .card { background:#161b22; border:1px solid #30363d; border-radius:12px; padding:20px; gap:10px; display:flex; flex-direction:column; }
        .row { display:flex; flex-direction:row; gap:10px; align-items:center; }
        .primary { background:#238636; color:white; padding:10px 18px; border-radius:8px; }
        .ok { color:#3fb950; }
        .danger { color:#f85149; }
        .skeleton { display:flex; flex-direction:column; gap:10px; }
        .skeleton-line { height:14px; border-radius:6px; background:#21262d; }
    """)

    # Layout shell: sidebar highlights the active route + a content slot.
    def nav_btn(label, path):
        cls = "active" if app.route() == path else ""
        return fluxui.Button(label).cls(cls).on_click(lambda: app.navigate(path))

    app.set_layout(lambda content: fluxui.Div([
        fluxui.Nav([
            fluxui.El("h2", [fluxui.Text("FluxUI")]),
            nav_btn("Store", "/store"),
            nav_btn("Form (Zod)", "/form"),
            nav_btn("Query", "/query"),
            nav_btn("User #7", "/user/7?tab=info"),
        ]).cls("sidebar"),
        fluxui.Div([content] if content else []).cls("content"),
    ]).cls("root"))

    # Route 1 — Store: global state shared between view and sidebar.
    def store_view():
        return fluxui.Div([
            fluxui.H1("Store (Zustand-style)"),
            fluxui.Div([
                fluxui.P("A single global store, mutated via a reducer."),
                fluxui.TextFn(lambda: "Items in cart: " + str(cart.get()["count"])),
                fluxui.TextFn(lambda: "Last added: " + (cart.get()["last"] or "(none)")),
                fluxui.Div([
                    fluxui.Button("Add apple").cls("primary").on_click(
                        lambda: cart.set(lambda s: s.update(count=s["count"] + 1, last="apple"))),
                    fluxui.Button("Add book").cls("primary").on_click(
                        lambda: cart.set(lambda s: s.update(count=s["count"] + 1, last="book"))),
                    fluxui.Button("Reset").on_click(
                        lambda: cart.set(lambda s: s.update(count=0, last=""))),
                ]).cls("row"),
            ]).cls("card"),
        ])

    # Route 2 — Schema + Rule: Zod-style validation of sample input.
    def form_view():
        result = fluxui.State("Press a button to validate a sample payload.")

        def validate(data):
            res = signup_schema().validate(data)
            result.set("Valid! Welcome aboard." if res.ok else "Invalid: " + res.first())

        return fluxui.Div([
            fluxui.H1("Schema validation (Zod-style)"),
            fluxui.Div([
                fluxui.TextFn(lambda: result.get()).cls("ok"),
                fluxui.Div([
                    fluxui.Button("Validate good payload").cls("primary").on_click(
                        lambda: validate({"email": "a@b.com", "age": "25", "nick": "joe"})),
                    fluxui.Button("Validate bad payload").on_click(
                        lambda: validate({"email": "nope", "age": "12", "nick": "x"})),
                ]).cls("row"),
            ]).cls("card"),
        ])

    # Route 3 — Query + Skeleton: async fetch with loading placeholders.
    def query_view():
        if users.status == fluxui.Query.IDLE:
            users.start()
        return fluxui.Div([
            fluxui.H1("Query (React-Query-style)"),
            fluxui.Div([
                fluxui.P("Fetches on a background thread. Shows a Skeleton while loading."),
                users.view(
                    lambda: fluxui.Skeleton(3),
                    lambda data: fluxui.Text("Users: " + data).cls("ok"),
                    lambda err: fluxui.Text("Error: " + err).cls("danger"),
                ),
                fluxui.Button("Refetch").cls("primary").on_click(lambda: users.refetch()),
            ]).cls("card"),
        ])

    # Route 4 — Route params + query string: "/user/:id?tab=..."
    def user_view():
        uid = app.param("id")
        tab = app.query("tab", "profile")
        return fluxui.Div([
            fluxui.H1("User #" + uid),
            fluxui.Div([
                fluxui.P("Route pattern: /user/:id — captured from the URL path."),
                fluxui.Text("id param  = " + uid),
                fluxui.Text("tab query = " + tab),
                fluxui.Div([
                    fluxui.Button("Go to user 42").on_click(lambda: app.navigate("/user/42?tab=settings")),
                    fluxui.Button("Go to user 99").on_click(lambda: app.navigate("/user/99?tab=info")),
                ]).cls("row"),
            ]).cls("card"),
        ])

    app.add_route("/store", store_view)
    app.add_route("/form", form_view)
    app.add_route("/query", query_view)
    app.add_route("/user/:id", user_view)

    app.build("/store")
    app.run_reactive()


if __name__ == "__main__":
    main()
