import io.fluxui.App;
import io.fluxui.FluxUI;

import java.nio.file.Path;

public final class Smoke {
    public static void main(String[] args) {
        FluxUI.loadFrom(Path.of(args.length > 0 ? args[0] : "build/Release"));
        try (App ignored = App.create()) {
            System.out.println("FluxUI Java binding loaded");
        }
    }
}
