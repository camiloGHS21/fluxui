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

    public Widget addFieldset(String className) {
        return child(Native.widgetAddFieldset(handle, className));
    }

    public Widget addText(String text, String className) {
        return child(Native.widgetAddText(handle, text, className));
    }

    public Widget addLabel(String text, String className) {
        return child(Native.widgetAddLabel(handle, text, className));
    }

    public Widget addLegend(String text, String className) {
        return child(Native.widgetAddLegend(handle, text, className));
    }

    public Widget addButton(String label, String className) {
        return child(Native.widgetAddButton(handle, label, className));
    }

    public Widget addTextInput(String placeholder, String className) {
        return child(Native.widgetAddTextInput(handle, placeholder, className));
    }

    public Widget addInput(String inputType, String placeholder, String className) {
        return child(Native.widgetAddInput(handle, inputType, placeholder, className));
    }

    public Widget addPasswordInput(String placeholder, String className) {
        return child(Native.widgetAddPasswordInput(handle, placeholder, className));
    }

    public Widget addTextarea(String placeholder, String className) {
        return child(Native.widgetAddTextarea(handle, placeholder, className));
    }

    public Widget addCheckbox(boolean checked, String className) {
        return child(Native.widgetAddCheckbox(handle, checked, className));
    }

    public Widget addRadio(boolean checked, String group, String className) {
        return child(Native.widgetAddRadio(handle, checked, group, className));
    }

    public Widget addRange(float value, float min, float max, float step, String className) {
        return child(Native.widgetAddRange(handle, value, min, max, step, className));
    }

    public Widget addSelect(String className) {
        return child(Native.widgetAddSelect(handle, className));
    }

    public Widget addOption(String label, String value, String className) {
        return child(Native.widgetAddOption(handle, label, value, className));
    }

    public Widget addAnchor(String text, String href, String className) {
        return child(Native.widgetAddAnchor(handle, text, href, className));
    }

    public Widget addDetails(String className) {
        return child(Native.widgetAddDetails(handle, className));
    }

    public Widget addSummary(String text, String className) {
        return child(Native.widgetAddSummary(handle, text, className));
    }

    public Widget addDialog(String className) {
        return child(Native.widgetAddDialog(handle, className));
    }

    public Widget addMeter(float value, float min, float max, String className) {
        return child(Native.widgetAddMeter(handle, value, min, max, className));
    }

    public Widget addProgressElement(float value, float max, String className) {
        return child(Native.widgetAddProgressElement(handle, value, max, className));
    }

    public Widget addHr(String className) {
        return child(Native.widgetAddHr(handle, className));
    }

    public Widget addBr(String className) {
        return child(Native.widgetAddBr(handle, className));
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

    public Widget setTextInputType(String inputType) {
        Native.textInputSetType(handle, inputType);
        return this;
    }

    public Widget setChecked(boolean checked) {
        Native.checkboxSetChecked(handle, checked);
        Native.radioSetChecked(handle, checked);
        return this;
    }

    public boolean isChecked() {
        return Native.checkboxGetChecked(handle) || Native.radioGetChecked(handle);
    }

    public Widget setRangeValue(float value) {
        Native.rangeSetValue(handle, value);
        return this;
    }

    public float rangeValue() {
        return Native.rangeGetValue(handle);
    }

    public Widget setSelectedIndex(int index) {
        Native.selectSetSelectedIndex(handle, index);
        return this;
    }

    public int selectedIndex() {
        return Native.selectGetSelectedIndex(handle);
    }

    public Widget setDetailsOpen(boolean open) {
        Native.detailsSetOpen(handle, open);
        return this;
    }

    public boolean detailsOpen() {
        return Native.detailsGetOpen(handle);
    }

    public Widget showDialog() {
        Native.dialogShow(handle);
        return this;
    }

    public Widget showModalDialog() {
        Native.dialogShowModal(handle);
        return this;
    }

    public Widget closeDialog() {
        Native.dialogClose(handle);
        return this;
    }

    public boolean dialogOpen() {
        return Native.dialogGetOpen(handle);
    }

    public Widget setMeterValue(float value) {
        Native.meterSetValue(handle, value);
        return this;
    }

    public float meterValue() {
        return Native.meterGetValue(handle);
    }

    public Widget setProgressElementValue(float value) {
        Native.progressElementSetValue(handle, value);
        return this;
    }

    public float progressElementValue() {
        return Native.progressElementGetValue(handle);
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
