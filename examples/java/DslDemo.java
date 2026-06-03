import io.fluxui.App;
import io.fluxui.Backend;
import io.fluxui.Dsl;
import io.fluxui.FluxUI;
import io.fluxui.State;

import java.nio.file.Path;

/** FluxUI Declarative DSL example in Java — modern HTML/Blink-named API. */
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
                ".app { display: flex; flex-direction: row; width: 100%; height: 100%; }" +
                ".sidebar { display: flex; flex-direction: column; width: 250px; background-color: #111115; padding: 16px; gap: 8px; }" +
                ".content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; gap: 16px; }" +
                "h1 { font-size: 24px; font-weight: 700; }" +
                ".metrics { display: flex; flex-direction: row; gap: 16px; }" +
                ".metric-card { display: flex; flex-direction: column; background-color: #1e1e2e; padding: 20px; border-radius: 12px; gap: 8px; }" +
                ".primary { background-color: #3b82f6; color: white; padding: 12px 24px; border-radius: 8px; }"
            );

            State<Integer> devices = new State<>(128);

            Dsl.setRoot(app,
                Dsl.div(
                    Dsl.nav(
                        Dsl.button("Dashboard"),
                        Dsl.button("Dispositivos"),
                        Dsl.button("Backups"),
                        Dsl.button("Seguridad")
                    ).className("sidebar"),

                    Dsl.div(
                        Dsl.h1("CompanyGuard"),
                        Dsl.div(
                            Dsl.div(
                                Dsl.span("Equipos activos"),
                                Dsl.textFn(() -> String.valueOf(devices.get()))
                            ).className("metric-card"),
                            Dsl.div(
                                Dsl.span("Alertas"),
                                Dsl.span("7")
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
