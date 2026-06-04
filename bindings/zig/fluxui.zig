pub const c = @cImport({
    @cInclude("fluxui/fluxui_c.h");
});

const std = @import("std");

pub const Color = c.FluxUIColor;
pub const Rect = c.FluxUIRect;

pub const Backend = enum(c_uint) {
    auto = c.FLUXUI_BACKEND_AUTO,
    vulkan = c.FLUXUI_BACKEND_VULKAN,
    direct3d12 = c.FLUXUI_BACKEND_DIRECT3D12,
    metal = c.FLUXUI_BACKEND_METAL,
    compatibility = c.FLUXUI_BACKEND_COMPATIBILITY,

    pub fn default() Backend {
        return @enumFromInt(c.fluxui_default_backend());
    }
};

pub const ScrollStrategy = enum(c_int) {
    start = 0,
    center = 1,
    end = 2,
    nearest = 3,
};

pub const Error = error{
    CreateFailed,
    InitFailed,
    MissingRoot,
    MissingWidget,
};

pub const App = struct {
    raw: ?*c.FluxUIApp,

    pub fn create() Error!App {
        const raw = c.fluxui_app_create();
        if (raw == null) return Error.CreateFailed;
        return .{ .raw = raw };
    }

    pub fn init(self: App, title: [*:0]const u8, width: i32, height: i32) Error!void {
        if (c.fluxui_app_init(self.raw, title, width, height) == 0) {
            return Error.InitFailed;
        }
    }

    pub fn setBackend(self: App, selected_backend: Backend) bool {
        return c.fluxui_app_set_backend(self.raw, @intFromEnum(selected_backend)) != 0;
    }

    pub fn backend(self: App) Backend {
        return @enumFromInt(c.fluxui_app_get_backend(self.raw));
    }

    pub fn deinit(self: App) void {
        c.fluxui_app_destroy(self.raw);
    }

    pub fn run(self: App) void {
        c.fluxui_app_run(self.raw);
    }

    pub fn stop(self: App) void {
        c.fluxui_app_stop(self.raw);
    }

    pub fn loadFont(self: App, path: [*:0]const u8, size: f32) bool {
        return c.fluxui_app_load_font(self.raw, path, size) != 0;
    }

    pub fn loadDefaultFont(self: App, size: f32) bool {
        return c.fluxui_app_load_default_font(self.raw, size) != 0;
    }

    pub fn warmFontCache(self: App, sizes: []const f32, name: [*:0]const u8) void {
        if (sizes.len == 0) return;
        c.fluxui_app_warm_font_cache(self.raw, sizes.ptr, @intCast(sizes.len), name);
    }

    pub fn releaseFontSources(self: App) void {
        c.fluxui_app_release_font_sources(self.raw);
    }

    pub fn loadStylesheet(self: App, path: [*:0]const u8) bool {
        return c.fluxui_app_load_stylesheet(self.raw, path) != 0;
    }

    pub fn addStylesheet(self: App, css: [*:0]const u8) void {
        c.fluxui_app_add_stylesheet(self.raw, css);
    }

    pub fn setUpdateCallback(self: App, callback: c.FluxUIUpdateCallback, user_data: ?*anyopaque) void {
        c.fluxui_app_set_update_callback(self.raw, callback, user_data);
    }

    /// Mount a declarative Node tree as the application root.
    pub fn setRoot(self: App, node: dsl.Node) Error!void {
        const r = try self.root();
        r.clearChildren();
        _ = try node.mount(r);
    }

    /// Register a route. `view` is a fn() that returns a Node tree for the path.
    pub fn addRoute(self: App, path: [*:0]const u8, view: dsl.ViewFn) void {
        _ = self;
        dsl.routerAddRoute(path, view);
    }

    /// Register all views collected via dsl.registerView() (file-based routing).
    pub fn useViews(self: App) void {
        _ = self;
        dsl.routerUseViews();
    }

    /// Set the app shell. `layout` is a fn(content) -> Node embedding the view.
    pub fn setLayout(self: App, layout: dsl.LayoutFn) void {
        _ = self;
        dsl.routerSetLayout(layout);
    }

    /// Current route path.
    pub fn route(self: App) [*:0]const u8 {
        _ = self;
        return dsl.routerCurrent();
    }

    /// Switch to a route and rebuild the shell (nav highlights refresh).
    pub fn navigate(self: App, path: [*:0]const u8) void {
        dsl.routerSetCurrent(path);
        self.build(path) catch {};
    }

    /// Mount the shell + initial route. Uses the first route if path is empty.
    pub fn build(self: App, initial_route: [*:0]const u8) Error!void {
        dsl.routerSetApp(self.raw);
        const r = try self.root();
        r.clearChildren();
        dsl.routerBuild(r, initial_route);
    }

    /// Install the reactive pump into the update loop and run the app.
    pub fn runReactive(self: App) void {
        if (dsl.routerHasRoutes()) {
            self.build(dsl.routerCurrent()) catch {};
        }
        self.setUpdateCallback(dsl.reactiveUpdateCallback, null);
        self.run();
    }

    pub fn root(self: App) Error!Widget {
        const raw_widget = c.fluxui_app_root(self.raw);
        if (raw_widget == null) return Error.MissingRoot;
        return .{ .raw = raw_widget };
    }

    pub fn addAction(
        self: App,
        name: [*:0]const u8,
        key_code: i32,
        modifiers: i32,
        callback: c.FluxUIActionCallback,
        user_data: ?*anyopaque,
    ) u64 {
        return c.fluxui_app_add_action(
            self.raw,
            name,
            key_code,
            modifiers,
            callback,
            user_data,
        );
    }

    pub fn removeAction(self: App, action_id: u64) void {
        c.fluxui_app_remove_action(self.raw, action_id);
    }

    pub fn dispatchAction(self: App, name: [*:0]const u8) bool {
        return c.fluxui_app_dispatch_action(self.raw, name) != 0;
    }

    pub fn userData(self: App) ?*anyopaque {
        return @ptrCast(self.raw);
    }
};

