import io.fluxui.App;
import io.fluxui.FluxUI;
import io.fluxui.Keys;
import io.fluxui.Modifiers;

import java.nio.file.Path;

public final class ActionSmoke {
    public static void main(String[] args) {
        FluxUI.loadFrom(Path.of(args.length > 0 ? args[0] : "build/Release"));

        try (App app = App.create()) {
            boolean[] called = {false};
            app.addAction("app.ping", Keys.ENTER, Modifiers.CTRL, action -> {
                called[0] = "app.ping".equals(action);
            });
            if (!app.dispatchAction("app.ping") || !called[0]) {
                throw new IllegalStateException("FluxUI Java action dispatch failed");
            }
            System.out.println("FluxUI Java action dispatch ok");
        }
    }
}
