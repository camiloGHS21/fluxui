import io.fluxui.App;
import io.fluxui.Backend;
import io.fluxui.Dsl;
import io.fluxui.FluxUI;
import io.fluxui.Query;
import io.fluxui.Rule;
import io.fluxui.Schema;
import io.fluxui.State;
import io.fluxui.Store;

import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * FluxUI Ecosystem Demo (Java) — the modern Next.js/Zod/Zustand-style toolkit.
 *
 * Shows the five ecosystem features in one app:
 *   1. Routing + params  — addRoute("/user/:id"), router.param("id"), query()
 *   2. Store<T>          — Zustand-style global state shared across views
 *   3. Schema + Rule     — Zod-style runtime validation
 *   4. Query<T>          — async fetch with IDLE/LOADING/SUCCESS/ERROR + view()
 *   5. Dsl.skeleton      — loading placeholders for a pending Query
 */
public final class Ecosystem {

    /** Global store (Zustand-style): a tiny cart shared by every view. */
    static final class CartState {
        int count;
        String last = "(none)";
    }

    static final Store<CartState> cart = new Store<>(new CartState());

    static Schema signupSchema() {
        return new Schema()
            .field("email", Rule.string().email())
            .field("age", Rule.number().min(18).max(120))
            .field("nick", Rule.string().minLength(3).maxLength(12));
    }

    /** A fake API query that resolves to a fixed payload after a short delay. */
    static final Query<String> users = new Query<>(() -> {
        try {
            Thread.sleep(1200);
        } catch (InterruptedException ignored) {
            Thread.currentThread().interrupt();
        }
        return "Ada Lovelace, Alan Turing, Grace Hopper";
    });

    public static void main(String[] args) {
        if (args.length > 0) {
            FluxUI.loadFrom(Path.of(args[0]));
        }

        try (App app = App.create()) {
            app.setBackend(Backend.AUTO);
            if (!app.init("FluxUI Ecosystem (Java)", 1100, 760)) {
                return;
            }
            app.loadDefaultFont(16.0f);
            app.addStylesheet(
                ".root { display:flex; flex-direction:row; background:#0d1117; color:#e6edf3; }" +
                ".sidebar { display:flex; flex-direction:column; width:220px; background:#161b22; padding:18px; gap:6px; }" +
                ".sidebar button { text-align:left; padding:10px 12px; border-radius:8px; color:#c9d1d9; }" +
                ".sidebar button.active { background:#1f6feb; color:white; }" +
                ".content { display:flex; flex-direction:column; flex-grow:1; padding:28px; gap:18px; }" +
                "h1 { font-size:26px; font-weight:700; }" +
                ".card { background:#161b22; border:1px solid #30363d; border-radius:12px; padding:20px; gap:10px; display:flex; flex-direction:column; }" +
                ".row { display:flex; flex-direction:row; gap:10px; align-items:center; }" +
                ".primary { background:#238636; color:white; padding:10px 18px; border-radius:8px; }" +
                ".ok { color:#3fb950; } .danger { color:#f85149; }" +
                ".skeleton { display:flex; flex-direction:column; gap:10px; }" +
                ".skeleton-line { height:14px; border-radius:6px; background:#21262d; }"
            );

            Dsl.Router router = Dsl.router(app);

            // Route 1 — Store: global state shared between view and sidebar.
            router.addRoute("/store", () -> Dsl.div(
                Dsl.h1("Store (Zustand-style)"),
                Dsl.div(
                    Dsl.p("A single global store, mutated via a reducer."),
                    Dsl.textFn(() -> "Items in cart: " + cart.get().count),
                    Dsl.textFn(() -> "Last added: " + cart.get().last),
                    Dsl.div(
                        Dsl.button("Add apple").className("primary")
                            .onClick(() -> cart.set(s -> { s.count++; s.last = "apple"; })),
                        Dsl.button("Add book").className("primary")
                            .onClick(() -> cart.set(s -> { s.count++; s.last = "book"; })),
                        Dsl.button("Reset")
                            .onClick(() -> cart.set(s -> { s.count = 0; s.last = "(none)"; }))
                    ).className("row")
                ).className("card")
            ));

            // Route 2 — Schema + Rule: Zod-style validation of sample input.
            router.addRoute("/form", () -> {
                State<String> result = new State<>("Press a button to validate a sample payload.");
                return Dsl.div(
                    Dsl.h1("Schema validation (Zod-style)"),
                    Dsl.div(
                        Dsl.textFn(result::get).className("ok"),
                        Dsl.div(
                            Dsl.button("Validate good payload").className("primary").onClick(() -> {
                                Map<String, String> d = new LinkedHashMap<>();
                                d.put("email", "a@b.com");
                                d.put("age", "25");
                                d.put("nick", "joe");
                                Schema.Result res = signupSchema().validate(d);
                                result.set(res.ok ? "Valid! Welcome aboard." : "Invalid: " + res.first());
                            }),
                            Dsl.button("Validate bad payload").onClick(() -> {
                                Map<String, String> d = new LinkedHashMap<>();
                                d.put("email", "nope");
                                d.put("age", "12");
                                d.put("nick", "x");
                                Schema.Result res = signupSchema().validate(d);
                                result.set(res.ok ? "Valid! Welcome aboard." : "Invalid: " + res.first());
                            })
                        ).className("row")
                    ).className("card")
                );
            });

            // Route 3 — Query + Skeleton: async fetch with loading placeholders.
            router.addRoute("/query", () -> {
                if (users.status() == Query.Status.IDLE) {
                    users.start();
                }
                return Dsl.div(
                    Dsl.h1("Query (React-Query-style)"),
                    Dsl.div(
                        Dsl.p("Fetches on a background thread. Shows a Skeleton while loading."),
                        users.view(
                            () -> Dsl.skeleton(3),
                            data -> Dsl.text("Users: " + data).className("ok"),
                            err -> Dsl.text("Error: " + err).className("danger")
                        )
                    ).className("card")
                );
            });

            // Route 4 — Route params + query string: "/user/:id?tab=..."
            router.addRoute("/user/:id", () -> {
                String id = router.param("id");
                String tab = router.query("tab");
                if (tab.isEmpty()) {
                    tab = "profile";
                }
                final String tabFinal = tab;
                return Dsl.div(
                    Dsl.h1("User #" + id),
                    Dsl.div(
                        Dsl.p("Route pattern: /user/:id — captured from the URL path."),
                        Dsl.text("id param  = " + id),
                        Dsl.text("tab query = " + tabFinal),
                        Dsl.div(
                            Dsl.button("Go to user 42").onClick(() -> router.navigate("/user/42?tab=settings")),
                            Dsl.button("Go to user 99").onClick(() -> router.navigate("/user/99?tab=info"))
                        ).className("row")
                    ).className("card")
                );
            });

            // Layout shell: sidebar highlights the active route + a content slot.
            router.setLayout(content -> Dsl.div(
                Dsl.nav(
                    Dsl.el("h2", Dsl.text("FluxUI")),
                    navBtn(router, "Store", "/store"),
                    navBtn(router, "Form (Zod)", "/form"),
                    navBtn(router, "Query", "/query"),
                    navBtn(router, "User #7", "/user/7?tab=info")
                ).className("sidebar"),
                Dsl.div(content).id("__content__").className("content")
            ).className("root"));

            router.build("/store");
            router.runReactive();
        }
    }

    private static io.fluxui.Node navBtn(Dsl.Router router, String label, String path) {
        String cls = router.route().equals(path) ? "active" : "";
        return Dsl.button(label).className(cls).onClick(() -> router.navigate(path));
    }
}
