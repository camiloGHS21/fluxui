import io.fluxui.App;
import io.fluxui.Backend;
import io.fluxui.Dsl;
import io.fluxui.FluxUI;
import io.fluxui.State;

import java.nio.file.Path;

/** FluxUI Declarative DSL example in Java — mirrors the C++ demo_dsl.cpp. */
public final class DslDemo {
    public static void main(String[] args) {
        if (args.length > 0) {
            FluxUI.loadFrom(Path.of(args[0]));
        }

        try (App app = App.create()) {
            app.setBackend(Backend.AUTO);
            if (!app.init("CompanyGuard", 1200, 800)) {
                return;
            }
            app.loadDefaultFont(16.0f);
            app.addStylesheet(
                ".app { width: 100%; height: 100%; }" +
                ".sidebar { width: 250px; background-color: #111115; padding: 16px; gap: 8px; }" +
                ".content { flex-grow: 1; padding: 24px; gap: 16px; }" +
                ".h1 { font-size: 24px; font-weight: 700; }" +
                ".metric-card { background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }" +
                ".primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }"
            );

            State<Integer> devices = new State<>(128);

            Dsl.setRoot(app,
                Dsl.row(
                    Dsl.sidebar(
                        Dsl.navItem("Dashboard"),
                        Dsl.navItem("Dispositivos"),
                        Dsl.navItem("Backups"),
                        Dsl.navItem("Seguridad")
                    ).className("sidebar"),

                    Dsl.column(
                        Dsl.text("CompanyGuard").className("h1"),
                        Dsl.row(
                            Dsl.card(
                                Dsl.text("Equipos activos"),
                                Dsl.textFn(() -> String.valueOf(devices.get()))
                            ).className("metric-card"),
                            Dsl.card(
                                Dsl.text("Alertas"),
                                Dsl.text("7")
                            ).className("metric-card")
                        ).className("metrics"),
                        Dsl.button("Escanear ahora")
                            .className("primary")
                            .onClick(() -> devices.set(devices.get() + 1))
                    ).className("content")
                ).className("app"));

            Dsl.runReactive(app);
        }
    }
}
