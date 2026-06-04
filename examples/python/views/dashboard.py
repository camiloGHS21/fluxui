"""View: /dashboard — auto-discovered by app.auto_routes() (Next.js style)."""
import fluxui


def view():
    return fluxui.Div([
        fluxui.H1("Dashboard").cls("title"),
        fluxui.P("Welcome to the auto-routed dashboard.").cls("body"),
    ]).cls("page")
