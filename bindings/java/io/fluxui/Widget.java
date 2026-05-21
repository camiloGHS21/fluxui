package io.fluxui;

public final class Widget {
    private final App owner;
    private final long handle;

    Widget(App owner, long handle) {
        if (handle == 0) {
            throw new IllegalStateException("FluxUI widget handle is null");
        }
        this.owner = owner;
        this.handle = handle;
    }

    Widget(long handle) {
        this(null, handle);
    }

    public long rawHandle() {
        return handle;
    }

    public Widget clearChildren() {
        Native.widgetClearChildren(handle);
        return this;
    }

    public Widget reserveChildren(int count) {
        Native.widgetReserveChildren(handle, count);
        return this;
    }

    public Widget addPanel(String className) {
        return child(Native.widgetAddPanel(handle, className));
    }

    public Widget addText(String text, String className) {
        return child(Native.widgetAddText(handle, text, className));
    }

    public Widget addButton(String label, String className) {
        return child(Native.widgetAddButton(handle, label, className));
    }

    public Widget addTextInput(String placeholder, String className) {
        return child(Native.widgetAddTextInput(handle, placeholder, className));
    }

    public Widget addIcon(String glyph, String className) {
        return child(Native.widgetAddIcon(handle, glyph, className));
    }

    public Widget addProgressBar(String className, float progress) {
        return child(Native.widgetAddProgressBar(handle, className, progress));
    }

    public Widget addCanvas(String className) {
        return child(Native.widgetAddCanvas(handle, className));
    }

    public Widget addVirtualList(String className,
                                 int itemCount,
                                 float itemHeight,
                                 VirtualListItemBuilder builder) {
        long[] result = Native.widgetAddVirtualList(handle, className, itemCount, itemHeight, builder);
        if (owner != null && result.length > 1) {
            owner.retainCallback(result[1]);
        }
        return child(result[0]);
    }

    public Widget setVirtualListItemCount(int itemCount) {
        Native.virtualListSetItemCount(handle, itemCount);
        return this;
    }

    public Widget refreshVirtualList() {
        Native.virtualListRefresh(handle);
        return this;
    }

    public Widget scrollVirtualListToIndex(int index) {
        return scrollVirtualListToIndex(index, ScrollStrategy.NEAREST);
    }

    public Widget scrollVirtualListToIndex(int index, ScrollStrategy strategy) {
        Native.virtualListScrollToIndex(handle, index, strategy.value);
        return this;
    }

    public Widget setId(String id) {
        Native.widgetSetId(handle, id);
        return this;
    }

    public Widget setClassName(String className) {
        Native.widgetSetClass(handle, className);
        return this;
    }

    public Widget setVisible(boolean visible) {
        Native.widgetSetVisible(handle, visible);
        return this;
    }

    public Widget setOnClick(Runnable callback) {
        long ref = Native.widgetSetOnClick(handle, callback);
        if (owner != null) {
            owner.retainCallback(ref);
        }
        return this;
    }

    public Widget setText(String text) {
        Native.textSetContent(handle, text);
        return this;
    }

    public Widget setButtonLabel(String label) {
        Native.buttonSetLabel(handle, label);
        return this;
    }

    public Widget setProgress(float progress) {
        Native.progressBarSetValue(handle, progress);
        return this;
    }

    public Widget width(float px) {
        Native.styleWidthPx(handle, px);
        return this;
    }

    public Widget height(float px) {
        Native.styleHeightPx(handle, px);
        return this;
    }

    public Widget flexGrow(float value) {
        Native.styleFlexGrow(handle, value);
        return this;
    }

    public Widget padding(float px) {
        Native.stylePaddingAllPx(handle, px);
        return this;
    }

    public Widget gap(float px) {
        Native.styleGapPx(handle, px);
        return this;
    }

    private Widget child(long childHandle) {
        return new Widget(owner, childHandle);
    }
}
