import io.fluxui.App;
import io.fluxui.Backend;
import io.fluxui.Dsl;
import io.fluxui.FluxUI;
import io.fluxui.Keys;
import io.fluxui.Modifiers;
import io.fluxui.State;

import java.nio.file.Path;

/** FluxUI Java minimal example — modern HTML/Blink-named declarative DSL. */
public final class Minimal {
    public static void main(String[] args) {
        if (args.length > 0) {
            FluxUI.loadFrom(Path.of(args[0]));
        }

        try (App app = App.create()) {
            app.setBackend(Backend.AUTO);
            if (!app.init("FluxUI Java Example", 900, 600)) {
                return;
            }

            if (!app.loadDefaultFont(16.0f)) {
                app.loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
            }
            app.addStylesheet(
                ".root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 16px; }" +
                ".title { font-size: 26px; font-weight: 700; color: #edf3f8; }" +
                ".body { font-size: 14px; color: rgba(237, 243, 248, 0.68); }" +
                ".button { width: 200px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; }"
            );

            State<Integer> clicks = new State<>(0);

            Dsl.setRoot(app,
                Dsl.div(
                    Dsl.h1("Hello from Java").className("title"),
                    Dsl.p("This app uses the declarative HTML-named FluxUI DSL.").className("body"),
                    Dsl.textFn(() -> "Clicked " + clicks.get() + " times").className("body"),
                    Dsl.button("Click Me").className("button")
                        .onClick(() -> clicks.set(clicks.get() + 1)),
                    Dsl.button("Close").className("button").onClick(app::stop)
                ).className("root"));

            app.addAction("app.close", Keys.ESCAPE, Modifiers.NONE, action -> app.stop());
            Dsl.runReactive(app);
        }
    }
}
