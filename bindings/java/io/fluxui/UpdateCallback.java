package io.fluxui;

@FunctionalInterface
public interface UpdateCallback {
    void update(float deltaTime);
}
