package io.fluxui;

@FunctionalInterface
public interface ActionCallback {
    void run(String actionName);
}
