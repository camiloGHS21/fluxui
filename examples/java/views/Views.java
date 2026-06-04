// File-based view definitions. Each view self-registers via a static block so
// the router picks them up with router.useViews() (no codegen).
package views;

import io.fluxui.Dsl;
import io.fluxui.Node;

public final class Views {
    static {
        Dsl.registerView("/dashboard", Views::dashboardView);
        Dsl.registerView("/settings", Views::settingsView);
    }

    public static Node dashboardView() {
        return Dsl.div(
            Dsl.h1("Dashboard").className("title"),
            Dsl.p("Welcome to the auto-routed dashboard.").className("body")
        ).className("page");
    }

    public static Node settingsView() {
        return Dsl.div(
            Dsl.h1("Settings").className("title"),
            Dsl.p("Configure your preferences here.").className("body")
        ).className("page");
    }

    // Force static init (loads the registrations).
    public static void load() {}
}