pub const Widget = struct {
    raw: ?*c.FluxUIWidget,

    pub fn clearChildren(self: Widget) void {
        c.fluxui_widget_clear_children(self.raw);
    }

    pub fn reserveChildren(self: Widget, count: u32) void {
        c.fluxui_widget_reserve_children(self.raw, count);
    }

    pub fn addElement(self: Widget, tag_name: [*:0]const u8, text: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_element(self.raw, tag_name, text, class_name));
    }

    pub fn addPanel(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_panel(self.raw, class_name));
    }

    pub fn addForm(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_form(self.raw, class_name));
    }

    pub fn addFieldset(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_fieldset(self.raw, class_name));
    }

    pub fn addText(self: Widget, text: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_text(self.raw, text, class_name));
    }

    pub fn addLabel(self: Widget, text: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_label(self.raw, text, class_name));
    }

    pub fn addLegend(self: Widget, text: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_legend(self.raw, text, class_name));
    }

    pub fn addButton(self: Widget, label: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_button(self.raw, label, class_name));
    }

    pub fn addTextInput(self: Widget, placeholder: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_text_input(self.raw, placeholder, class_name));
    }

    pub fn addInput(self: Widget, input_type: [*:0]const u8, placeholder: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_input(self.raw, input_type, placeholder, class_name));
    }

    pub fn addPasswordInput(self: Widget, placeholder: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_password_input(self.raw, placeholder, class_name));
    }

    pub fn addTextarea(self: Widget, placeholder: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_textarea(self.raw, placeholder, class_name));
    }

    pub fn addCheckbox(self: Widget, is_checked: bool, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_checkbox(self.raw, if (is_checked) 1 else 0, class_name));
    }

    pub fn addRadio(self: Widget, is_checked: bool, group: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_radio(self.raw, if (is_checked) 1 else 0, group, class_name));
    }

    pub fn addRange(self: Widget, value: f32, min: f32, max: f32, step: f32, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_range(self.raw, value, min, max, step, class_name));
    }

    pub fn addSelect(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_select(self.raw, class_name));
    }

    pub fn addOption(self: Widget, label: [*:0]const u8, value: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_option(self.raw, label, value, class_name));
    }

    pub fn addAnchor(self: Widget, text: [*:0]const u8, href: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_anchor(self.raw, text, href, class_name));
    }

    pub fn addDetails(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_details(self.raw, class_name));
    }

    pub fn addSummary(self: Widget, text: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_summary(self.raw, text, class_name));
    }

    pub fn addDialog(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_dialog(self.raw, class_name));
    }

    pub fn addMeter(self: Widget, value: f32, min: f32, max: f32, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_meter(self.raw, value, min, max, class_name));
    }

    pub fn addProgressElement(self: Widget, value: f32, max: f32, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_progress_element(self.raw, value, max, class_name));
    }

    pub fn addHr(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_hr(self.raw, class_name));
    }

    pub fn addBr(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_br(self.raw, class_name));
    }

    pub fn addIcon(self: Widget, glyph: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_icon(self.raw, glyph, class_name));
    }

    pub fn addProgressBar(self: Widget, class_name: [*:0]const u8, progress: f32) Error!Widget {
        return fromRaw(c.fluxui_widget_add_progress_bar(self.raw, class_name, progress));
    }

    pub fn addCanvas(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_canvas(self.raw, class_name));
    }

    pub fn addVirtualList(
        self: Widget,
        class_name: [*:0]const u8,
        item_count: u32,
        item_height: f32,
        callback: c.FluxUIVirtualListItemCallback,
        user_data: ?*anyopaque,
    ) Error!Widget {
        return fromRaw(c.fluxui_widget_add_virtual_list(
            self.raw,
            class_name,
            item_count,
            item_height,
            callback,
            user_data,
        ));
    }

    pub fn setVirtualListItemCount(self: Widget, item_count: u32) void {
        c.fluxui_virtual_list_set_item_count(self.raw, item_count);
    }

    pub fn refreshVirtualList(self: Widget) void {
        c.fluxui_virtual_list_refresh(self.raw);
    }

    pub fn scrollVirtualListToIndex(self: Widget, index: u32, strategy: ScrollStrategy) void {
        c.fluxui_virtual_list_scroll_to_index(self.raw, index, @intFromEnum(strategy));
    }

    pub fn setOnDraw(self: Widget, callback: c.FluxUIDrawCallback, user_data: ?*anyopaque) void {
        c.fluxui_canvas_set_on_draw(self.raw, callback, user_data);
    }

    pub fn setOnClick(self: Widget, callback: c.FluxUIClickCallback, user_data: ?*anyopaque) void {
        c.fluxui_widget_set_on_click(self.raw, callback, user_data);
    }

    pub fn setOnClickStopApp(self: Widget, app: App) void {
        self.setOnClick(stopAppCallback, app.userData());
    }

    pub fn bounds(self: Widget) Rect {
        return c.fluxui_widget_get_bounds(self.raw);
    }

    pub fn setTextInputType(self: Widget, input_type: [*:0]const u8) void {
        c.fluxui_text_input_set_type(self.raw, input_type);
    }

    pub fn setChecked(self: Widget, is_checked: bool) void {
        c.fluxui_checkbox_set_checked(self.raw, if (is_checked) 1 else 0);
        c.fluxui_radio_set_checked(self.raw, if (is_checked) 1 else 0);
    }

    pub fn checked(self: Widget) bool {
        return c.fluxui_checkbox_get_checked(self.raw) != 0 or
            c.fluxui_radio_get_checked(self.raw) != 0;
    }

    pub fn setRangeValue(self: Widget, value: f32) void {
        c.fluxui_range_set_value(self.raw, value);
    }

    pub fn rangeValue(self: Widget) f32 {
        return c.fluxui_range_get_value(self.raw);
    }

    pub fn setSelectedIndex(self: Widget, index: u32) void {
        c.fluxui_select_set_selected_index(self.raw, index);
    }

    pub fn selectedIndex(self: Widget) u32 {
        return c.fluxui_select_get_selected_index(self.raw);
    }

    pub fn setDetailsOpen(self: Widget, open: bool) void {
        c.fluxui_details_set_open(self.raw, if (open) 1 else 0);
    }

    pub fn detailsOpen(self: Widget) bool {
        return c.fluxui_details_get_open(self.raw) != 0;
    }

    pub fn showDialog(self: Widget) void {
        c.fluxui_dialog_show(self.raw);
    }

    pub fn showModalDialog(self: Widget) void {
        c.fluxui_dialog_show_modal(self.raw);
    }

    pub fn closeDialog(self: Widget) void {
        c.fluxui_dialog_close(self.raw);
    }

    pub fn dialogOpen(self: Widget) bool {
        return c.fluxui_dialog_get_open(self.raw) != 0;
    }

    pub fn setMeterValue(self: Widget, value: f32) void {
        c.fluxui_meter_set_value(self.raw, value);
    }

    pub fn meterValue(self: Widget) f32 {
        return c.fluxui_meter_get_value(self.raw);
    }

    pub fn setProgressElementValue(self: Widget, value: f32) void {
        c.fluxui_progress_element_set_value(self.raw, value);
    }

    pub fn progressElementValue(self: Widget) f32 {
        return c.fluxui_progress_element_get_value(self.raw);
    }

    pub fn styleWidth(self: Widget, px: f32) void {
        c.fluxui_style_width_px(self.raw, px);
    }

    pub fn styleHeight(self: Widget, px: f32) void {
        c.fluxui_style_height_px(self.raw, px);
    }

    pub fn styleMinWidth(self: Widget, px: f32) void {
        c.fluxui_style_min_width_px(self.raw, px);
    }

    pub fn styleMinHeight(self: Widget, px: f32) void {
        c.fluxui_style_min_height_px(self.raw, px);
    }

    pub fn styleMaxWidth(self: Widget, px: f32) void {
        c.fluxui_style_max_width_px(self.raw, px);
    }

    pub fn styleMaxHeight(self: Widget, px: f32) void {
        c.fluxui_style_max_height_px(self.raw, px);
    }

    pub fn styleFlexGrow(self: Widget, grow: f32) void {
        c.fluxui_style_flex_grow(self.raw, grow);
    }

    pub fn styleGap(self: Widget, px: f32) void {
        c.fluxui_style_gap_px(self.raw, px);
    }

    pub fn stylePaddingAll(self: Widget, px: f32) void {
        c.fluxui_style_padding_all_px(self.raw, px);
    }

    pub fn stylePadding(self: Widget, top: f32, right: f32, bottom: f32, left: f32) void {
        c.fluxui_style_padding_px(self.raw, top, right, bottom, left);
    }

    pub fn styleMarginAll(self: Widget, px: f32) void {
        c.fluxui_style_margin_all_px(self.raw, px);
    }

    pub fn styleMargin(self: Widget, top: f32, right: f32, bottom: f32, left: f32) void {
        c.fluxui_style_margin_px(self.raw, top, right, bottom, left);
    }

    pub fn styleBorderRadius(self: Widget, px: f32) void {
        c.fluxui_style_border_radius_px(self.raw, px);
    }

    pub fn styleBackgroundColor(self: Widget, color: Color) void {
        c.fluxui_style_background_color(self.raw, color);
    }

    pub fn styleTextColor(self: Widget, color: Color) void {
        c.fluxui_style_text_color(self.raw, color);
    }

    pub fn css(self: Widget, declarations: [*:0]const u8) void {
        c.fluxui_widget_css(self.raw, declarations);
    }

    pub fn setContent(self: Widget, text: [*:0]const u8) void {
        c.fluxui_text_set_content(self.raw, text);
    }

    pub fn setClassName(self: Widget, class_name: [*:0]const u8) void {
        c.fluxui_widget_set_class(self.raw, class_name);
    }

    pub fn setId(self: Widget, widget_id: [*:0]const u8) void {
        c.fluxui_widget_set_id(self.raw, widget_id);
    }

    fn fromRaw(raw: ?*c.FluxUIWidget) Error!Widget {
        if (raw == null) return Error.MissingWidget;
        return .{ .raw = raw };
    }
};

