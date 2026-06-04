"""View: /settings — auto-discovered by app.auto_routes() (Next.js style)."""
import fluxui


def view():
    return fluxui.Div([
        fluxui.H1("Settings").cls("title"),
        fluxui.P("Configure your preferences here.").cls("body"),
    ]).cls("page")
