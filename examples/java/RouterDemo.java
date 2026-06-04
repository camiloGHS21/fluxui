import io.fluxui.App;
import io.fluxui.Backend;
import io.fluxui.Dsl;
import io.fluxui.FluxUI;

import java.nio.file.Path;

import views.Views;

/** FluxUI Java file-based routing demo — views self-register, no codegen. */
public final class RouterDemo {
    public static void main(String[] args) {
        if (args.length > 0) {
            FluxUI.loadFrom(Path.of(args[0]));
        }

        try (App app = App.create()) {
            app.setBackend(Backend.AUTO);
            if (!app.init("FluxUI Java Router Demo", 1000, 700)) {
                return;
            }
            app.loadDefaultFont(16.0f);
            app.addStylesheet(
                ".app { display: flex; flex-direction: row; width: 100%; height: 100%; }" +
                ".sidebar { display: flex; flex-direction: column; width: 220px; background-color: #111; padding: 16px; gap: 8px; }" +
                ".content { display: flex; flex-direction: column; flex-grow: 1; padding: 24px; }" +
                ".title { font-size: 24px; font-weight: 700; }"
            );

            Views.load();  // trigger view self-registration

            Dsl.Router router = Dsl.router(app);
            router.useViews();  // file-based routing: wire all registered views

            router.setLayout(content -> Dsl.div(
                Dsl.nav(
                    Dsl.button("Dashboard").onClick(() -> router.navigate("/dashboard")),
                    Dsl.button("Settings").onClick(() -> router.navigate("/settings"))
                ).className("sidebar"),
                Dsl.div(content).className("content")
            ).className("app"));

            router.runReactive();
        }
    }
}
