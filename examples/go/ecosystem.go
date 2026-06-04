//go:build ignore

// FluxUI Ecosystem Demo (Go) — the modern Next.js/Zod/Zustand-style toolkit.
//
// Shows the five ecosystem features in one app:
//   1. Routing + params  — AddRoute("/user/:id"), app.Param("id"), app.Query()
//   2. Store[T]          — Zustand-style global state shared across views
//   3. Schema + Rule     — Zod-style runtime validation
//   4. Query[T]          — async fetch with Idle/Loading/Success/Error + View()
//   5. Skeleton          — loading placeholders while a Query is pending
//
// Run with: go run examples/go/ecosystem.go
package main

import (
	"runtime"
	"strconv"
	"time"

	fluxui "github.com/camiloGHS21/DataleakGuard/bindings/go"
)

// Global store (Zustand-style): a tiny cart shared by every view.
type CartState struct {
	Count int
	Last  string
}

var cart = fluxui.NewStore(CartState{})

// A schema (Zod-style) validating a sign-up form.
func signupSchema() *fluxui.Schema {
	return fluxui.NewSchema().
		Field("email", fluxui.RuleString().Email()).
		Field("age", fluxui.RuleNumber().Min(18).Max(120)).
		Field("nick", fluxui.RuleString().MinLength(3).MaxLength(12))
}

// A fake API query that takes ~1.2s to resolve.
var users = fluxui.NewQuery(func() (string, error) {
	time.Sleep(1200 * time.Millisecond)
	return "Ada Lovelace, Alan Turing, Grace Hopper", nil
})

func main() {
	runtime.LockOSThread()

	app := fluxui.NewApp(1100, 760, "FluxUI Ecosystem (Go)")
	if app == nil {
		return
	}
	defer app.Destroy()

	app.AddCSS(`
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
	`)

	// Layout shell: sidebar highlights the active route + a content slot.
	navBtn := func(label, path string) *fluxui.Element {
		cls := ""
		if app.Route() == path {
			cls = "active"
		}
		return fluxui.Button(label).Class(cls).OnClick(func() { app.Navigate(path) })
	}
	app.SetLayout(func(content *fluxui.Element) *fluxui.Element {
		if content == nil {
			content = fluxui.Div()
		}
		return fluxui.Div(
			fluxui.Nav(
				fluxui.El("h2", fluxui.Text("FluxUI")),
				navBtn("Store", "/store"),
				navBtn("Form (Zod)", "/form"),
				navBtn("Query", "/query"),
				navBtn("User #7", "/user/7?tab=info"),
			).Class("sidebar"),
			fluxui.Div(content).ID("__content__").Class("content"),
		).Class("root")
	})

	// Route 1 — Store: global state shared between view and sidebar.
	app.AddRoute("/store", func() *fluxui.Element {
		return fluxui.Div(
			fluxui.H1("Store (Zustand-style)"),
			fluxui.Div(
				fluxui.P("A single global store, mutated via a reducer."),
				fluxui.TextFn(func() string { return "Items in cart: " + strconv.Itoa(cart.Get().Count) }),
				fluxui.TextFn(func() string {
					last := cart.Get().Last
					if last == "" {
						last = "(none)"
					}
					return "Last added: " + last
				}),
				fluxui.Div(
					fluxui.Button("Add apple").Class("primary").OnClick(func() {
						cart.Set(func(s *CartState) { s.Count++; s.Last = "apple" })
					}),
					fluxui.Button("Add book").Class("primary").OnClick(func() {
						cart.Set(func(s *CartState) { s.Count++; s.Last = "book" })
					}),
					fluxui.Button("Reset").OnClick(func() {
						cart.Set(func(s *CartState) { s.Count = 0; s.Last = "" })
					}),
				).Class("row"),
			).Class("card"),
		)
	})

	// Route 2 — Schema + Rule: Zod-style validation of sample input.
	app.AddRoute("/form", func() *fluxui.Element {
		result := fluxui.NewState("Press a button to validate a sample payload.")
		validate := func(data map[string]string) {
			res := signupSchema().Validate(data)
			if res.OK {
				result.Set("Valid! Welcome aboard.")
			} else {
				for _, msg := range res.Errors {
					result.Set("Invalid: " + msg)
					break
				}
			}
		}
		return fluxui.Div(
			fluxui.H1("Schema validation (Zod-style)"),
			fluxui.Div(
				fluxui.TextFn(func() string { return result.Get() }).Class("ok"),
				fluxui.Div(
					fluxui.Button("Validate good payload").Class("primary").OnClick(func() {
						validate(map[string]string{"email": "a@b.com", "age": "25", "nick": "joe"})
					}),
					fluxui.Button("Validate bad payload").OnClick(func() {
						validate(map[string]string{"email": "nope", "age": "12", "nick": "x"})
					}),
				).Class("row"),
			).Class("card"),
		)
	})

	// Route 3 — Query + Skeleton: async fetch with loading placeholders.
	app.AddRoute("/query", func() *fluxui.Element {
		if users.Status() == fluxui.QueryIdle {
			users.Start()
		}
		return fluxui.Div(
			fluxui.H1("Query (React-Query-style)"),
			fluxui.Div(
				fluxui.P("Fetches on a goroutine. Shows a Skeleton while loading."),
				users.View(
					func() *fluxui.Element { return fluxui.Skeleton(3) },
					func(data string) *fluxui.Element { return fluxui.Text("Users: " + data).Class("ok") },
					func(err string) *fluxui.Element { return fluxui.Text("Error: " + err).Class("danger") },
				),
				fluxui.Button("Refetch").Class("primary").OnClick(func() { users.Refetch() }),
			).Class("card"),
		)
	})

	// Route 4 — Route params + query string: "/user/:id?tab=..."
	app.AddRoute("/user/:id", func() *fluxui.Element {
		id := app.Param("id")
		tab := app.Query("tab")
		if tab == "" {
			tab = "profile"
		}
		return fluxui.Div(
			fluxui.H1("User #"+id),
			fluxui.Div(
				fluxui.P("Route pattern: /user/:id — captured from the URL path."),
				fluxui.Text("id param  = "+id),
				fluxui.Text("tab query = "+tab),
				fluxui.Div(
					fluxui.Button("Go to user 42").OnClick(func() { app.Navigate("/user/42?tab=settings") }),
					fluxui.Button("Go to user 99").OnClick(func() { app.Navigate("/user/99?tab=info") }),
				).Class("row"),
			).Class("card"),
		)
	})

	app.Build("/store")
	app.RunReactive()
}
