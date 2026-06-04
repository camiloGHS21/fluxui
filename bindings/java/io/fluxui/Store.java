package io.fluxui;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;

/**
 * Global state container — Zustand-style. Holds a single state object, mutated
 * via {@link #set} which notifies subscribers.
 *
 * <pre>{@code
 *   static class CartState { int count; }
 *   Store<CartState> cart = new Store<>(new CartState());
 *   cart.set(s -> s.count++);
 *   Dsl.textFn(() -> String.valueOf(cart.get().count));
 * }</pre>
 */
public final class Store<T> {
    private final T state;
    private final List<Runnable> subscribers = new ArrayList<>();

    public Store(T initial) {
        this.state = initial;
    }

    public T get() {
        return state;
    }

    /** Mutate the state in place and notify subscribers. */
    public void set(Consumer<T> mutator) {
        mutator.accept(state);
        for (Runnable r : subscribers) {
            r.run();
        }
    }

    public <R> R select(Function<T, R> selector) {
        return selector.apply(state);
    }

    public void subscribe(Runnable fn) {
        subscribers.add(fn);
    }
}
