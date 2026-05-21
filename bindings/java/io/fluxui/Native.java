package io.fluxui;

final class Native {
    static {
        FluxUI.load();
    }

    private Native() {
    }

    static native long appCreate();
    static native void appDestroy(long app);
    static native boolean appInit(long app, String title, int width, int height);
    static native void appSetBackend(long app, int backend);
    static native int appGetBackend(long app);
    static native void appRun(long app);
    static native void appStop(long app);
    static native void appShutdown(long app);
    static native void appAddStylesheet(long app, String css);
    static native boolean appLoadFont(long app, String path, float size);
    static native boolean appLoadDefaultFont(long app, float size);
    static native void appWarmFontCache(long app, float[] sizes, String name);
    static native void appReleaseFontSources(long app);
    static native long appSetUpdateCallback(long app, UpdateCallback callback);
    static native long appRoot(long app);
    static native long appAddRoute(long app, String path, RouteBuilder builder);
    static native boolean appNavigate(long app, String path);
    static native String appCurrentRoute(long app);
    static native boolean appRouteDirty(long app);
    static native boolean appRenderRoute(long app, long container);

    static native void widgetClearChildren(long widget);
    static native void widgetReserveChildren(long widget, int count);
    static native long widgetAddPanel(long parent, String className);
    static native long widgetAddText(long parent, String text, String className);
    static native long widgetAddButton(long parent, String label, String className);
    static native long widgetAddTextInput(long parent, String placeholder, String className);
    static native long widgetAddIcon(long parent, String glyph, String className);
    static native long widgetAddProgressBar(long parent, String className, float progress);
    static native long widgetAddCanvas(long parent, String className);
    static native long[] widgetAddVirtualList(long parent,
                                              String className,
                                              int itemCount,
                                              float itemHeight,
                                              VirtualListItemBuilder builder);
    static native void virtualListSetItemCount(long widget, int itemCount);
    static native void virtualListRefresh(long widget);
    static native void virtualListScrollToIndex(long widget, int index, int strategy);
    static native void widgetSetId(long widget, String id);
    static native void widgetSetClass(long widget, String className);
    static native void widgetSetVisible(long widget, boolean visible);
    static native long widgetSetOnClick(long widget, Runnable callback);
    static native void textSetContent(long widget, String text);
    static native void buttonSetLabel(long widget, String label);
    static native void progressBarSetValue(long widget, float progress);
    static native void styleWidthPx(long widget, float value);
    static native void styleHeightPx(long widget, float value);
    static native void styleFlexGrow(long widget, float value);
    static native void styleGapPx(long widget, float value);
    static native void stylePaddingAllPx(long widget, float value);
    static native void releaseCallback(long callbackRef);
}
