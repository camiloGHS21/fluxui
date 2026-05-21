package io.fluxui;

@FunctionalInterface
public interface VirtualListItemBuilder {
    void build(Widget item, int index);
}
