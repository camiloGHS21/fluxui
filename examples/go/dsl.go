//go:build ignore

// FluxUI Declarative DSL example in Go — modern HTML/Blink-named API.
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
		.app { display: flex; flex-direction: row; width: 100%; height: 100%; }
		.sidebar { display: flex; flex-direction: column; width: 250px; background-color: #111115; padding: 16px; gap: 8px; }
		.content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; gap: 16px; }
		h1 { font-size: 24px; font-weight: 700; }
		.metrics { display: flex; flex-direction: row; gap: 16px; }
		.metric-card { display: flex; flex-direction: column; background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }
		.primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }
	`)

	devices := fluxui.NewState(128)

	app.SetRoot(
		fluxui.Div(
			fluxui.Nav(
				fluxui.Button("Dashboard"),
				fluxui.Button("Dispositivos"),
				fluxui.Button("Backups"),
				fluxui.Button("Seguridad"),
			).Class("sidebar"),

			fluxui.Div(
				fluxui.H1("CompanyGuard"),
				fluxui.Div(
					fluxui.Div(
						fluxui.Span("Equipos activos"),
						fluxui.TextFn(func() string { return strconv.Itoa(devices.Get()) }),
					).Class("metric-card"),
					fluxui.Div(
						fluxui.Span("Alertas"),
						fluxui.Span("7"),
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
