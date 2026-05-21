package io.fluxui;

@FunctionalInterface
public interface RouteBuilder {
    void build(Widget container, String route);
}
