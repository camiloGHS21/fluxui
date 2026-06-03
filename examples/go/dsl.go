//go:build ignore

// FluxUI Declarative DSL example in Go — mirrors the C++ demo_dsl.cpp.
// Run with: go run examples/go/dsl.go
package main

import (
	"runtime"
	"strconv"

	fluxui "github.com/camiloGHS21/DataleakGuard/bindings/go"
)

func main() {
	runtime.LockOSThread()

	app := fluxui.NewApp(1200, 800, "CompanyGuard")
	if app == nil {
		return
	}
	defer app.Destroy()

	app.AddCSS(`
		.app { width: 100%; height: 100%; }
		.sidebar { width: 250px; background-color: #111115; padding: 16px; gap: 8px; }
		.content { flex-grow: 1; padding: 24px; gap: 16px; }
		.h1 { font-size: 24px; font-weight: 700; }
		.metric-card { background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }
		.primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }
	`)

	devices := fluxui.NewState(128)

	app.SetRoot(
		fluxui.Row(
			fluxui.Sidebar(
				fluxui.NavItem("Dashboard"),
				fluxui.NavItem("Dispositivos"),
				fluxui.NavItem("Backups"),
				fluxui.NavItem("Seguridad"),
			).Class("sidebar"),

			fluxui.Column(
				fluxui.Text("CompanyGuard").Class("h1"),
				fluxui.Row(
					fluxui.Card(
						fluxui.Text("Equipos activos"),
						fluxui.TextFn(func() string { return strconv.Itoa(devices.Get()) }),
					).Class("metric-card"),
					fluxui.Card(
						fluxui.Text("Alertas"),
						fluxui.Text("7"),
					).Class("metric-card"),
				).Class("metrics"),
				fluxui.Button("Escanear ahora").
					Class("primary").
					OnClick(func() {
						devices.Set(devices.Get() + 1)
					}),
			).Class("content"),
		).Class("app"),
	)

	app.RunReactive()
}
