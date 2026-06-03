import io.fluxui.State;

/**
 * Headless test for the FluxUI Java DSL reactive State primitive.
 *
 * <p>State is the reactive core that drives {@code Dsl.textFn} nodes. The full
 * pump ({@code Dsl.pumpReactiveBindings}) requires native Text widgets, which is
 * exercised end-to-end by {@code DslDemo}; here we validate the language-side
 * reactive logic without a GPU context, mirroring examples/cpp/test_dsl.cpp.
 */
public final class TestDsl {
    static int failures = 0;

    static void check(boolean cond, String label) {
        if (!cond) {
            System.out.println("FAIL: " + label);
            failures++;
        } else {
            System.out.println("  PASS " + label);
        }
    }

    public static void main(String[] args) {
        // [1] State holds and updates its value.
        State<Integer> devices = new State<>(128);
        check(devices.get() == 128, "[1] initial value");
        devices.set(129);
        check(devices.get() == 129, "[1] value updates on set");

        // [2] Listeners fire on each set with the latest value.
        final int[] fired = {0};
        final int[] lastSeen = {0};
        State<Integer> count = new State<>(0);
        count.onChange(() -> {
            fired[0]++;
            lastSeen[0] = count.get();
        });
        count.set(1);
        count.set(2);
        count.set(3);
        check(fired[0] == 3, "[2] listener fired on each set");
        check(lastSeen[0] == 3, "[2] listener sees latest value");

        // [3] asText() supplier reflects current state (what textFn reads).
        State<Integer> metric = new State<>(7);
        java.util.function.Supplier<String> txt = metric.asText();
        check(txt.get().equals("7"), "[3] asText initial");
        metric.set(42);
        check(txt.get().equals("42"), "[3] asText reflects update");

        // [4] onClick-style mutation drives the bound supplier value.
        State<Integer> clicks = new State<>(0);
        Runnable onClick = () -> clicks.set(clicks.get() + 1);
        onClick.run();
        onClick.run();
        onClick.run();
        check(clicks.asText().get().equals("3"), "[4] click mutations accumulate");

        if (failures == 0) {
            System.out.println("All Java DSL tests passed!");
        } else {
            System.out.println(failures + " Java DSL test(s) failed");
            System.exit(1);
        }
    }
}
