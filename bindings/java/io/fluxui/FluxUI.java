package io.fluxui;

import java.nio.file.Path;
import java.util.Objects;

public final class FluxUI {
    private static boolean loaded;

    private FluxUI() {
    }

    public static synchronized void load() {
        if (loaded) return;
        System.loadLibrary("fluxui_shared");
        System.loadLibrary("fluxui_java");
        loaded = true;
    }

    public static synchronized void loadFrom(Path libraryDirectory) {
        if (loaded) return;
        Path dir = Objects.requireNonNull(libraryDirectory, "libraryDirectory");
        System.load(dir.resolve(System.mapLibraryName("fluxui_shared")).toAbsolutePath().toString());
        System.load(dir.resolve(System.mapLibraryName("fluxui_java")).toAbsolutePath().toString());
        loaded = true;
    }
}
