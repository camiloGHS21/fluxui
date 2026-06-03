// FluxUI Go minimal example — modern HTML/Blink-named declarative DSL.
package main

import (
	"fmt"
	"runtime"

	fluxui "github.com/camiloGHS21/DataleakGuard/bindings/go"
)

func main() {
	// UI frameworks require running on the main OS thread.
	runtime.LockOSThread()

	app := fluxui.NewApp(800, 600, "FluxUI Go Minimal Example")
	if app == nil {
		fmt.Println("Failed to initialize FluxUI App")
		return
	}
	defer app.Destroy()

	app.AddCSS(`
		.root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 16px; }
		.title { font-size: 26px; font-weight: 700; color: #edf3f8; }
		.body { font-size: 14px; color: rgba(237, 243, 248, 0.68); }
		.btn { width: 200px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; }
	`)

	clicks := fluxui.NewState(0)

	app.SetRoot(
		fluxui.Div(
			fluxui.H1("Welcome to FluxUI from Go!").Class("title"),
			fluxui.P("This window is built with the declarative HTML-named DSL.").Class("body"),
			fluxui.TextFn(func() string {
				return fmt.Sprintf("Button clicked %d times", clicks.Get())
			}).Class("body"),
			fluxui.Button("Click Me").Class("btn").OnClick(func() {
				clicks.Set(clicks.Get() + 1)
			}),
			fluxui.Button("Exit").Class("btn").OnClick(func() {
				app.Stop()
			}),
		).Class("root"),
	)

	app.RunReactive()
}
