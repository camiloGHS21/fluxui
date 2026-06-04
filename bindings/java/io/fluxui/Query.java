package io.fluxui;

import java.util.function.Function;
import java.util.function.Supplier;

/**
 * Async data fetch with loading/error/data states (React Query / SWR-style).
 *
 * <pre>{@code
 *   Query<String> users = new Query<>(() -> httpGet("/api/users"));
 *   users.start();
 *   users.view(
 *       () -> Dsl.skeleton(3),
 *       data -> Dsl.text(data),
 *       err -> Dsl.text("Error: " + err)
 *   );
 * }</pre>
 */
public final class Query<T> {
    public enum Status { IDLE, LOADING, SUCCESS, ERROR }

    private final Supplier<T> fetcher;
    private volatile Status status = Status.IDLE;
    private volatile T data;
    private volatile String error = "";

    public Query(Supplier<T> fetcher) {
        this.fetcher = fetcher;
    }

    public Status status() {
        return status;
    }

    public boolean isLoading() { return status == Status.LOADING; }
    public boolean isSuccess() { return status == Status.SUCCESS; }
    public boolean isError() { return status == Status.ERROR; }

    /** Kick off the fetch on a background thread. */
    public void start() {
        if (status == Status.LOADING) {
            return;
        }
        status = Status.LOADING;
        Thread t = new Thread(() -> {
            try {
                data = fetcher.get();
                status = Status.SUCCESS;
            } catch (Exception e) {
                error = String.valueOf(e.getMessage());
                status = Status.ERROR;
            }
        });
        t.setDaemon(true);
        t.start();
    }

    public void refetch() {
        status = Status.IDLE;
        start();
    }

    /** Render the right Node for the current state. */
    public Node view(Supplier<Node> onLoading,
                     Function<T, Node> onSuccess,
                     Function<String, Node> onError) {
        switch (status) {
            case SUCCESS:
                return onSuccess.apply(data);
            case ERROR:
                return onError != null ? onError.apply(error) : Dsl.p("Error: " + error);
            default:
                return onLoading != null ? onLoading.get() : Dsl.div();
        }
    }
}
