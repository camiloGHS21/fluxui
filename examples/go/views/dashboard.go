// View: /dashboard — self-registers via init() (file-based routing, no codegen).
package main

import fluxui "github.com/camiloGHS21/DataleakGuard/bindings/go"

func init() { fluxui.RegisterView("/dashboard", DashboardView) }

func DashboardView() *fluxui.Element {
	return fluxui.Div(
		fluxui.H1("Dashboard").Class("title"),
		fluxui.P("Welcome to the auto-routed dashboard.").Class("body"),
	).Class("page")
}
