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

    pub fn userData(self: App) ?*anyopaque {
        return @ptrCast(self.raw);
    }
};

pub const Widget = struct {
    raw: ?*c.FluxUIWidget,

    pub fn clearChildren(self: Widget) void {
        c.fluxui_widget_clear_children(self.raw);
    }

    pub fn addPanel(self: Widget, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_panel(self.raw, class_name));
    }

    pub fn addText(self: Widget, text: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_text(self.raw, text, class_name));
    }

    pub fn addButton(self: Widget, label: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_button(self.raw, label, class_name));
    }

    pub fn addTextInput(self: Widget, placeholder: [*:0]const u8, class_name: [*:0]const u8) Error!Widget {
        return fromRaw(c.fluxui_widget_add_text_input(self.raw, placeholder, class_name));
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
