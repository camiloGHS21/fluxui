// View: /settings — self-registers via init() (file-based routing, no codegen).
package main

import fluxui "github.com/camiloGHS21/DataleakGuard/bindings/go"

func init() { fluxui.RegisterView("/settings", SettingsView) }

func SettingsView() *fluxui.Element {
	return fluxui.Div(
		fluxui.H1("Settings").Class("title"),
		fluxui.P("Configure your preferences here.").Class("body"),
	).Class("page")
}
