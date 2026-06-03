package io.fluxui;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Declarative DSL entry point — modern HTML/Blink-named builder.
 *
 * <p>Element names match HTML exactly (div, span, p, h1..h6, nav, section,
 * button, input, a, img, ul, li, ...). Layout is expressed in CSS
 * (display:flex/grid) exactly like the browser. Build a tree with the static
 * factory methods, attach classes/handlers via {@link Node} chaining, mount it
 * with {@link #setRoot}, and run the reactive loop with {@link #runReactive}.
 *
 * <pre>{@code
 * App app = App.create();
 * app.init("CompanyGuard", 1200, 800);
 * app.loadDefaultFont(16f);
 * State<Integer> devices = new State<>(128);
 * Dsl.setRoot(app,
 *     Dsl.div(
 *         Dsl.nav(Dsl.button("Dashboard"), Dsl.button("Settings")).className("sidebar"),
 *         Dsl.div(
 *             Dsl.h1("CompanyGuard"),
 *             Dsl.textFn(() -> String.valueOf(devices.get())),
 *             Dsl.button("Escanear").className("primary")
 *                 .onClick(() -> devices.set(devices.get() + 1))
 *         ).className("content")
 *     ).className("app"));
 * Dsl.runReactive(app);
 * }</pre>
 */
public final class Dsl {
    private Dsl() {}

    // ---- Reactive binding registry — re-evaluates textFn nodes each frame. ----
    private static final class Binding {
        final Widget widget;
        final Supplier<String> fn;
        String last;

        Binding(Widget widget, Supplier<String> fn, String last) {
            this.widget = widget;
            this.fn = fn;
            this.last = last;
        }
    }

    private static final List<Binding> bindings = new ArrayList<>();

    static void registerReactive(Widget widget, Supplier<String> fn, String initial) {
        bindings.add(new Binding(widget, fn, initial));
    }

    /**
     * Re-evaluate every reactive textFn binding, pushing changed values into the
     * underlying widgets. Returns true if anything changed. Called each frame.
     */
    public static boolean pumpReactiveBindings() {
        boolean changed = false;
        for (Binding b : bindings) {
            String v = b.fn.get();
            if (!v.equals(b.last)) {
                b.last = v;
                b.widget.setText(v);
                changed = true;
            }
        }
        return changed;
    }

    // ---- HTML container builders (names match HTML/Blink exactly). ----
    public static Node div(Node... children) { return new Node("div").children(children); }
    public static Node section(Node... children) { return new Node("section").children(children); }
    public static Node article(Node... children) { return new Node("article").children(children); }
    public static Node aside(Node... children) { return new Node("aside").children(children); }
    public static Node header(Node... children) { return new Node("header").children(children); }
    public static Node footer(Node... children) { return new Node("footer").children(children); }
    public static Node main(Node... children) { return new Node("main").children(children); }
    public static Node nav(Node... children) { return new Node("nav").children(children); }
    public static Node form(Node... children) { return new Node("form").children(children); }
    public static Node fieldset(Node... children) { return new Node("fieldset").children(children); }
    public static Node blockquote(Node... children) { return new Node("blockquote").children(children); }
    public static Node figure(Node... children) { return new Node("figure").children(children); }
    public static Node ul(Node... children) { return new Node("ul").children(children); }
    public static Node ol(Node... children) { return new Node("ol").children(children); }
    public static Node li(Node... children) { return new Node("li").children(children); }
    public static Node table(Node... children) { return new Node("table").children(children); }
    public static Node tr(Node... children) { return new Node("tr").children(children); }

    // ---- HTML text content builders. ----
    public static Node text(String content) { return new Node("span").content(content); }
    public static Node span(String content) { return new Node("span").content(content); }
    public static Node p(String content) { return new Node("p").content(content); }
    public static Node h1(String content) { return new Node("h1").content(content); }
    public static Node h2(String content) { return new Node("h2").content(content); }
    public static Node h3(String content) { return new Node("h3").content(content); }
    public static Node h4(String content) { return new Node("h4").content(content); }
    public static Node h5(String content) { return new Node("h5").content(content); }
    public static Node h6(String content) { return new Node("h6").content(content); }
    public static Node strong(String content) { return new Node("strong").content(content); }
    public static Node em(String content) { return new Node("em").content(content); }
    public static Node small(String content) { return new Node("small").content(content); }
    public static Node label(String content) { return new Node("label").content(content); }
    public static Node legend(String content) { return new Node("legend").content(content); }
    public static Node code(String content) { return new Node("code").content(content); }
    public static Node pre(String content) { return new Node("pre").content(content); }
    public static Node td(String content) { return new Node("td").content(content); }
    public static Node th(String content) { return new Node("th").content(content); }

    /** Reactive text — re-evaluated whenever bound State changes. */
    public static Node textFn(Supplier<String> fn) {
        return new Node("span").textFn(fn);
    }

    // ---- Interactive controls. ----
    public static Node button(String label) { return new Node("button").content(label); }
    public static Node input(String placeholder) { return new Node("input").content(placeholder); }
    public static Node textArea(String placeholder) { return new Node("textarea").content(placeholder); }

    public static Node a(String content, String href) {
        Node n = new Node("a").content(content);
        if (href != null && !href.isEmpty()) {
            n.attr("href", href);
        }
        return n;
    }

    public static Node img(String src) { return new Node("img").content(src); }
    public static Node checkbox() { return new Node("checkbox"); }
    public static Node radio() { return new Node("radio"); }
    public static Node hr() { return new Node("hr"); }
    public static Node br() { return new Node("br"); }
    public static Node select(Node... options) { return new Node("select").children(options); }
    public static Node option(String label) { return new Node("option").content(label); }

    /** Generic escape hatch for any HTML tag. */
    public static Node el(String tag, Node... children) {
        return new Node(tag).children(children);
    }

    // ---- App helpers for the declarative flow. ----

    /** Mount a declarative Node tree as the application root. */
    public static void setRoot(App app, Node root) {
        Widget r = app.root();
        if (r == null || root == null) {
            return;
        }
        r.clearChildren();
        root.mount(r);
    }

    /** Install the reactive pump into the update loop and run the app. */
    public static void runReactive(App app) {
        app.setUpdateCallback(dt -> pumpReactiveBindings());
        app.run();
    }
}
