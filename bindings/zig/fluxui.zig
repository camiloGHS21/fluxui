pub const c = @cImport({
    @cInclude("fluxui/fluxui_c.h");
});

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

    pub fn addCheckbox(self: Widget, checked: bool, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_checkbox(self.raw, if (checked) 1 else 0, class_name));
    }

    pub fn addRadio(self: Widget, checked: bool, group: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_radio(self.raw, if (checked) 1 else 0, group, class_name));
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

    pub fn setChecked(self: Widget, checked: bool) void {
        c.fluxui_checkbox_set_checked(self.raw, if (checked) 1 else 0);
        c.fluxui_radio_set_checked(self.raw, if (checked) 1 else 0);
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
