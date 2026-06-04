//go:build ignore

// FluxUI Go file-based routing demo — like Next.js, NO generated files.
// Each views/*.go self-registers via its init(). main just calls UseViews().
//
//	go run ./examples/go/views
package main

import (
	"runtime"

	fluxui "github.com/camiloGHS21/DataleakGuard/bindings/go"
)

func main() {
	runtime.LockOSThread()

	app := fluxui.NewApp(1000, 700, "FluxUI Go Router Demo")
	if app == nil {
		return
	}
	defer app.Destroy()

	app.AddCSS(`
		.app { display: flex; flex-direction: row; width: 100%; height: 100%; }
		.sidebar { display: flex; flex-direction: column; width: 220px; background-color: #111; padding: 16px; gap: 8px; }
		.content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; }
		.title { font-size: 24px; font-weight: 700; }
	`)

	// File-based routing: every view self-registered via init(). No codegen.
	app.UseViews()

	app.SetLayout(func(content *fluxui.Element) *fluxui.Element {
		return fluxui.Div(
			fluxui.Nav(
				fluxui.Button("Dashboard").OnClick(func() { app.Navigate("/dashboard") }),
				fluxui.Button("Settings").OnClick(func() { app.Navigate("/settings") }),
			).Class("sidebar"),
			fluxui.Div(content).Class("content"),
		).Class("app")
	})

	app.RunReactive()
}
