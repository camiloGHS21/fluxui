package io.fluxui;

import java.util.ArrayList;
import java.util.List;

public final class App implements AutoCloseable {
    private long handle;
    private final List<Long> callbackRefs = new ArrayList<>();

    private App(long handle) {
        if (handle == 0) {
            throw new IllegalStateException("fluxui_app_create returned null");
        }
        this.handle = handle;
    }

    public static App create() {
        return new App(Native.appCreate());
    }

    public boolean init(String title, int width, int height) {
        return Native.appInit(handle(), title, width, height);
    }

    public App setBackend(Backend backend) {
        Native.appSetBackend(handle(), backend.value);
        return this;
    }

    public Backend backend() {
        int value = Native.appGetBackend(handle());
        for (Backend backend : Backend.values()) {
            if (backend.value == value) return backend;
        }
        return Backend.AUTO;
    }

    public App addStylesheet(String css) {
        Native.appAddStylesheet(handle(), css);
        return this;
    }

    public boolean loadFont(String path, float size) {
        return Native.appLoadFont(handle(), path, size);
    }

    public boolean loadDefaultFont(float size) {
        return Native.appLoadDefaultFont(handle(), size);
    }

    public App warmFontCache(float[] sizes) {
        Native.appWarmFontCache(handle(), sizes, "default");
        return this;
    }

    public App warmFontCache(float[] sizes, String name) {
        Native.appWarmFontCache(handle(), sizes, name);
        return this;
    }

    public App releaseFontSources() {
        Native.appReleaseFontSources(handle());
        return this;
    }

    public Widget root() {
        return new Widget(this, Native.appRoot(handle()));
    }

    public void run() {
        Native.appRun(handle());
    }

    public void stop() {
        Native.appStop(handle());
    }

    public void shutdown() {
        Native.appShutdown(handle());
    }

    long handle() {
        if (handle == 0) {
            throw new IllegalStateException("FluxUI app is already closed");
        }
        return handle;
    }

    void retainCallback(long callbackRef) {
        if (callbackRef != 0) {
            callbackRefs.add(callbackRef);
        }
    }

    @Override
    public void close() {
        long current = handle;
        handle = 0;
        if (current != 0) {
            Native.appDestroy(current);
        }
        for (long ref : callbackRefs) {
            Native.releaseCallback(ref);
        }
        callbackRefs.clear();
    }
}
