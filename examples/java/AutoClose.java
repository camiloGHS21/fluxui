import io.fluxui.App;
import io.fluxui.Backend;
import io.fluxui.FluxUI;
import io.fluxui.Widget;

import java.nio.file.Path;

public final class AutoClose {
    public static void main(String[] args) {
        FluxUI.loadFrom(Path.of(args.length > 0 ? args[0] : "build/Release"));

        try (App app = App.create()) {
            app.setBackend(Backend.AUTO);
            if (!app.init("FluxUI Java AutoClose", 480, 260)) {
                throw new IllegalStateException("FluxUI failed to initialize");
            }

            if (!app.loadDefaultFont(16.0f)) {
                app.loadFont("C:/Windows/Fonts/segoeui.ttf", 16.0f);
            }
            app.warmFontCache(new float[] {14.0f, 16.0f, 24.0f});
            app.releaseFontSources();
            app.addStylesheet(
                ".root { display: flex; flex-direction: column; background-color: #101418; padding: 28px; gap: 14px; }" +
                ".title { height: 32px; font-size: 24px; font-weight: 700; color: #edf3f8; }" +
                ".body { height: 24px; font-size: 14px; color: rgba(237, 243, 248, 0.72); }"
            );

            Widget root = app.root();
            root.addText("Java update callback works", "title");
            root.addText("This window closes itself from Java.", "body");

            float[] elapsed = {0.0f};
            app.setUpdateCallback(deltaTime -> {
                elapsed[0] += deltaTime;
                if (elapsed[0] >= 0.35f) {
                    app.stop();
                }
            });

            app.run();
            System.out.println("FluxUI Java update callback ok");
        }
    }
}
