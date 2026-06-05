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

    /** Load and parse a CSS file. Loaded files are watched when hot-reload is on. */
    public boolean loadStylesheet(String path) {
        return Native.appLoadStylesheet(handle(), path);
    }

    /** Alias matching the C++/Go/Python naming. */
    public boolean loadCSS(String path) {
        return loadStylesheet(path);
    }

    /** Alias matching the C++/Go/Python naming. */
    public App addCSS(String css) {
        return addStylesheet(css);
    }

    /**
     * Enable live CSS reloading: stylesheets loaded via {@link #loadStylesheet}
     * are watched and re-applied on edit — no recompile, no relaunch.
     */
    public App hotReload(boolean enable, float pollIntervalSeconds) {
        Native.appEnableHotReload(handle(), enable, pollIntervalSeconds);
        return this;
    }

    /** Enable live CSS reloading with the default 0.25s poll interval. */
    public App hotReload() {
        return hotReload(true, 0.25f);
    }

    /** Watch an extra CSS file (e.g. a partial or @import target). */
    public App watchCSS(String path) {
        Native.appWatchStylesheet(handle(), path);
        return this;
    }

    /** Force an immediate reload of all CSS sources from disk. */
    public boolean reloadCSS() {
        return Native.appReloadStyles(handle());
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

    public App setUpdateCallback(UpdateCallback callback) {
        long ref = Native.appSetUpdateCallback(handle(), callback);
        retainCallback(ref);
        return this;
    }

    public Widget root() {
        return new Widget(this, Native.appRoot(handle()));
    }

    public App addRoute(String path, RouteBuilder builder) {
        long ref = Native.appAddRoute(handle(), path, builder);
        retainCallback(ref);
        return this;
    }

    public boolean navigate(String path) {
        return Native.appNavigate(handle(), path);
    }

    public String currentRoute() {
        return Native.appCurrentRoute(handle());
    }

    public boolean routeDirty() {
        return Native.appRouteDirty(handle());
    }

    public boolean renderRoute(Widget container) {
        return Native.appRenderRoute(handle(), container.rawHandle());
    }

    public long addAction(String name, int keyCode, int modifiers, ActionCallback callback) {
        long[] result = Native.appAddAction(handle(), name, keyCode, modifiers, callback);
        if (result.length > 1) {
            retainCallback(result[1]);
        }
        return result.length > 0 ? result[0] : 0;
    }

    public App removeAction(long actionId) {
        Native.appRemoveAction(handle(), actionId);
        return this;
    }

    public boolean dispatchAction(String name) {
        return Native.appDispatchAction(handle(), name);
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
