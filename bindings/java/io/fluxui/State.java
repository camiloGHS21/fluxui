package io.fluxui;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Lightweight reactive primitive — mirrors the C++ {@code State<T>}.
 *
 * <p>Mutating the value via {@link #set} notifies registered listeners. Reactive
 * {@code Dsl.textFn} nodes read state through a supplier and are refreshed by the
 * reactive pump each frame (see {@link Dsl#pumpReactiveBindings()}).
 */
public final class State<T> {
    private T value;
    private final List<Runnable> listeners = new ArrayList<>();

    public State(T initial) {
        this.value = initial;
    }

    public T get() {
        return value;
    }

    public void set(T newValue) {
        this.value = newValue;
        for (Runnable r : listeners) {
            r.run();
        }
    }

    public void onChange(Runnable listener) {
        listeners.add(listener);
    }

    /** Convenience: build a string supplier bound to this state. */
    public Supplier<String> asText() {
        return () -> String.valueOf(value);
    }
}