pub fn rgba(r: f32, g: f32, b: f32, a: f32) Color {
    return c.fluxui_color_rgba(r, g, b, a);
}

pub fn rgbU8(r: u8, g: u8, b: u8) Color {
    return c.fluxui_color_rgb_u8(r, g, b);
}

pub fn stopAppCallback(_: ?*c.FluxUIWidget, user_data: ?*anyopaque) callconv(.c) void {
    c.fluxui_app_stop(@ptrCast(user_data));
}

pub const Renderer = struct {
    raw: ?*anyopaque,

    pub fn drawRect(self: Renderer, rect: Rect, color: Color) void {
        c.fluxui_draw_rect(self.raw, rect, color);
    }

    pub fn drawText(self: Renderer, text: [*:0]const u8, x: f32, y: f32, color: Color, font_size: f32) void {
        c.fluxui_draw_text(self.raw, text, x, y, color, font_size);
    }

    pub fn drawImage(self: Renderer, name_or_path: [*:0]const u8, rect: Rect, opacity: f32) void {
        c.fluxui_draw_image(self.raw, name_or_path, rect, opacity);
    }

    pub fn flush(self: Renderer) void {
        c.fluxui_renderer_flush(self.raw);
    }
};

// ============================================================
//  Declarative DSL — modern HTML/Blink-named functional builder.
//
//  Element names match HTML exactly (div, span, h1, button, nav, section, ...).
//  Zig has no closures, so reactive text + click handlers use explicit C-ABI
//  function pointers plus a global binding registry. Layout is expressed in CSS
//  (display:flex/grid). Use via `fluxui.dsl.div(...)`, `fluxui.dsl.State(i32)`.
// ============================================================
pub const dsl = struct {

/// Lightweight reactive primitive. `T` must be a value type.
pub fn State(comptime T: type) type {
    return struct {
        const Self = @This();
        value: T,

        pub fn init(initial: T) Self {
            return .{ .value = initial };
        }
        pub fn get(self: *const Self) T {
            return self.value;
        }
        pub fn set(self: *Self, v: T) void {
            self.value = v;
        }
    };
}

// ---- Reactive binding registry (fixed capacity, no allocator needed). ----
pub const TextFnPtr = *const fn (?*anyopaque) callconv(.c) [*:0]const u8;

const ReactiveBinding = struct {
    widget: Widget,
    func: TextFnPtr,
    user_data: ?*anyopaque,
    last: [*:0]const u8,
};

var g_bindings: [256]ReactiveBinding = undefined;
var g_binding_count: usize = 0;

fn registerReactive(widget: Widget, func: TextFnPtr, user_data: ?*anyopaque, initial: [*:0]const u8) void {
    if (g_binding_count >= g_bindings.len) return;
    g_bindings[g_binding_count] = .{
        .widget = widget,
        .func = func,
        .user_data = user_data,
        .last = initial,
    };
    g_binding_count += 1;
}

fn cstrEql(a: [*:0]const u8, b: [*:0]const u8) bool {
    var i: usize = 0;
    while (true) : (i += 1) {
        if (a[i] != b[i]) return false;
        if (a[i] == 0) return true;
    }
}

/// Re-evaluate every reactive `textFn` binding, pushing changed values into the
/// underlying widgets. Returns true if anything changed. Called each frame.
pub fn pumpReactiveBindings() bool {
    var changed = false;
    var i: usize = 0;
    while (i < g_binding_count) : (i += 1) {
        const b = &g_bindings[i];
        const v = b.func(b.user_data);
        if (!cstrEql(v, b.last)) {
            b.last = v;
            b.widget.setContent(v);
            changed = true;
        }
    }
    return changed;
}

fn reactiveUpdateCallback(_: ?*c.FluxUIApp, _: f32, _: ?*anyopaque) callconv(.c) void {
    _ = pumpReactiveBindings();
}

// ---- Node — deferred, HTML-named declarative element, materialized on mount. ----
//
// Every node carries an HTML tag name; mount() routes through Widget.addElement,
// the single source of truth for the tag -> widget mapping (Blink UA parity).
// Reactive text nodes use `is_text_fn` + a function pointer.
pub const Node = struct {
    tag: [*:0]const u8 = "div",
    content: [*:0]const u8 = "",
    class_name: [*:0]const u8 = "",
    node_id: [*:0]const u8 = "",
    inline_css: [*:0]const u8 = "",
    on_click: ?c.FluxUIClickCallback = null,
    on_click_data: ?*anyopaque = null,
    is_text_fn: bool = false,
    text_fn: ?TextFnPtr = null,
    text_fn_data: ?*anyopaque = null,
    children: []const Node = &.{},

    pub fn class(self: Node, cls: [*:0]const u8) Node {
        var n = self;
        n.class_name = cls;
        return n;
    }
    pub fn id(self: Node, node_id: [*:0]const u8) Node {
        var n = self;
        n.node_id = node_id;
        return n;
    }
    pub fn css(self: Node, decl: [*:0]const u8) Node {
        var n = self;
        n.inline_css = decl;
        return n;
    }
    pub fn onClick(self: Node, callback: c.FluxUIClickCallback, user_data: ?*anyopaque) Node {
        var n = self;
        n.on_click = callback;
        n.on_click_data = user_data;
        return n;
    }

    pub fn mount(self: Node, parent: Widget) Error!Widget {
        var w: Widget = undefined;
        if (self.is_text_fn) {
            const initial = if (self.text_fn) |f| f(self.text_fn_data) else "";
            w = try parent.addElement("span", initial, self.class_name);
            if (self.text_fn) |f| registerReactive(w, f, self.text_fn_data, initial);
        } else {
            w = try parent.addElement(self.tag, self.content, self.class_name);
        }

        if (self.node_id[0] != 0) w.setId(self.node_id);
        if (self.inline_css[0] != 0) w.css(self.inline_css);
        if (self.on_click) |cb| w.setOnClick(cb, self.on_click_data);
        for (self.children) |child| {
            _ = try child.mount(w);
        }
        return w;
    }
};

// ---- HTML element builders (names match HTML/Blink exactly). ----
fn container(comptime tag: [*:0]const u8, children: []const Node) Node {
    return .{ .tag = tag, .children = children };
}
fn leaf(comptime tag: [*:0]const u8, content: [*:0]const u8) Node {
    return .{ .tag = tag, .content = content };
}

// Flow containers.
pub fn div(children: []const Node) Node { return container("div", children); }
pub fn section(children: []const Node) Node { return container("section", children); }
pub fn article(children: []const Node) Node { return container("article", children); }
pub fn aside(children: []const Node) Node { return container("aside", children); }
pub fn header(children: []const Node) Node { return container("header", children); }
pub fn footer(children: []const Node) Node { return container("footer", children); }
pub fn mainEl(children: []const Node) Node { return container("main", children); }
pub fn nav(children: []const Node) Node { return container("nav", children); }
pub fn form(children: []const Node) Node { return container("form", children); }
pub fn fieldset(children: []const Node) Node { return container("fieldset", children); }
pub fn blockquote(children: []const Node) Node { return container("blockquote", children); }
pub fn figure(children: []const Node) Node { return container("figure", children); }
pub fn ul(children: []const Node) Node { return container("ul", children); }
pub fn ol(children: []const Node) Node { return container("ol", children); }
pub fn li(children: []const Node) Node { return container("li", children); }
pub fn table(children: []const Node) Node { return container("table", children); }
pub fn tr(children: []const Node) Node { return container("tr", children); }

// Text content.
pub fn text(content: [*:0]const u8) Node { return leaf("span", content); }
pub fn span(content: [*:0]const u8) Node { return leaf("span", content); }
pub fn p(content: [*:0]const u8) Node { return leaf("p", content); }
pub fn h1(content: [*:0]const u8) Node { return leaf("h1", content); }
pub fn h2(content: [*:0]const u8) Node { return leaf("h2", content); }
pub fn h3(content: [*:0]const u8) Node { return leaf("h3", content); }
pub fn h4(content: [*:0]const u8) Node { return leaf("h4", content); }
pub fn h5(content: [*:0]const u8) Node { return leaf("h5", content); }
pub fn h6(content: [*:0]const u8) Node { return leaf("h6", content); }
pub fn strong(content: [*:0]const u8) Node { return leaf("strong", content); }
pub fn em(content: [*:0]const u8) Node { return leaf("em", content); }
pub fn small(content: [*:0]const u8) Node { return leaf("small", content); }
pub fn label(content: [*:0]const u8) Node { return leaf("label", content); }
pub fn legend(content: [*:0]const u8) Node { return leaf("legend", content); }
pub fn code(content: [*:0]const u8) Node { return leaf("code", content); }
pub fn pre(content: [*:0]const u8) Node { return leaf("pre", content); }
pub fn td(content: [*:0]const u8) Node { return leaf("td", content); }
pub fn th(content: [*:0]const u8) Node { return leaf("th", content); }

/// Reactive text — re-evaluated whenever bound State changes.
pub fn textFn(func: TextFnPtr, user_data: ?*anyopaque) Node {
    return .{ .tag = "span", .is_text_fn = true, .text_fn = func, .text_fn_data = user_data };
}

// Interactive controls.
pub fn button(lbl: [*:0]const u8) Node { return leaf("button", lbl); }
pub fn input(placeholder: [*:0]const u8) Node { return leaf("input", placeholder); }
pub fn textarea(placeholder: [*:0]const u8) Node { return leaf("textarea", placeholder); }
pub fn img(source: [*:0]const u8) Node { return leaf("img", source); }
pub fn checkbox() Node { return .{ .tag = "checkbox" }; }
pub fn radio() Node { return .{ .tag = "radio" }; }
pub fn hr() Node { return .{ .tag = "hr" }; }
pub fn br() Node { return .{ .tag = "br" }; }
pub fn select(options: []const Node) Node { return container("select", options); }
pub fn option(lbl: [*:0]const u8) Node { return leaf("option", lbl); }

/// Generic escape hatch for any HTML tag.
pub fn el(tag: [*:0]const u8, children: []const Node) Node {
    return .{ .tag = tag, .children = children };
}

// ---- Declarative routing (like Next.js) ----
pub const ViewFn = *const fn () Node;
pub const LayoutFn = *const fn (content: Node) Node;

const Route = struct { path: [*:0]const u8, view: ViewFn };

var g_routes: [64]Route = undefined;
var g_route_count: usize = 0;
var g_layout: ?LayoutFn = null;
var g_current: [*:0]const u8 = "";
var g_active_app: ?*c.FluxUIApp = null;

/// Navigate the active app to a route (usable from C-ABI onClick handlers where
/// you don't hold an App). Pairs with file-based routing.
pub fn navigate(path: [*:0]const u8) void {
    if (g_active_app) |raw| {
        const app = App{ .raw = raw };
        app.navigate(path);
    }
}

// Pending views collected at comptime/startup for file-based routing.
var g_pending: [64]Route = undefined;
var g_pending_count: usize = 0;

/// Register a view for file-based routing. Call from each view module's
/// `pub fn register()` (Zig has no auto-init), then `app.useViews()` wires them.
pub fn registerView(path: [*:0]const u8, view: ViewFn) void {
    if (g_pending_count >= g_pending.len) return;
    g_pending[g_pending_count] = .{ .path = path, .view = view };
    g_pending_count += 1;
}

fn routerUseViews() void {
    var i: usize = 0;
    while (i < g_pending_count) : (i += 1) {
        routerAddRoute(g_pending[i].path, g_pending[i].view);
    }
}

fn routerSetApp(raw: ?*c.FluxUIApp) void {
    g_active_app = raw;
}

fn routerAddRoute(path: [*:0]const u8, view: ViewFn) void {
    if (g_route_count >= g_routes.len) return;
    g_routes[g_route_count] = .{ .path = path, .view = view };
    g_route_count += 1;
}

fn routerSetLayout(layout: LayoutFn) void {
    g_layout = layout;
}

fn routerSetCurrent(path: [*:0]const u8) void {
    g_current = path;
}

fn routerCurrent() [*:0]const u8 {
    return g_current;
}

fn routerHasRoutes() bool {
    return g_route_count > 0;
}

fn findRoute(path: [*:0]const u8) ?ViewFn {
    var i: usize = 0;
    while (i < g_route_count) : (i += 1) {
        if (cstrEql(g_routes[i].path, path)) return g_routes[i].view;
    }
    return null;
}

fn routerBuild(rootWidget: Widget, initial_route: [*:0]const u8) void {
    if (initial_route[0] != 0) {
        g_current = initial_route;
    } else if (g_current[0] == 0 and g_route_count > 0) {
        g_current = g_routes[0].path;
    }

    var content: ?Node = null;
    if (findRoute(g_current)) |view| {
        content = view();
    }

    if (g_layout) |layout| {
        const shell = layout(content orelse Node{ .tag = "div" });
        _ = shell.mount(rootWidget) catch {};
    } else if (content) |node| {
        _ = node.mount(rootWidget) catch {};
    }
}

}; // pub const dsl
