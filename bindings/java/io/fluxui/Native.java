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
    static native long[] appAddAction(long app,
                                      String name,
                                      int keyCode,
                                      int modifiers,
                                      ActionCallback callback);
    static native void appRemoveAction(long app, long actionId);
    static native boolean appDispatchAction(long app, String name);

    static native void widgetClearChildren(long widget);
    static native void widgetReserveChildren(long widget, int count);
    static native long widgetAddElement(long parent, String tagName, String text, String className);
    static native long widgetAddPanel(long parent, String className);
    static native long widgetAddForm(long parent, String className);
    static native long widgetAddFieldset(long parent, String className);
    static native long widgetAddText(long parent, String text, String className);
    static native long widgetAddLabel(long parent, String text, String className);
    static native long widgetAddLegend(long parent, String text, String className);
    static native long widgetAddButton(long parent, String label, String className);
    static native long widgetAddTextInput(long parent, String placeholder, String className);
    static native long widgetAddInput(long parent, String inputType, String placeholder, String className);
    static native long widgetAddPasswordInput(long parent, String placeholder, String className);
    static native long widgetAddTextarea(long parent, String placeholder, String className);
    static native long widgetAddCheckbox(long parent, boolean checked, String className);
    static native long widgetAddRadio(long parent, boolean checked, String group, String className);
    static native long widgetAddRange(long parent, float value, float min, float max, float step, String className);
    static native long widgetAddSelect(long parent, String className);
    static native long widgetAddOption(long parent, String label, String value, String className);
    static native long widgetAddAnchor(long parent, String text, String href, String className);
    static native long widgetAddDetails(long parent, String className);
    static native long widgetAddSummary(long parent, String text, String className);
    static native long widgetAddDialog(long parent, String className);
    static native long widgetAddMeter(long parent, float value, float min, float max, String className);
    static native long widgetAddProgressElement(long parent, float value, float max, String className);
    static native long widgetAddHr(long parent, String className);
    static native long widgetAddBr(long parent, String className);
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
    static native void textInputSetType(long widget, String inputType);
    static native void checkboxSetChecked(long widget, boolean checked);
    static native boolean checkboxGetChecked(long widget);
    static native void radioSetChecked(long widget, boolean checked);
    static native boolean radioGetChecked(long widget);
    static native void rangeSetValue(long widget, float value);
    static native float rangeGetValue(long widget);
    static native void selectSetSelectedIndex(long widget, int index);
    static native int selectGetSelectedIndex(long widget);
    static native void detailsSetOpen(long widget, boolean open);
    static native boolean detailsGetOpen(long widget);
    static native void dialogShow(long widget);
    static native void dialogShowModal(long widget);
    static native void dialogClose(long widget);
    static native boolean dialogGetOpen(long widget);
    static native void meterSetValue(long widget, float value);
    static native float meterGetValue(long widget);
    static native void progressElementSetValue(long widget, float value);
    static native float progressElementGetValue(long widget);
    static native void progressBarSetValue(long widget, float progress);
    static native void styleWidthPx(long widget, float value);
    static native void styleHeightPx(long widget, float value);
    static native void styleFlexGrow(long widget, float value);
    static native void styleGapPx(long widget, float value);
    static native void stylePaddingAllPx(long widget, float value);
    static native void releaseCallback(long callbackRef);
}
