import io.fluxui.App;
import io.fluxui.Backend;
import io.fluxui.FluxUI;
import io.fluxui.Keys;
import io.fluxui.Modifiers;
import io.fluxui.Widget;

import java.nio.file.Path;

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
            app.warmFontCache(new float[] {14.0f, 16.0f, 26.0f});
            app.releaseFontSources();
            app.addStylesheet(
                ".root { display: flex; flex-direction: column; background-color: #101418; padding: 32px; gap: 16px; }" +
                ".title { height: 36px; font-size: 26px; font-weight: 700; color: #edf3f8; }" +
                ".body { height: 24px; font-size: 14px; color: rgba(237, 243, 248, 0.68); }" +
                ".button { width: 140px; height: 44px; border-radius: 8px; background-color: #37c6a3; color: #06100d; }"
            );

            Widget root = app.root();
            root.reserveChildren(3);
            root.addText("Hello from Java", "title");
            root.addText("This app uses the native FluxUI Java binding.", "body");
            root.addButton("Close", "button").setOnClick(app::stop);
            app.addAction("app.close", Keys.ESCAPE, Modifiers.NONE, action -> app.stop());

            app.run();
        }
    }
}
